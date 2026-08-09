{
  description = "fpag development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        llvmPkgs = pkgs.llvmPackages_22;
      in
      {
        devShells.default = pkgs.mkShell {
          nativeBuildInputs = [
            pkgs.xmake
            pkgs.ninja
            pkgs.pkg-config
          ];

          buildInputs = [
            llvmPkgs.clang

            # C++
            pkgs.stdenv.cc.cc.lib
          ];

          shellHook = ''
            export LD_LIBRARY_PATH="${pkgs.lib.makeLibraryPath [
              pkgs.stdenv.cc.cc.lib
            ]}:$LD_LIBRARY_PATH"
          '';
        };
      }
    );
}

