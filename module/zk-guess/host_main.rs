// zk-guess M1 host — prove a turn + the 4-ways harness.  (DRAFT — not yet built.)
//
// Rename the generated image constants to GUESS_ELF / GUESS_ID when scaffolding.
use anyhow::{bail, Result};
use methods::{GUESS_ELF, GUESS_ID};
use risc0_zkvm::{default_prover, ExecutorEnv, Receipt};
use sha2::{Digest, Sha256};

/// The commitment the HOST publishes at seal time. Must equal the guest's
/// `risc0_zkvm::sha::Impl` digest of the same preimage (SHA-256 parity).
fn commit(secret: u64, blind: u64) -> [u8; 32] {
    let mut h = Sha256::new();
    h.update(secret.to_le_bytes());
    h.update(blind.to_le_bytes());
    h.finalize().into()
}

fn prove_turn(secret: u64, blind: u64, guess: u64, commitment: [u8; 32]) -> Result<Receipt> {
    let env = ExecutorEnv::builder()
        .write(&secret)?
        .write(&blind)?
        .write(&guess)?
        .write(&commitment)?
        .build()?;
    Ok(default_prover().prove(env, GUESS_ELF)?.receipt)
}

fn dir_str(d: u8) -> &'static str {
    match d {
        0 => "BELOW",
        1 => "EQUAL",
        2 => "ABOVE",
        _ => "?",
    }
}

fn main() -> Result<()> {
    let (secret, blind) = (573_118u64, 0xC0FFEEu64);
    let c = commit(secret, blind);
    println!("sealed. commitment = {}", hex::encode(c));

    // (1) a valid turn verifies, and the journal carries only (commitment, guess, dir)
    let guess = 600_000u64; // above the secret
    let r = prove_turn(secret, blind, guess, c)?;
    if r.verify(GUESS_ID).is_err() {
        bail!("check 1 FAILED: valid receipt did not verify");
    }
    let (jc, jg, jd): ([u8; 32], u64, u8) = r.journal.decode()?;
    println!("check 1 ✓ verified · journal=(commit,{jg},{}) ", dir_str(jd));
    if jg != guess || jd != 2 || jc != c {
        bail!("check 1 FAILED: journal mismatch");
    }

    // (2) the secret is absent from the journal (journal is exactly 32+8+1 bytes)
    if r.journal.bytes.windows(8).any(|w| w == secret.to_le_bytes()) {
        bail!("check 2 FAILED: secret leaked into the journal");
    }
    println!("check 2 ✓ secret absent from journal");

    // (3) a tampered receipt is rejected
    let mut t = r.clone();
    t.journal.bytes[0] ^= 0xFF;
    if t.verify(GUESS_ID).is_ok() {
        bail!("check 3 FAILED: tampered receipt verified");
    }
    println!("check 3 ✓ tampered receipt rejected");

    // (4) a swapped secret is unprovable: prove with a DIFFERENT secret against the
    //     original commitment → the guest's commitment-open assert halts → no receipt
    match prove_turn(secret + 1, blind, guess, c) {
        Ok(_) => bail!("check 4 FAILED: swapped secret produced a proof"),
        Err(_) => println!("check 4 ✓ swapped secret unprovable"),
    }

    println!("\nALL 4 CHECKS PASSED — real STARK, secret never revealed.");
    Ok(())
}
