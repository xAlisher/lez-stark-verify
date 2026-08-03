//! zk-guess on-LEZ program: the honesty core of the guessing game as a #[lez_program].
//!
//! The game master seals a number by committing C = SHA256(secret‖blind) into a
//! game account. Each turn is a **privacy-preserving** tx: private (secret, blind)
//! witnesses + public guess; the guest asserts commitment-open (a swapped number
//! fails → unprovable, exactly like the referral recipient binding), computes the
//! direction, and appends (guess, dir) to the game account's data. The sequencer
//! accepts the turn only if the STARK receipt verifies — "verified on LEZ" made literal.
//!
//! sha2 in both guest (here) and the client module → the commitment matches by construction.

use spel_framework::prelude::*;

#[lez_program]
mod zk_guess {
    use super::*;
    use sha2::{Digest, Sha256};

    /// Seal: commit C = SHA256(secret‖blind) into the game account's data.
    #[instruction]
    pub fn init_game(
        #[account(init, signer)] game: AccountWithMetadata,
        commitment: Vec<u8>,
    ) -> SpelResult {
        if commitment.len() != 32 {
            return Err(SpelError::custom(1, "commitment must be 32 bytes"));
        }
        let mut game = game;
        game.account.data = commitment
            .try_into()
            .map_err(|_| SpelError::custom(2, "commitment exceeds data limit"))?;
        Ok(SpelOutput::execute(vec![game], vec![]))
    }

    /// Turn: private (secret, blind), public guess. Assert commitment-open, compute
    /// dir (0 below / 1 equal / 2 above), append (guess_le ‖ dir) to game.data.
    /// secret/blind are witnesses only — never written to state → stay private.
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
            .ok_or_else(|| SpelError::custom(3, "game not initialized (no commitment)"))?;

        let mut h = Sha256::new();
        h.update(secret.to_le_bytes());
        h.update(blind.to_le_bytes());
        let digest: [u8; 32] = h.finalize().into();
        if digest.as_slice() != c {
            return Err(SpelError::custom(4, "commitment mismatch — sealed number was swapped"));
        }

        let dir: u8 = if guess < secret {
            0
        } else if guess == secret {
            1
        } else {
            2
        };

        let mut new_data: Vec<u8> = data.to_vec();
        new_data.extend_from_slice(&guess.to_le_bytes());
        new_data.push(dir);

        let mut game = game;
        game.account.data = new_data
            .try_into()
            .map_err(|_| SpelError::custom(5, "turn log exceeds data limit"))?;
        Ok(SpelOutput::execute(vec![game], vec![]))
    }

    /// Stake: move `amount` from a player into the program-custodied pot account.
    /// (Same balance-move primitive as referral emit_credit.) Trustless: the pot
    /// balance can only leave via `settle`.
    #[instruction]
    pub fn stake(
        #[account(mut, signer)] player: AccountWithMetadata,
        #[account(mut)] pot: AccountWithMetadata,
        amount: u128,
    ) -> SpelResult {
        let mut player = player;
        let mut pot = pot;
        player.account.balance = player
            .account
            .balance
            .checked_sub(amount)
            .ok_or_else(|| SpelError::custom(6, "insufficient balance to stake"))?;
        pot.account.balance = pot
            .account
            .balance
            .checked_add(amount)
            .ok_or_else(|| SpelError::custom(7, "pot overflow"))?;
        Ok(SpelOutput::execute(vec![player, pot], vec![]))
    }

    /// Settle: pay the whole pot to the winner. (First cut: called on a verified
    /// win; binding settle to the winning-guess proof is a follow-on.)
    #[instruction]
    pub fn settle(
        #[account(mut)] pot: AccountWithMetadata,
        #[account(mut, signer)] winner: AccountWithMetadata,
    ) -> SpelResult {
        let mut pot = pot;
        let mut winner = winner;
        let prize = pot.account.balance;
        pot.account.balance = 0;
        winner.account.balance = winner
            .account
            .balance
            .checked_add(prize)
            .ok_or_else(|| SpelError::custom(8, "winner balance overflow"))?;
        Ok(SpelOutput::execute(vec![pot, winner], vec![]))
    }
}
