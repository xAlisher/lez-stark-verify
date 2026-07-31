// Private-eligibility demo host — proves the ZK verifier works, four ways:
//   [1] a valid witness proves + the receipt VERIFIES,
//   [2] the secret is NOT in the journal (zero-knowledge of the witness),
//   [3] a tampered receipt is REJECTED,
//   [4] an ineligible witness CANNOT produce a valid proof (soundness).
//
// Run REAL (no dev mode):  unset RISC0_DEV_MODE; cargo run --release
use anyhow::Result;
use methods::{METHOD_ELF, METHOD_ID};
use risc0_zkvm::{default_prover, ExecutorEnv, Receipt};

fn prove(value: u64, threshold: u64) -> Result<Receipt> {
    let env = ExecutorEnv::builder()
        .write(&value)?
        .write(&threshold)?
        .build()?;
    Ok(default_prover().prove(env, METHOD_ELF)?.receipt)
}

fn main() -> Result<()> {
    tracing_subscriber::fmt()
        .with_env_filter(tracing_subscriber::filter::EnvFilter::from_default_env())
        .init();

    let secret: u64 = 42_000; // PRIVATE — a balance / score / age
    let threshold: u64 = 10_000; // PUBLIC threshold to clear

    let dev = std::env::var("RISC0_DEV_MODE").is_ok();
    println!("== Private eligibility (RISC0 STARK) — prove secret >= {threshold}, hide the secret ==");
    println!("   mode: {}", if dev { "DEV (fake proof)" } else { "REAL proof" });

    // [1] eligible witness → prove + verify
    let receipt = prove(secret, threshold)?;
    receipt.verify(METHOD_ID)?;
    let (t, eligible): (u64, bool) = receipt.journal.decode()?;
    println!("[1] verify OK · journal = (threshold={t}, eligible={eligible})");
    assert!(t == threshold && eligible);

    // [2] privacy: the secret must NOT appear in the public journal
    let leaks = receipt.journal.bytes.windows(8).any(|w| w == secret.to_le_bytes());
    println!("[2] secret present in journal? {leaks}  (want: false)");
    assert!(!leaks, "secret leaked into the journal");

    // [3] tamper: flip a journal byte → verification MUST fail
    let mut bad = receipt.clone();
    bad.journal.bytes[0] ^= 0xFF;
    let tampered_ok = bad.verify(METHOD_ID).is_ok();
    println!("[3] tampered receipt verifies? {tampered_ok}  (want: false)");
    assert!(!tampered_ok, "a tampered receipt must be rejected");

    // [4] soundness: an ineligible witness cannot produce a valid proof
    let ineligible = prove(threshold - 1, threshold);
    println!("[4] proof for ineligible witness produced? {}  (want: false)", ineligible.is_ok());
    assert!(ineligible.is_err(), "an ineligible witness must not be provable");

    println!("\nALL 4 CHECKS PASSED — the ZK eligibility module works.");
    Ok(())
}
