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
            pkgs.cmake
            pkgs.ninja
            pkgs.pkg-config
            pkgs.uv
            pkgs.typos
          ];

          buildInputs = [
            llvmPkgs.clang

            # clang-format, clang-tidy, llvm-profdata and llvm-cov
            llvmPkgs.clang-tools
            llvmPkgs.llvm

            # C++
            pkgs.stdenv.cc.cc.lib

            # Only for FPAG_ENABLE_LIBUNWIND=ON.
            pkgs.libunwind
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
