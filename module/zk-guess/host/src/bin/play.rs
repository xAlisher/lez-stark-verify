// zk-guess M2 — headless game loop (issue #13).
//
// Seals a secret, then plays a full game against the STARK oracle: each turn a
// guess is PROVED (above/below/equal) and VERIFIED before the range narrows.
// No UI — this is the mechanic, proven end to end. A win reveals the secret and
// re-checks it against the seal.
//
// The "player" is an automated binary-searcher; the point is that *every* answer
// is a verified STARK — the house cannot cheat and the number cannot move.
use anyhow::{bail, Result};
use host::{commit, dir_str, prove_turn};
use methods::GUESS_ID;
use std::time::Instant;

const RANGE_MAX: u64 = 1_000_000;

fn main() -> Result<()> {
    let (secret, blind) = (573_118u64, 0x00C0_FFEEu64); // sealed; never revealed until a win
    let c = commit(secret, blind);
    println!("── zk-guess · headless game ──");
    println!("sealed a number in 0..={RANGE_MAX}");
    println!("commitment = {}\n", hex::encode(c));

    let (mut lo, mut hi) = (0u64, RANGE_MAX);
    let mut turn = 0u32;
    let t_game = Instant::now();

    loop {
        turn += 1;
        if turn > 40 {
            bail!("did not converge in 40 turns (unexpected)");
        }
        let guess = lo + (hi - lo) / 2;

        let t = Instant::now();
        let r = prove_turn(secret, blind, guess, c)?; // PROVE
        if r.verify(GUESS_ID).is_err() {
            bail!("turn {turn}: receipt failed to verify");
        }
        let (jc, jg, dir): ([u8; 32], u64, u8) = r.journal.decode()?; // VERIFY + read verdict
        if jc != c || jg != guess {
            bail!("turn {turn}: journal/commitment mismatch");
        }
        let ms = t.elapsed().as_millis();

        println!(
            "turn {turn:>2} · guess {guess:>7} · {:<5} ✓verified {ms:>5}ms · range {lo}..={hi}",
            dir_str(dir)
        );

        match dir {
            1 => {
                // EQUAL — win
                println!("\n★ EXACT after {turn} turns in {:.1}s", t_game.elapsed().as_secs_f64());
                println!("reveal: secret = {secret}");
                if commit(secret, blind) == c {
                    println!("✓ revealed secret matches the seal — game was honest end to end.");
                } else {
                    bail!("reveal FAILED: secret does not match the seal");
                }
                break;
            }
            0 => lo = guess + 1, // guess BELOW secret -> go higher
            2 => hi = guess - 1, // guess ABOVE secret -> go lower
            _ => bail!("turn {turn}: bad direction"),
        }
    }
    Ok(())
}
