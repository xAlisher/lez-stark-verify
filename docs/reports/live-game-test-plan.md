# Live game — full cycle test plan

_The interactive zk-guess game: a host holds a sealed number and proves each turn; players type
guesses in their Basecamp module and get back verified above/below verdicts. Epic #10._

## Architecture under test
```
player BC (module)          shared dir (ZK_GAME_DIR)          game host (holds secret)
  submitGuess(n)  ──req/<id>.guess──▶  req/                 zk-verify host <dir> <s> <b>
  (async, "proving…")                                        proves sign(n−s) ~8s
  verify receipt  ◀──rcpt/<id>.receipt──  rcpt/  ◀───────────  writes atomic receipt
  show verdict + narrow range
```
- **Trust:** only the host process reads the secret; players' modules never see it — they submit a
  guess and verify the returned STARK receipt. (v1 single-host model.)
- **Headless preconditions already GREEN:** `host`/`turn` proven — `500000→BELOW`, `750000→ABOVE`,
  `573118→EQUAL`, all real proofs (see `mvp-headless-report.md`).

## Setup (per test session)
1. **Host:** `zk-verify host /extra/tmp/zk-guess-game <secret> <blind>` (backgrounded). Writes
   `commitment`, serves `req/` → `rcpt/`.
2. **N Basecamps** (isolated), each launched with:
   `ZK_VERIFY_BIN=~/.local/share/zk-guess/zk-verify`, `ZK_FIXTURES=…/fixtures`,
   `ZK_GAME_DIR=/extra/tmp/zk-guess-game`. Open **ZK Guess**.
3. Test secret = **573118** (known, so verdicts are checkable). Reseal with a fresh unknown number
   for genuine blind play.

## Test matrix

| # | Test | Steps | Pass criteria |
|---|---|---|---|
| **T1** | Render + header | open ZK Guess | room renders; header `🔒 sealed 0x9322c0b1…` (matches host commitment) |
| **T2** | Guess above | type `600000` ↵ | `proving…` shows (UI not frozen); then `✓ 600000 · ABOVE · verified`; `you know` hi → 599999 |
| **T3** | Guess below | type `500000` ↵ | `✓ 500000 · BELOW`; `you know` lo → 500001 |
| **T4** | Converge + win | binary-search down to `573118` | each turn narrows the range; `573118` → `★ EXACT — you win!`; input disables (`game over`) |
| **T5** | Proving UX | watch during a turn | `your move ›` → `proving…` (amber), input disabled ~8s, re-enables on verdict; window stays responsive |
| **T6** | Bad input | type `9999999` or letters | rejected: `enter a number 0–1,000,000`; no turn submitted |
| **T7** | Host down | stop the host, then guess | after ~30s: `✗ host timeout` (no crash, no freeze) |
| **T8** | Two players, one number | BC-A and BC-B both guess | both headers show the **same** commitment; each narrows independently; both can reach `EXACT` on `573118` (no pot lock yet — that's M4) |
| **T9** | Privacy from opponents | in BC-A, read the log | BC-A shows only **its own** guesses; BC-B's guess *numbers* never appear in BC-A (only the host sees all) |
| **T10** | Honesty (can't lie) | any turn | every verdict is a client-verified STARK receipt; a wrong direction is impossible (host would need a proof that fails verify) |

## Honest scope of THIS build
- **Proven interactive game:** guess → real STARK proof → verify → narrow → win, live in the UI, multi-player against one sealed number.
- **Not yet (later increments):** staking / **pot & payout on LEZ** (M4 #16) — so T8 has no "winner takes the pot," just first-to-EXACT; **guesses private from the host** (v2 threshold-FHE — here the host sees guesses, opponents don't); anti-abandonment (VDF reveal).

## Teardown
- Stop the host daemon; kill the isolated BCs (match `XDG_DATA_HOME` to each iso dir); `rm -rf` the iso trees + `/extra/tmp/zk-guess-game`.
