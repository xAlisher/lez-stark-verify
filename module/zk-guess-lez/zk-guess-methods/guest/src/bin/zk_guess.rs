//! Guest entry: the #[lez_program] macro in zk-guess-program generates a crate-root
//! `pub fn main()`, called from this guest binary (same pattern as referral_credit.rs).
fn main() {
    zk_guess_program::main();
}
