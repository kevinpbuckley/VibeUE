# Optional: build or run this plugin on Linux and NixOS against an Unreal Engine install you supply
# (the Epic Linux zip unpacked anywhere). Unreal itself is not packaged here.
#
#   UE_ENGINE_ROOT=/opt/UE_5.9 nix run . -- /tmp/VibeUE-Linux   # RunUAT BuildPlugin, packaged into that folder
#   nix run .#shell     # bash with the libraries the editor, UBT's .NET and clang load, e.g. for BuildAndLaunchGame.sh
{
  description = "Build VibeUE against a user-supplied Unreal Engine install";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";

  outputs = { nixpkgs, ... }:
    let
      pkgs = nixpkgs.legacyPackages.x86_64-linux;

      # An FHS root, so the engine's prebuilt binaries find their libraries and its #!/bin/bash scripts run unpatched.
      ueFhs = name: runScript: pkgs.buildFHSEnv {
        inherit name runScript;
        targetPkgs = p: with p; [
          # UnrealBuildTool's bundled .NET
          icu krb5 openssl zlib
          # editor: GPU and display (Vulkan and Wayland are dlopen'ed)
          vulkan-loader libglvnd libGLU libdrm libgbm wayland libxkbcommon
          libx11 libxcb libxext libxfixes libxrandr libxcomposite libxdamage
          libxcursor libxi libxinerama libxrender libxscrnsaver
          # editor: fonts, audio, desktop, CEF
          fontconfig freetype alsa-lib libpulseaudio dbus glib expat at-spi2-core
          cairo pango gtk3 nss nspr cups
          zstd lz4 xz bzip2 util-linux systemd sqlite
          procps # pgrep, used by BuildAndLaunchGame.sh
        ];
      };

      buildPlugin = pkgs.writeShellScript "build-plugin" ''
        set -euo pipefail
        : "''${UE_ENGINE_ROOT:?set UE_ENGINE_ROOT to the Unreal Engine folder that contains Engine/}"
        if (($# < 1)); then
          echo "usage: nix run . -- OUTPUT_DIR [extra RunUAT BuildPlugin arguments]" >&2
          echo "OUTPUT_DIR must be outside the plugin and outside any project's Plugins folder." >&2
          exit 2
        fi
        if [[ ! -f VibeUE.uplugin ]]; then
          echo "run this from the VibeUE plugin folder (the one with VibeUE.uplugin)" >&2
          exit 2
        fi
        out="$(realpath -m -- "$1")"
        shift
        exec "$UE_ENGINE_ROOT/Engine/Build/BatchFiles/RunUAT.sh" BuildPlugin \
          -Plugin="$PWD/VibeUE.uplugin" -Package="$out" -TargetPlatforms=Linux "$@"
      '';
    in
    {
      apps.x86_64-linux = {
        default = {
          type = "app";
          meta.description = "RunUAT BuildPlugin for VibeUE inside an FHS environment";
          program = "${ueFhs "vibeue-build-plugin" buildPlugin}/bin/vibeue-build-plugin";
        };
        shell = {
          type = "app";
          meta.description = "bash inside the same FHS environment";
          program = "${ueFhs "vibeue-shell" "bash"}/bin/vibeue-shell";
        };
      };
    };
}
