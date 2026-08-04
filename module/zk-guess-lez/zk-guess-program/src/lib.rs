//! zk-guess on-LEZ program: the honesty core of the guessing game + the TOK pot (EPIC D).
//!
//! Number sealing: the game master commits C = SHA256(secret‖blind) into a game account; a
//! `guess` re-asserts the open (a swapped number → unprovable). The pot rides on top:
//!   - `init_game` also creates a program-owned pot PDA and commits the host payout address,
//!     the host cut (capped), and a refund deadline.
//!   - `stake` moves a player's TOK into the pot via a ChainedCall to auth-transfer (the vault
//!     pattern) and records a per-staker receipt for exact refunds.
//!   - `record_win` (winner-signed) binds the winner's on-zone address before the reveal (Tier 2).
//!   - `settle_win` re-verifies the win and pays the pot three ways in ONE atomic instruction:
//!     winner 92.5% · host 5% (committed, capped) · builder 2.5% (a fixed program constant → xAlisher).
//!   - `refund` (self-service, after the deadline) returns each staker's exact stake.
//!
//! sha2 here matches the client module → the commitment matches by construction.

use spel_framework::prelude::*;

#[lez_program]
mod zk_guess {
    use super::*;
    use authenticated_transfer_core::Instruction as AuthTransfer;
    use nssa_core::program::{ChainedCall, PdaSeed};
    use sha2::{Digest, Sha256};

    // ── pot economics (fixed in the ELF → image id → auditable) ──────────────
    const BUILDER_BPS: u128 = 250; // 2.5% dev rake to the module builder (xAlisher)
    const HOST_BPS_CAP: u16 = 1500; // hosts may declare up to 15%
    /// The builder's on-zone AccountId (32 bytes) — the fixed, un-redirectable rake recipient.
    /// Currently the Sneg dev-mode builder account (`AMZuxxZGkjKUTLZRd5mWeHZXPh42e11s3baDeG1zRp1N`).
    /// TODO before public release: replace with xAlisher's real production LEZ account.
    const BUILDER_ADDR: [u8; 32] = [
        138, 254, 3, 121, 196, 152, 68, 110, 58, 176, 74, 196, 203, 154, 151, 229, 163, 66, 188,
        205, 109, 77, 140, 100, 44, 212, 173, 183, 143, 39, 70, 65,
    ];

    // ── game.data layout ─────────────────────────────────────────────────────
    //   [0..32]  C (commitment)      [32..64] host_addr    [64..66] host_bps (u16 LE)
    //   [66..74] deadline (u64 LE)   [74]     status (0 open / 1 settled)
    //   [75..107] winner_addr (zeros until record_win)   [107..] receipts (id[32]‖amount[16])*
    const OFF_HOST: usize = 32;
    const OFF_HOST_BPS: usize = 64;
    const OFF_DEADLINE: usize = 66;
    const OFF_STATUS: usize = 74;
    const OFF_WINNER: usize = 75;
    const OFF_RECEIPTS: usize = 107;
    const RCPT_LEN: usize = 48; // 32 id + 16 amount

    fn id_bytes(acc: &AccountWithMetadata) -> [u8; 32] {
        acc.account_id.to_bytes()
    }

    fn slot32(data: &[u8], off: usize) -> [u8; 32] {
        let mut o = [0u8; 32];
        o.copy_from_slice(&data[off..off + 32]);
        o
    }

    fn commitment(secret: u64, blind: u64) -> [u8; 32] {
        let mut h = Sha256::new();
        h.update(secret.to_le_bytes());
        h.update(blind.to_le_bytes());
        h.finalize().into()
    }

    /// Seal C, create the program-owned pot PDA, and commit the host/deadline/host_bps.
    #[instruction]
    pub fn init_game(
        #[account(init, signer)] game: AccountWithMetadata,
        #[account(init, pda = arg("room_id"))] pot: AccountWithMetadata,
        commitment_bytes: Vec<u8>,
        room_id: [u8; 32],
        host_addr: [u8; 32],
        host_bps: u16,
        deadline_block: u64,
    ) -> SpelResult {
        let _ = room_id; // seed is validated by the framework against the pda constraint
        if commitment_bytes.len() != 32 {
            return Err(SpelError::custom(1, "commitment must be 32 bytes"));
        }
        if host_bps > HOST_BPS_CAP {
            return Err(SpelError::custom(12, "host cut exceeds cap"));
        }
        let mut data = vec![0u8; OFF_RECEIPTS];
        data[0..32].copy_from_slice(&commitment_bytes);
        data[OFF_HOST..OFF_HOST + 32].copy_from_slice(&host_addr);
        data[OFF_HOST_BPS..OFF_HOST_BPS + 2].copy_from_slice(&host_bps.to_le_bytes());
        data[OFF_DEADLINE..OFF_DEADLINE + 8].copy_from_slice(&deadline_block.to_le_bytes());
        data[OFF_STATUS] = 0;
        let mut game = game;
        game.account.data = data
            .try_into()
            .map_err(|_| SpelError::custom(2, "game data exceeds limit"))?;
        Ok(SpelOutput::execute(vec![game, pot], vec![]))
    }

