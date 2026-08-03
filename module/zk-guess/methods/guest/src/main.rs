// zk-guess M1 guest — the game oracle.
//
// Proves the honest direction of a PUBLIC guess vs a SEALED secret, WITHOUT
// revealing the secret, and proves the secret is the COMMITTED one (no swap):
//   private witnesses : secret, blind   (never committed)
//   public inputs     : guess, commitment = SHA256(secret_le ‖ blind_le)
//   journal (public)  : (commitment, guess, dir)   dir ∈ {0 below,1 equal,2 above}
//
// Soundness: an inconsistent (secret,blind) halts the guest, so no valid receipt
// can exist for a swapped number — the host cannot lie OR move the target.
// Uses sha2 in both guest and host, so the commitment digest matches by construction.
use risc0_zkvm::guest::env;
use sha2::{Digest, Sha256};

fn main() {
    // PRIVATE witnesses — never committed
    let secret: u64 = env::read();
    let blind: u64 = env::read();
    // PUBLIC inputs
    let guess: u64 = env::read();
    let commitment: [u8; 32] = env::read();

    // 1) commitment-open: bind the sealed number. A swapped secret fails here.
    let mut h = Sha256::new();
    h.update(secret.to_le_bytes());
    h.update(blind.to_le_bytes());
    let digest: [u8; 32] = h.finalize().into();
    assert!(
        digest == commitment,
        "commitment mismatch: sealed number was swapped"
    );

    // 2) honest direction, from the GUESS's perspective:
    //    0 = below (guess < secret) · 1 = equal · 2 = above (guess > secret)
    let dir: u8 = match guess.cmp(&secret) {
        core::cmp::Ordering::Less => 0,
        core::cmp::Ordering::Equal => 1,
        core::cmp::Ordering::Greater => 2,
    };

    // Journal = only the public claim. Zero-knowledge of `secret`.
    env::commit(&(commitment, guess, dir));
}
