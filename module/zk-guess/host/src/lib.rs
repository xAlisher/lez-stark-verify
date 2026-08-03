// zk-guess host lib — shared seal / prove / verify helpers for the game.
use anyhow::Result;
use methods::GUESS_ELF;
use risc0_zkvm::{default_prover, ExecutorEnv, Receipt};
use sha2::{Digest, Sha256};

/// Seal commitment the host publishes: SHA256(secret_le ‖ blind_le).
/// Matches the guest digest by construction (both use sha2).
pub fn commit(secret: u64, blind: u64) -> [u8; 32] {
    let mut h = Sha256::new();
    h.update(secret.to_le_bytes());
    h.update(blind.to_le_bytes());
    h.finalize().into()
}

/// Prove one turn: the honest direction of `guess` vs the sealed secret, bound
/// to `commitment`. Errors if (secret,blind) don't open the commitment (the guest
/// halts) — i.e. a swapped number is unprovable.
pub fn prove_turn(secret: u64, blind: u64, guess: u64, commitment: [u8; 32]) -> Result<Receipt> {
    let env = ExecutorEnv::builder()
        .write(&secret)?
        .write(&blind)?
        .write(&guess)?
        .write(&commitment)?
        .build()?;
    Ok(default_prover().prove(env, GUESS_ELF)?.receipt)
}

pub fn dir_str(d: u8) -> &'static str {
    match d {
        0 => "BELOW",
        1 => "EQUAL",
        2 => "ABOVE",
        _ => "?",
    }
}
