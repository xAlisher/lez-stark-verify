// zk-verify (zk-guess) — the CLI the Basecamp module backend drives (issue #12).
//
//   zk-verify seal <secret> <blind>                       -> print commitment hex
//   zk-verify prove-turn <secret> <blind> <guess> [out]   -> prove one turn, write receipt
//   zk-verify verify <receipt>                            -> JSON {valid, commitment, guess, dir, dir_str}
//   zk-verify gen <dir>                                   -> write valid.receipt + tampered.receipt fixtures
//
// `verify` is pure (in-process STARK check, no r0vm/prover) -> small + bundle-able in the
// module .lgx and runs on-node in ms. `prove-turn`/`gen` need the prover (host-side).
use anyhow::{bail, Context, Result};
use host::{commit, dir_str, prove_turn};
use methods::GUESS_ID;
use risc0_zkvm::Receipt;
use std::{
    env, fs, thread,
    time::{Duration, SystemTime, UNIX_EPOCH},
};

fn parse<T>(s: Option<&String>, what: &str) -> Result<T>
where
    T: std::str::FromStr,
    T::Err: std::fmt::Display,
{
    s.with_context(|| format!("missing {what}"))?
        .parse()
        .map_err(|e| anyhow::anyhow!("bad {what}: {e}"))
}

