# Wicked Launcher Nix Packaging

## Installing a development release (flake)

This flake builds Wicked Launcher from source. No binary cache is provided, so
the first build will compile the launcher and its dependencies.

### Installing the package directly

After adding `github:W1ckeddamn/WickedLauncher` to your flake inputs, you can access the flake's `packages` output.

Example:

```nix
{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

    wickedlauncher = {
      url = "github:W1ckeddamn/WickedLauncher";

      # Optional: Override the nixpkgs input of wickedlauncher to use the same revision as the rest of your flake
      # Note that this may break the reproducibility mentioned above.
      #
      # inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs =
    { nixpkgs, wickedlauncher, ... }:
    {
      nixosConfigurations.foo = nixpkgs.lib.nixosSystem {
        modules = [
          ./configuration.nix

          (
            { pkgs, ... }:
            {
              environment.systemPackages = [ wickedlauncher.packages.${pkgs.system}.wickedlauncher ];
            }
          )
        ];
      };
    };
}
```

### Using the overlay

Alternatively, if you don't want to use our `packages` output, you can add our overlay to your nixpkgs instance.
This will ensure Wicked Launcher is built with your system's packages.

> [!WARNING]
> Depending on what revision of nixpkgs your system uses, this may result in binaries that differ from the above `packages` output
> If this is the case, you will not be able to use the binary cache

Example:

```nix
{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

    wickedlauncher = {
      url = "github:W1ckeddamn/WickedLauncher";

      # Optional: Override the nixpkgs input of wickedlauncher to use the same revision as the rest of your flake
      # Note that this may break the reproducibility mentioned above.
      #
      # inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs =
    { nixpkgs, wickedlauncher, ... }:
    {
      nixosConfigurations.foo = nixpkgs.lib.nixosSystem {
        modules = [
          ./configuration.nix

          (
            { pkgs, ... }:
            {
              nixpkgs.overlays = [ wickedlauncher.overlays.default ];

              environment.systemPackages = [ pkgs.wickedlauncher ];
            }
          )
        ];
      };
    };
}
```

### Installing the package ad-hoc (`nix shell`, `nix run`, etc.)

You can simply call the default package of this flake.

Example:

```shell
nix run github:W1ckeddamn/WickedLauncher

nix shell github:W1ckeddamn/WickedLauncher

nix profile install github:W1ckeddamn/WickedLauncher
```

## Installing a development release (without flakes)

### Installing the package directly (`fetchTarball`)

We use flake-compat to allow using this Flake on a system that doesn't use flakes.

Example:

```nix
{ pkgs, ... }:
{
  environment.systemPackages = [
    (import (
      builtins.fetchTarball "https://github.com/W1ckeddamn/WickedLauncher/archive/develop.tar.gz"
    )).packages.${pkgs.system}.wickedlauncher
  ];
}
```

### Using the overlay (`fetchTarball`)

Alternatively, if you don't want to use our `packages` output, you can add our overlay to your instance of nixpkgs.
This results in Wicked Launcher using your system's libraries

Example:

```nix
{ pkgs, ... }:
{
  nixpkgs.overlays = [
    (import (
      builtins.fetchTarball "https://github.com/W1ckeddamn/WickedLauncher/archive/develop.tar.gz"
    )).overlays.default
  ];

  environment.systemPackages = [ pkgs.wickedlauncher ];
}
```

### Installing the package ad-hoc (`nix-env`)

You can add this repository as a channel and install its packages that way.

Example:

```shell
nix-channel --add https://github.com/W1ckeddamn/WickedLauncher/archive/develop.tar.gz wickedlauncher

nix-channel --update wickedlauncher

nix-env -iA wickedlauncher.wickedlauncher
```

## Package variants

This repository offers the following packages:

- `wickedlauncher` - The preferred build, wrapped with everything necessary to run the launcher and Minecraft
- `wickedlauncher-unwrapped` - A minimal build that allows for advanced customization of the launcher's runtime environment

### Customizing wrapped packages

The wrapped package (`wickedlauncher`) offers some build parameters to further customize the launcher's environment.

The following parameters can be overridden:

- `additionalLibs` (default: `[ ]`) Additional libraries that will be added to `LD_LIBRARY_PATH`
- `additionalPrograms` (default: `[ ]`) Additional libraries that will be added to `PATH`
- `controllerSupport` (default: `isLinux`) Turn on/off support for controllers on Linux (macOS will always have this)
- `gamemodeSupport` (default: `isLinux`) Turn on/off support for [Feral GameMode](https://github.com/FeralInteractive/gamemode) on Linux
- `jdks` (default: `[ jdk21 jdk17 jdk8 ]`) Java runtimes added to `WICKEDLAUNCHER_JAVA_PATHS` variable
- `msaClientID` (default: `null`, requires full rebuild!) Client ID used for Microsoft Authentication
- `textToSpeechSupport` (default: `isLinux`) Turn on/off support for text-to-speech on Linux (macOS will always have this)
