// zk-guess M1 host — the 4-ways harness (issue #11).
use anyhow::{bail, Result};
use host::{commit, dir_str, prove_turn};
use methods::GUESS_ID;

fn main() -> Result<()> {
    let (secret, blind) = (573_118u64, 0x00C0_FFEEu64);
    let c = commit(secret, blind);
    println!("sealed.  commitment = {}", hex::encode(c));

    // (1) a valid turn verifies; journal carries only (commitment, guess, dir)
    let guess = 600_000u64; // above the secret -> dir ABOVE (2)
    let r = prove_turn(secret, blind, guess, c)?;
    if r.verify(GUESS_ID).is_err() {
        bail!("check 1 FAILED: valid receipt did not verify");
    }
    let (jc, jg, jd): ([u8; 32], u64, u8) = r.journal.decode()?;
    println!("check 1 ✓ verified · journal = (commit, {jg}, {})", dir_str(jd));
    if jg != guess || jd != 2 || jc != c {
        bail!("check 1 FAILED: journal mismatch (jg={jg}, jd={jd})");
    }

    // (2) the secret never appears in the journal
    let sb = secret.to_le_bytes();
    if r.journal.bytes.windows(8).any(|w| w == &sb[..]) {
        bail!("check 2 FAILED: secret leaked into the journal");
    }
    println!("check 2 ✓ secret absent from journal ({} bytes)", r.journal.bytes.len());

    // (3) a tampered receipt is rejected
    let mut t = r.clone();
    t.journal.bytes[0] ^= 0xFF;
    if t.verify(GUESS_ID).is_ok() {
        bail!("check 3 FAILED: tampered receipt verified");
    }
    println!("check 3 ✓ tampered receipt rejected");

    // (4) a swapped secret is unprovable: prove a DIFFERENT secret against the
    //     original commitment -> the guest's commitment-open assert halts -> no receipt.
    //     (the guest's `commitment mismatch` panic on stderr is this check working.)
    match prove_turn(secret + 1, blind, guess, c) {
        Ok(_) => bail!("check 4 FAILED: swapped secret produced a proof"),
        Err(_) => println!("check 4 ✓ swapped secret unprovable"),
    }

    println!("\nALL 4 CHECKS PASSED — real STARK, secret never revealed.");
    Ok(())
}
