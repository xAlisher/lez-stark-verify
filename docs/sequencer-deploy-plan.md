# Deploy plan — our own public LEZ sequencer at `sequencer.logos.live`

_Goal: stand up a version-matched (rev `787a15aa`) public LEZ sequencer we control, so on-zone
settlement (win + pot) works from a catalog install without the `MethodNotFound` version wall we hit
on `testnet.lez.logos.co`. Grounded in the in-tree sequencer service + our existing `*.logos.live`
infra._

## Key finding — the cost is on the client, not the sequencer
Proving (the ~16 min / ~9.6 GB peak) happens **in the wallet/client** that submits the tx
(`lez/wallet/src/lib.rs` `execute_and_prove_with_padded_inputs` → `default_prover()` +
`ProverOpts::succinct()` in `.../privacy_preserving_transaction/circuit/mod.rs`). The **sequencer only
verifies** the receipt (`receipt.verify(PRIVACY_PRESERVING_CIRCUIT_ID)`) — seconds, low RAM. **So a
public sequencer runs on a modest box; the heavy proving stays on our submitting client.**

`RISC0_DEV_MODE` isn't in the source — it's honored implicitly by `default_prover()`/`verify`. For a
**public endpoint that accepts real STARKs, run the sequencer with `RISC0_DEV_MODE` UNSET** (and the
client must prove in real mode too). Dev-mode is iteration-only.

## What a standalone production run needs
`lez/sequencer/service/` (in `refs/logos-execution-zone`, checked out at rev `787a15aa`):
- **binary** `sequencer_service <config> [--port 3040] [--listen-address 0.0.0.0] [--home <dir>]` —
  default bind `0.0.0.0:3040`. **The RPC has NO caller auth** (main.rs comment: bind loopback unless
  firewalled).
- **`r0vm` on PATH** via `RISC0_SERVER_PATH=/usr/local/bin/r0vm` (the Dockerfile sets this; a copied
  binary panics at genesis without it).
- **`--features standalone`** (`Cargo.toml` `standalone = ["sequencer_core/mock"]`) → mocks the
  Bedrock L1; this is the mode our Sneg e2e already used.
- a **config** (copy `configs/docker/sequencer_config.json`; `home: /var/lib/sequencer_service` for
  RocksDB; set our own `signing_key` + genesis accounts that fund the wallet/e2e accounts).
- a **persistent** `home`/RocksDB dir, port 3040, `RUST_LOG=info`.
- **built from rev `787a15aa`** so `PRIVACY_PRESERVING_CIRCUIT_ID` + the RPC method set match our
  `zk-guess-program` + wallet (this is what fixes the `MethodNotFound`).

## Auth / exposure — the proxy must enforce it (the sequencer won't)
- `basic_auth` in the wallet (`lez/wallet/src/config.rs`, `multi_client.rs`) is **client-side only** —
  it just sends `Authorization: Basic <b64>`. There is **no server-side auth**.
- So: bind the sequencer to **loopback / private docker net**, and put **Caddy (443, TLS + auth)** in
  front. We already have the exact pattern in-house: `infra/logos-storage/Caddyfile.redacted` gates
  `msg.logos.live` with `@noauth not header Authorization "Bearer <TOKEN>" → 403` + `tls`. Mirror it
  for `sequencer.logos.live`, checking for `Basic <...>` so the wallet's `basic_auth` satisfies it.
  Firewall 3040 shut to the internet.

## Hosting — reuse the Hetzner VPS that already owns `*.logos.live`
- **`116.202.19.154`** (`ssh root@116.202.19.154`, `infra/logos-storage/RUNBOOK.md`) already runs
  **Caddy on :443 for `msg.logos.live`** with real TLS certs — our reverse-proxy + TLS + DNS-control
  asset. Sequencer verify is light, so it co-hosts fine alongside nwaku + logos-storage (bump to a
  dedicated small VPS only if CPU/RAM is tight).
- **DNS:** `logos.live` is ours (`msg.logos.live` already A-records to the VPS; GH pages
  `xAlisher/logos-live`). Add `sequencer.logos.live` A → `116.202.19.154` + mint a cert via the same
  certbot flow.
- Sneg has a public IP (`88.19.213.0`) but is the LAN DHCP/DNS host — **excluded** (never live-cutover
  on Sneg).

## The concrete run
**A. Build (on the VPS or a build box, rev `787a15aa`):**
```
cd refs/logos-execution-zone/lez/sequencer/service
docker compose build --build-arg STANDALONE=true sequencer_service   # builds risc0_base (r0vm) + binary
# bare-metal alt: just run-sequencer standalone "" 3040
```
**B. Run (standalone, persistent, private-bound, REAL mode)** — a compose service (from
`lez/sequencer/service/docker-compose.yml`), **no host port map** (only Caddy reaches it):
```yaml
sequencer_service:
  image: lez/sequencer_service
  command: ["sequencer_service","/etc/sequencer_service/sequencer_config.json","--port","3040"]
  environment: [ "RUST_LOG=info", "RISC0_SERVER_PATH=/usr/local/bin/r0vm" ]   # RISC0_DEV_MODE UNSET → real verify
  volumes:
    - ./sequencer_config.json:/etc/sequencer_service/sequencer_config.json
    - sequencer_data:/var/lib/sequencer_service
  restart: unless-stopped
```
**C. Expose (Caddyfile, model = logos-storage):**
```
sequencer.logos.live:443 {
  tls /certs/live/sequencer.logos.live/fullchain.pem /certs/live/sequencer.logos.live/privkey.pem
  @noauth not header Authorization "Basic <b64(user:pass) — VPS-only, not in git>"
  handle @noauth { respond "unauthorized" 403 }
  handle { reverse_proxy sequencer_service:3040 }
}
```
**D. Point the wallet:** `NSSA_SEQUENCER_URL=https://sequencer.logos.live` (our `e2e_submit.rs` reads
it). If the Basic gate is on, set the wallet `basic_auth` to the pre-encoded `user:pass` (confirm the
`common::config::BasicAuth` Display shape first).

## Open dependencies / blockers
1. **DNS** — add `sequencer.logos.live` A → `116.202.19.154`.
2. **TLS cert** — issue for the subdomain via the VPS's existing certbot.
3. **Host decision** — confirm the Hetzner VPS can spare CPU/RAM for r0vm *verification* (light);
   else a small dedicated VPS.
4. **`common::config::BasicAuth` serialization** — verify before wiring server-side basic auth.
5. **Genesis funding** — set `sequencer_config.json` genesis to fund the accounts `e2e_submit` uses
   (standalone has no external L1 bridge).

Everything else (binary, r0vm, standalone feature, RocksDB persistence, Caddy+TLS pattern, DNS
ownership) already exists in-tree at the paths above.

_Source: exploration 2026-08-03 against `refs/logos-execution-zone` @ `787a15aa` + `infra/`._
