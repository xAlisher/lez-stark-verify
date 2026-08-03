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
}
