// zk-verify — the tool the Basecamp module's backend drives.
//   zk-verify gen <dir>        prove once, write valid.receipt + tampered.receipt
//   zk-verify verify <file>    verify a receipt against the guest image id → JSON
//
// `verify` is pure (no r0vm) → small + bundle-able in the module .lgx. `gen` proves
// (needs the toolchain) and is a one-shot to produce fixtures.
use anyhow::{Context, Result};
use methods::{METHOD_ELF, METHOD_ID};
use risc0_zkvm::{default_prover, ExecutorEnv, Receipt};
use std::{env, fs};

fn prove(value: u64, threshold: u64) -> Result<Receipt> {
    let env = ExecutorEnv::builder().write(&value)?.write(&threshold)?.build()?;
    Ok(default_prover().prove(env, METHOD_ELF)?.receipt)
}

fn main() -> Result<()> {
    let a: Vec<String> = env::args().collect();
    match a.get(1).map(String::as_str) {
        Some("gen") => {
            let dir = a.get(2).cloned().unwrap_or_else(|| ".".into());
            fs::create_dir_all(&dir)?;
            let r = prove(42_000, 10_000)?;
            fs::write(format!("{dir}/valid.receipt"), bincode::serialize(&r)?)?;
            let mut t = r.clone();
            t.journal.bytes[0] ^= 0xFF; // corrupt the journal → must fail verify
            fs::write(format!("{dir}/tampered.receipt"), bincode::serialize(&t)?)?;
            eprintln!("wrote valid.receipt + tampered.receipt to {dir}");
        }
        Some("verify") => {
            let path = a.get(2).context("usage: zk-verify verify <receipt>")?;
            let r: Receipt = bincode::deserialize(&fs::read(path)?)?;
            let ok = r.verify(METHOD_ID).is_ok();
            let (mut threshold, mut eligible) = (0u64, false);
            if ok {
                if let Ok((t, e)) = r.journal.decode::<(u64, bool)>() {
                    threshold = t;
                    eligible = e;
                }
            }
            // JSON for the module backend to parse
            println!(
                "{{\"valid\":{ok},\"threshold\":{threshold},\"eligible\":{eligible}}}"
            );
        }
        _ => {
            eprintln!("usage: zk-verify gen <dir> | verify <receipt>");
            std::process::exit(2);
        }
    }
    Ok(())
}
