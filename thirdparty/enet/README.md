# ENet

Elysia vendors the minimal build surface of ENet 1.3.18 in this directory so
configuration and builds do not require network access. The vendored files are
the upstream CMake file, public headers, implementation sources, and MIT
license. Upstream tests, documentation, examples, and build-system files are
not included.

Upstream: https://github.com/lsalzman/enet/tree/v1.3.18

Release archive:
https://codeload.github.com/lsalzman/enet/zip/refs/tags/v1.3.18

SHA-256:
`32d6b15611c2667b88cd865e0cb48c3c011ddefcff6dfbdb266b8e0b04fffab3`

To update ENet, download the intended tagged archive, verify its checksum,
replace the vendored CMake file, `include`, and C sources, then run the full
Elysia test suite.
