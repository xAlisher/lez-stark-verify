{
  description = "ZK Guess — provably-fair number-guessing party game over Logos Messaging (rooms by code)";

  inputs = {
    # newest module-builder master (rev pinned in flake.lock); universal ui_qml + typed modules().
    logos-module-builder.url = "github:logos-co/logos-module-builder";
    # delivery_module auto-resolved from metadata.json `dependencies`; pin MAIN to match the platform's
    # running delivery (version 1.0.0) — a version skew leaves getClient hanging (see receiver #20).
    delivery_module.url = "github:logos-co/logos-delivery-module";
    delivery_module.inputs.logos-module-builder.follows = "logos-module-builder";
  };

  outputs = inputs@{ logos-module-builder, ... }:
    logos-module-builder.lib.mkLogosQmlModule {
      src = ./.;
      configFile = ./metadata.json;
      flakeInputs = inputs;
      # NOTE: the prover (zk-verify + r0vm, ~147 MB) is bundled into the .lgx AFTER this build by
      # tools/bundle-prover-into-lgx.sh — the module-builder's postInstall $out is filtered out by the
      # portable bundler (it copies only the plugin .so + its ldd-traced deps), so a flake-native bundle
      # isn't possible until the bundler gains an extra-files hook (upstream ask). See docs/GAME-DESIGN.md.
    };
}
