# Box2D

Elysia vendors the minimal build surface of Box2D 3.1.1 in this directory so
configuration and builds do not require network access. The vendored files are
the upstream root and `src` CMake files, public headers, implementation sources,
and MIT license. Upstream samples, tests, benchmarks, and documentation are not
included.

Upstream: https://github.com/erincatto/box2d/tree/v3.1.1

Release archive:
https://codeload.github.com/erincatto/box2d/zip/refs/tags/v3.1.1

SHA-256:
`7c88bef902a118a2933c21027918fea455237a1310e5422daf4640d22ba9a446`

To update Box2D, download the intended tagged archive, verify its checksum,
replace the vendored build files, `include`, and `src`, then run the full Elysia
test suite and Release physics benchmark.
