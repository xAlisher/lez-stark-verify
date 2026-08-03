//! E2E submit of the zk-guess program against a LIVE sequencer (mirrors the
//! referral e2e). Proves a guess TURN settles on-zone: the sequencer accepts it
//! only if the STARK receipt verifies — "verified on LEZ" made literal.
//!
//! Env: LEE_WALLET_HOME_DIR (fresh dir), NSSA_SEQUENCER_URL, RISC0_DEV_MODE=1.
//!
//! Flow: init_game (seal C = SHA256(secret‖blind)) → guess (above/below) →
//!       negative guess with a WRONG secret → must be rejected at proving.

use anyhow::{bail, Context, Result};
use lee::privacy_preserving_transaction::circuit::ProgramWithDependencies;
use lee::program::Program;
use sha2::{Digest, Sha256};
use std::collections::HashMap;
use wallet::{
    config::{SequencerConnectionData, WalletConfigOverrides},
    WalletCore,
};
use zk_guess_program::Instruction;

const SECRET: u64 = 573_118;
const BLIND: u64 = 0x00C0_FFEE;

fn commitment(secret: u64, blind: u64) -> Vec<u8> {
    let mut h = Sha256::new();
    h.update(secret.to_le_bytes());
    h.update(blind.to_le_bytes());
    h.finalize().to_vec()
}

fn program() -> Result<ProgramWithDependencies> {
    let program = Program::new(zk_guess_methods::ZK_GUESS_ELF.to_vec().into())
        .context("decode zk-guess guest ELF")?;
    println!("program id (image id): {:?}", program.id());
    Ok(ProgramWithDependencies::new(program, HashMap::new()))
}

#[tokio::main]
async fn main() -> Result<()> {
    env_logger::init();
    let home = std::path::PathBuf::from(
        std::env::var("LEE_WALLET_HOME_DIR").context("LEE_WALLET_HOME_DIR not set")?,
    );
    std::fs::create_dir_all(&home)?;
    let seq_url: url::Url = std::env::var("NSSA_SEQUENCER_URL")
        .context("NSSA_SEQUENCER_URL not set")?
        .parse()?;
    let overrides = WalletConfigOverrides {
        sequencers: Some(vec![SequencerConnectionData {
            sequencer_addr: seq_url,
            basic_auth: None,
        }]),
        ..Default::default()
    };
    let (mut wallet, _mnemonic) = WalletCore::new_init_storage(
        home.join("wallet_config.json"),
        home.join("storage.json"),
        home.join("statistics.json"),
        Some(overrides),
        "zk-guess-e2e",
    )
    .await
    .context("wallet init")?;
    let program = program()?;

    // game master's private account = the game account
    let (game_id, _chain) = wallet.create_new_account_private(None);
    println!("game account: {game_id}");
    let c = commitment(SECRET, BLIND);
    println!("commitment: {}", hex::encode(&c));

    // ── 1. init_game — seal C ────────────────────────────────────────────
    let game = wallet.resolve_private_account(game_id).context("resolve game")?;
    let init_ix = Program::serialize_instruction(Instruction::InitGame { commitment: c.clone() })
        .context("serialize init_game")?;
    let (tx1, _) = wallet
        .send_privacy_preserving_tx(vec![game], init_ix, &program)
        .await
        .map_err(|e| anyhow::anyhow!("init_game tx failed: {e:?}"))?;
    println!("INIT submitted: tx {}", hex::encode(tx1));
    wallet.poller_helm().poll_tx(tx1).await.context("init not included")?;
    let b = wallet.sync_to_latest_block().await.context("sync after init")?;
    println!("INIT included in a block ✓ (block {b})");

    // ── 2. a guess turn (600000 > 573118 → ABOVE) ────────────────────────
    let guess_ix = Program::serialize_instruction(Instruction::Guess {
        guess: 600_000,
        secret: SECRET,
        blind: BLIND,
    })
    .context("serialize guess")?;
    let game = wallet.resolve_private_account(game_id).context("resolve game")?;
    let (tx2, _) = wallet
        .send_privacy_preserving_tx(vec![game], guess_ix, &program)
        .await
        .map_err(|e| anyhow::anyhow!("guess tx failed: {e:?}"))?;
    println!("GUESS submitted: tx {}", hex::encode(tx2));
    wallet.poller_helm().poll_tx(tx2).await.context("guess not included")?;
    let b = wallet.sync_to_latest_block().await.context("sync after guess")?;
    println!("GUESS(600000)=ABOVE included in a block ✓ (block {b}) — verified on LEZ");

    // ── 3. negative: wrong secret must fail at proving ───────────────────
    let bad_ix = Program::serialize_instruction(Instruction::Guess {
        guess: 600_000,
        secret: SECRET + 1,
        blind: BLIND,
    })
    .context("serialize bad guess")?;
    let game = wallet.resolve_private_account(game_id).context("resolve game")?;
    match wallet.send_privacy_preserving_tx(vec![game], bad_ix, &program).await {
        Err(e) => println!("WRONG-SECRET correctly rejected at proving: {e:?}"),
        Ok((tx, _)) => bail!("SOUNDNESS FAILURE: wrong secret accepted, tx {}", hex::encode(tx)),
    }

    println!("\nE2E COMPLETE: init ✓  guess ✓  wrong-secret rejected ✓");
    Ok(())
}
