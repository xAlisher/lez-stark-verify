// Private-eligibility zkVM guest.
//
// Proves a SECRET value clears a PUBLIC threshold, committing only the public
// claim (threshold, eligible) to the journal — the secret value is read as a
// private witness and is never revealed.
use risc0_zkvm::guest::env;

fn main() {
    let value: u64 = env::read(); // PRIVATE witness — never committed
    let threshold: u64 = env::read(); // public threshold

    // Soundness: an ineligible witness halts the guest, so no valid eligibility
    // proof can exist for value < threshold.
    assert!(value >= threshold, "witness below threshold — not eligible");

    // Journal = only the public claim. Zero-knowledge of `value`.
    env::commit(&(threshold, true));
}
