# EPIC D — trustless pot (LEZ TOK) — real funded pot on Sneg ✅

_Real TOK staked into a program-custodied pot and settled to a winner, on the Sneg sequencer.
Issue #20 (epic #18)._

## The custody model (learned)
Faucet TOK lands in **auth-transfer-owned** accounts. A program can't directly move another
program's balance (the sequencer rejects it) — so a trustless pot must be **vault-style**: a
**program-derived pot account (PDA)** whose balance moves only via **ChainedCall** to auth-transfer.
The LEZ **vault program already implements this** (`lez/programs/vault/` — deposit into a PDA vault,
claim later), so the pot needs **no new program deploy** — it reuses the vault via the wallet.

## What runs (wallet CLI vs Sneg, dev-mode)
```
account new (pot, A, B) + auth-transfer init
pinata claim → A=150, B=150 TOK
vault transfer --from A --to POT --amount 50   → block 242   (real TOK into POT's vault PDA)
vault transfer --from B --to POT --amount 50   → block 244
  after: A=100, B=100, pot vault=100 (program-custodied — no human holds it)
vault claim --account-id POT --amount 100      → block 246   (pot → winner)
FINAL: A=100 · B=100 · winner=100
```
**Proven:** real TOK, staked into a program-custodied pot, settled to a winner, on the real
sequencer. This is the Tier-1 substance — the funds are held by the vault PDA mid-game, not a host.

## Honest remaining nuance — bind settle to the win
Today the payout is triggered manually (`vault claim` by the pot owner). The fully-trustless form
binds **settle to the on-zone win proof** — only a *proven* winner (an EQUAL guess accepted by the
zk-guess program, F) can trigger the pot release. That's the **F↔D integration**: a custom
vault-style program whose `claim`/`settle` requires the winning-guess receipt (or a CPI from the
guess program on the EQUAL branch). The custody + real-funds movement is done; this binding is the
last mile to "the pot pays the winner and no one else, enforced on-zone."

_(The earlier direct-balance `stake`/`settle` in `zk-guess-program` is kept as the naive
same-program variant; real cross-program TOK custody needs the vault/ChainedCall path above.)_

## Reuse for the module (EPIC D UX)
The Basecamp game module drives this via the `wallet` CLI (as `logos-wallet-basecamp` already does):
`pinata claim` (onboard), `vault transfer` (stake per guess), `vault claim`/settle (payout) — the
same `QProcess`-drives-`wallet` pattern used for zk-verify.