fn main() -> Result<()> {
    let a: Vec<String> = env::args().collect();
    match a.get(1).map(String::as_str) {
        Some("seal") => {
            let secret: u64 = parse(a.get(2), "secret")?;
            let blind: u64 = parse(a.get(3), "blind")?;
            println!("{}", hex::encode(commit(secret, blind)));
        }
        Some("prove-turn") => {
            let secret: u64 = parse(a.get(2), "secret")?;
            let blind: u64 = parse(a.get(3), "blind")?;
            let guess: u64 = parse(a.get(4), "guess")?;
            let out = a.get(5).cloned().unwrap_or_else(|| "turn.receipt".into());
            let c = commit(secret, blind);
            let r = prove_turn(secret, blind, guess, c)?;
            fs::write(&out, bincode::serialize(&r)?)?;
            eprintln!("wrote {out} (commitment {})", hex::encode(c));
        }
        Some("verify") => {
            let path = a.get(2).context("usage: zk-verify verify <receipt>")?;
            let r: Receipt = bincode::deserialize(&fs::read(path)?)?;
            let valid = r.verify(GUESS_ID).is_ok();
            let (mut commitment, mut guess, mut dir) = (String::new(), 0u64, 255u8);
            if valid {
                if let Ok((c, g, d)) = r.journal.decode::<([u8; 32], u64, u8)>() {
                    commitment = hex::encode(c);
                    guess = g;
                    dir = d;
                }
            }
            println!(
                "{}",
                serde_json::json!({
                    "valid": valid,
                    "commitment": commitment,
                    "guess": guess,
                    "dir": dir,
                    "dir_str": dir_str(dir),
                })
            );
        }
        Some("gen") => {
            let dir = a.get(2).cloned().unwrap_or_else(|| ".".into());
            fs::create_dir_all(&dir)?;
            let (secret, blind) = (573_118u64, 0x00C0_FFEEu64);
            let c = commit(secret, blind);
            let r = prove_turn(secret, blind, 600_000, c)?; // a real ABOVE turn
            fs::write(format!("{dir}/valid.receipt"), bincode::serialize(&r)?)?;
            let mut t = r.clone();
            t.journal.bytes[0] ^= 0xFF; // corrupt the journal -> must fail verify
            fs::write(format!("{dir}/tampered.receipt"), bincode::serialize(&t)?)?;
            eprintln!("wrote valid.receipt + tampered.receipt to {dir}");
        }
        // ── live game: the HOST holds the secret and proves each turn ──
        // zk-verify host <dir> <secret> <blind>
        //   seals the number, writes <dir>/commitment, then serves turn requests:
        //   for each <dir>/req/<id>.guess it proves and writes <dir>/rcpt/<id>.receipt.
        Some("host") => {
            let dir = a.get(2).context("usage: host <dir> <secret> <blind>")?.clone();
            let secret: u64 = parse(a.get(3), "secret")?;
            let blind: u64 = parse(a.get(4), "blind")?;
            let c = commit(secret, blind);
            fs::create_dir_all(format!("{dir}/req"))?;
            fs::create_dir_all(format!("{dir}/rcpt"))?;
            fs::write(format!("{dir}/commitment"), hex::encode(c))?;
            eprintln!("host: sealed. commitment={} · serving {dir}/req", hex::encode(c));
            loop {
                if let Ok(rd) = fs::read_dir(format!("{dir}/req")) {
                    for e in rd.flatten() {
                        let p = e.path();
                        let id = match p.file_stem().and_then(|s| s.to_str()) {
                            Some(s) => s.to_string(),
                            None => continue,
                        };
                        let guess: u64 = match fs::read_to_string(&p).ok().and_then(|s| s.trim().parse().ok()) {
                            Some(g) => g,
                            None => { let _ = fs::remove_file(&p); continue }
                        };
                        let _ = fs::remove_file(&p); // claim it
                        match prove_turn(secret, blind, guess, c) {
                            Ok(r) => {
                                let tmp = format!("{dir}/rcpt/.{id}.tmp");
                                let _ = fs::write(&tmp, bincode::serialize(&r).unwrap_or_default());
                                let _ = fs::rename(&tmp, format!("{dir}/rcpt/{id}.receipt")); // atomic
                                eprintln!("host: turn {id} guess={guess} proved");
                            }
                            Err(e) => eprintln!("host: turn {id} prove failed: {e}"),
                        }
                    }
                }
                thread::sleep(Duration::from_millis(250));
            }
        }
        // zk-verify turn <dir> <guess>
        //   submit a guess to the host, wait for the receipt, VERIFY it client-side
        //   (no secret here), print JSON {valid,commitment,guess,dir,dir_str}.
        Some("turn") => {
            let dir = a.get(2).context("usage: turn <dir> <guess>")?.clone();
            let guess: u64 = parse(a.get(3), "guess")?;
            let nanos = SystemTime::now().duration_since(UNIX_EPOCH)?.as_nanos();
            let id = format!("{}-{}", std::process::id(), nanos);
            fs::create_dir_all(format!("{dir}/req"))?;
            let tmp = format!("{dir}/req/.{id}.tmp");
            fs::write(&tmp, guess.to_string())?;
            fs::rename(&tmp, format!("{dir}/req/{id}.guess"))?; // atomic submit
            let rcpt = format!("{dir}/rcpt/{id}.receipt");
            // poll up to ~30s for the host to prove
            for _ in 0..150 {
                if fs::metadata(&rcpt).is_ok() {
                    let r: Receipt = bincode::deserialize(&fs::read(&rcpt)?)?;
                    let _ = fs::remove_file(&rcpt);
                    let valid = r.verify(GUESS_ID).is_ok();
                    let (mut commitment, mut g, mut d) = (String::new(), 0u64, 255u8);
                    if valid {
                        if let Ok((cc, gg, dd)) = r.journal.decode::<([u8; 32], u64, u8)>() {
                            commitment = hex::encode(cc); g = gg; d = dd;
                        }
                    }
                    println!("{}", serde_json::json!({
                        "valid": valid, "commitment": commitment,
                        "guess": g, "dir": d, "dir_str": dir_str(d),
                    }));
                    return Ok(());
                }
                thread::sleep(Duration::from_millis(200));
            }
            println!("{}", serde_json::json!({
                "valid": false, "commitment": "", "guess": guess, "dir": -1,
                "dir_str": "?", "error": "host timeout",
            }));
        }
        _ => bail!("usage: zk-verify seal|prove-turn|verify|gen|host|turn ..."),
    }
    Ok(())
}
