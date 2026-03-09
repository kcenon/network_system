## Canonical Manifest

`network_system` uses the repository root [`vcpkg.json`](../../vcpkg.json) as the
single source of truth for dependency, package name, and license metadata.

This directory must not contain an independently maintained `vcpkg.json`.
Packaging and scanning workflows should read the root manifest directly so SBOM,
license, and vulnerability results cannot drift from the canonical metadata.