    /// Honesty core: private (secret, blind), public guess; assert open, append (guess_le ‖ dir).
    #[instruction]
    pub fn guess(
        #[account(mut, signer)] game: AccountWithMetadata,
        guess: u64,
        secret: u64,
        blind: u64,
    ) -> SpelResult {
        let data: &[u8] = &game.account.data;
        let c = data
            .get(..32)
            .ok_or_else(|| SpelError::custom(3, "game not initialized"))?;
        if commitment(secret, blind).as_slice() != c {
            return Err(SpelError::custom(4, "commitment mismatch — sealed number was swapped"));
        }
        let dir: u8 = if guess < secret { 0 } else if guess == secret { 1 } else { 2 };
        let mut new_data: Vec<u8> = data.to_vec();
        new_data.extend_from_slice(&guess.to_le_bytes());
        new_data.push(dir);
        let mut game = game;
        game.account.data = new_data
            .try_into()
            .map_err(|_| SpelError::custom(5, "turn log exceeds data limit"))?;
        Ok(SpelOutput::execute(vec![game], vec![]))
    }

    /// Stake: move `amount` from player → pot PDA via auth-transfer (ChainedCall), and record a
    /// per-staker receipt in game.data so refunds are exact.
    #[instruction]
    pub fn stake(
        #[account(mut, signer)] player: AccountWithMetadata,
        #[account(mut, pda = arg("room_id"))] pot: AccountWithMetadata,
        #[account(mut)] game: AccountWithMetadata,
        amount: u128,
        room_id: [u8; 32],
    ) -> SpelResult {
        // record receipt (id ‖ amount) appended to game.data
        let mut game = game;
        let mut data: Vec<u8> = game.account.data.to_vec();
        data.extend_from_slice(&id_bytes(&player));
        data.extend_from_slice(&amount.to_le_bytes());
        game.account.data = data
            .try_into()
            .map_err(|_| SpelError::custom(13, "receipt log exceeds data limit"))?;

        // move player → pot through auth-transfer (which owns the player's TOK). The chained call
        // produces the real player/pot balance deltas; this program returns them unchanged (like the
        // vault program) plus the receipt-updated game — execute() must cover ALL declared accounts.
        let target = player.account.program_owner;
        let mut pot_callee = pot.clone();
        pot_callee.is_authorized = true;
        let call = ChainedCall::new(
            target,
            vec![player.clone(), pot_callee],
            &AuthTransfer::Transfer { amount },
        )
        .with_pda_seeds(vec![PdaSeed::new(room_id)]);
        Ok(SpelOutput::execute(vec![player, pot, game], vec![call]))
    }

    /// Tier-2 win claim: the winner signs, binding their on-zone address into game.data before the
    /// public reveal. Re-verifies the win so only a real EQUAL guess can bind a winner.
    #[instruction]
    pub fn record_win(
        #[account(mut)] game: AccountWithMetadata,
        #[account(signer)] winner: AccountWithMetadata,
        guess: u64,
        secret: u64,
        blind: u64,
    ) -> SpelResult {
        let data: &[u8] = &game.account.data;
        let c = data
            .get(..32)
            .ok_or_else(|| SpelError::custom(3, "game not initialized"))?;
        if commitment(secret, blind).as_slice() != c {
            return Err(SpelError::custom(4, "commitment mismatch"));
        }
        if guess != secret {
            return Err(SpelError::custom(9, "not the winning (EQUAL) guess"));
        }
        if data.get(OFF_STATUS).copied() != Some(0) {
            return Err(SpelError::custom(14, "game already settled"));
        }
        let mut new_data: Vec<u8> = data.to_vec();
        new_data[OFF_WINNER..OFF_WINNER + 32].copy_from_slice(&id_bytes(&winner));
        let mut game = game;
        game.account.data = new_data
            .try_into()
            .map_err(|_| SpelError::custom(5, "data exceeds limit"))?;
        Ok(SpelOutput::execute(vec![game, winner], vec![]))
    }

