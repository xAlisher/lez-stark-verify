# ADR-0007 — Basic-auth wiring for the auth-gated sequencer

**Status:** accepted

## Context
The LEZ `sequencer_service` RPC has **no built-in caller auth** (its own source says: bind loopback
unless firewalled). Exposing `sequencer.logos.live` publicly (ADR-0003) needs *something* in front to
gate writes. Caddy already fronts the VPS with TLS, so it's the natural place — using an
`Authorization` header match.

The wallet supports a `basic_auth` field, but with a **wire-format quirk**: it sends
`Authorization: Basic {Display(BasicAuth)}` where `Display` renders `username:password`
**un-re-encoded** — *not* the base64 that HTTP Basic and Caddy's matcher expect.

## Decision
Gate `sequencer.logos.live` in Caddy with a literal header match:
`@noauth not header Authorization "Basic <base64(user:pass)>" → 403`. To make the wallet emit exactly
that header, stash the **pre-computed base64 blob in `BasicAuth.username`** with `password: None` →
the wallet sends `Basic <base64>`, matching Caddy. In `zk-guess-methods/e2e_submit.rs` this is env
`SEQ_BASIC_AUTH`; the module reads URL/auth/binary **from env only — never shipped in the `.lgx`**.

Verified live: no-auth → 403, wallet-header → 200.

## Consequences
- The public RPC is TLS + auth-gated without touching the auth-less sequencer itself; the credential
  lives VPS-side and in the launch env, never in git or the distributed module.
- It's a **shared write credential**, not per-user — fine for a gated demo endpoint, not for
  untrusted public write. Per-user auth (or an on-zone rate/fee gate) is the path if this opens up.
- The base64-in-username trick is a workaround for the wallet's `Display` format; a cleaner fix is a
  wallet that base64-encodes `basic_auth` itself (upstream nicety).