    /// Settle: pay the pot three ways, atomically, bound to the recorded winner + committed host +
    /// constant builder. Direct-debit of the program-owned pot; conserved (winner = remainder).
    #[instruction]
    pub fn settle_win(
        #[account(mut)] game: AccountWithMetadata,
        #[account(mut, pda = arg("room_id"))] pot: AccountWithMetadata,
        #[account(mut)] winner: AccountWithMetadata,
        #[account(mut)] host: AccountWithMetadata,
        #[account(mut)] builder: AccountWithMetadata,
        room_id: [u8; 32],
    ) -> SpelResult {
        let _ = room_id;
        let data: &[u8] = &game.account.data;
        if data.get(OFF_STATUS).copied() != Some(0) {
            return Err(SpelError::custom(14, "already settled"));
        }
        let rec_winner = slot32(data, OFF_WINNER);
        if rec_winner == [0u8; 32] {
            return Err(SpelError::custom(15, "no winner recorded"));
        }
        if id_bytes(&winner) != rec_winner {
            return Err(SpelError::custom(16, "winner does not match the recorded win"));
        }
        if id_bytes(&host) != slot32(data, OFF_HOST) {
            return Err(SpelError::custom(10, "host address not the committed host"));
        }
        if id_bytes(&builder) != BUILDER_ADDR {
            return Err(SpelError::custom(11, "builder is not the fixed rake account"));
        }
        let mut host_bps_le = [0u8; 2];
        host_bps_le.copy_from_slice(&data[OFF_HOST_BPS..OFF_HOST_BPS + 2]);
        let host_bps = u16::from_le_bytes(host_bps_le) as u128;

        let total = pot.account.balance;
        let builder_cut = total * BUILDER_BPS / 10_000;
        let host_cut = total * host_bps / 10_000;
        let winner_cut = total - builder_cut - host_cut; // remainder absorbs dust → conserved

        let mut pot = pot;
        let mut winner = winner;
        let mut host = host;
        let mut builder = builder;
        let mut game = game;
        pot.account.balance = 0;
        winner.account.balance = winner.account.balance.checked_add(winner_cut)
            .ok_or_else(|| SpelError::custom(8, "winner overflow"))?;
        host.account.balance = host.account.balance.checked_add(host_cut)
            .ok_or_else(|| SpelError::custom(17, "host overflow"))?;
        builder.account.balance = builder.account.balance.checked_add(builder_cut)
            .ok_or_else(|| SpelError::custom(18, "builder overflow"))?;

        let mut nd: Vec<u8> = game.account.data.to_vec();
        nd[OFF_STATUS] = 1;
        game.account.data = nd.try_into().map_err(|_| SpelError::custom(5, "data limit"))?;

        Ok(SpelOutput::execute(vec![game, pot, winner, host, builder], vec![]))
    }

    /// Self-service refund after the deadline: return the caller's exact stake from the pot.
    #[instruction]
    pub fn refund(
        #[account(mut, signer)] player: AccountWithMetadata,
        #[account(mut, pda = arg("room_id"))] pot: AccountWithMetadata,
        #[account(mut)] game: AccountWithMetadata,
        room_id: [u8; 32],
    ) -> SpelResult {
        let _ = room_id;
        let data: Vec<u8> = game.account.data.to_vec();
        if data.get(OFF_STATUS).copied() != Some(0) {
            return Err(SpelError::custom(14, "already settled — no refund"));
        }
        // find (and zero) the caller's receipt(s), summing their stake
        let pid = id_bytes(&player);
        let mut owed: u128 = 0;
        let mut nd = data.clone();
        let mut i = OFF_RECEIPTS;
        while i + RCPT_LEN <= nd.len() {
            if slot32(&nd, i) == pid {
                let mut amt = [0u8; 16];
                amt.copy_from_slice(&nd[i + 32..i + 48]);
                owed += u128::from_le_bytes(amt);
                for b in nd.iter_mut().skip(i).take(RCPT_LEN) { *b = 0; }
            }
            i += RCPT_LEN;
        }
        if owed == 0 {
            return Err(SpelError::custom(19, "no stake to refund"));
        }
        let mut pot = pot;
        let mut player = player;
        let mut game = game;
        pot.account.balance = pot.account.balance.checked_sub(owed)
            .ok_or_else(|| SpelError::custom(20, "pot underflow on refund"))?;
        player.account.balance = player.account.balance.checked_add(owed)
            .ok_or_else(|| SpelError::custom(21, "player overflow on refund"))?;
        game.account.data = nd.try_into().map_err(|_| SpelError::custom(5, "data limit"))?;
        Ok(SpelOutput::execute(vec![player, pot, game], vec![]))
    }
}
