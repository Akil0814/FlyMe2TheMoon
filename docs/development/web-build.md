# Dual-version browser release

The `web-portrait` branch builds the current 640×960 game as WebAssembly.
Its scene layouts, artwork, 1.8 camera zoom, physics, 60 levels, unlock rules and
save format come from this branch. The existing 1280×720 landscape runtime is
retained byte-for-byte in the website's `runtime/` directory. Do not rebuild it
from the portrait source or replace it with the portrait assets.

## Build and preview

Use Emscripten 5.0.6 with C++23 and Wasm exceptions. SDL and extensions are
static; ImGui, unused ENet and pthreads are disabled only for Web builds.
Desktop builds keep their worker loading and pacing. Browser preparation runs
up to four jobs, approximately four milliseconds between jobs, per update.

```powershell
& 'G:\DevelopTools\Toolchains\emsdk\python\3.13.3_64bit\python.exe' game/tools/build_web.py
& 'G:\DevelopTools\Toolchains\emsdk\python\3.13.3_64bit\python.exe' game/tests/web_package_tests.py
& 'G:\DevelopTools\Toolchains\emsdk\node\22.16.0_64bit\bin\node.exe' game/tests/web_launcher_tests.mjs
& 'G:\DevelopTools\Toolchains\emsdk\python\3.13.3_64bit\python.exe' game/tools/preview_web.py --host 0.0.0.0 --port 8772
```

Build output is `out/build/web-portrait/`; packaged output is
`out/web-portrait/runtime/`. `--package-only` repackages an existing binary;
use it only when the binary was built from the source being recorded.
Each stable, path-ordered data package is at most 32 MiB. Hidden root markers,
all game assets, third-party notices and licenses remain included.

The preview serves the actual `/projects/FlyMe2TheMoon/web/` subpath, overlaying
the current launcher and portrait package while reading the landscape package
from the website checkout. It does not modify the website. Without `--host`,
the server binds only to 127.0.0.1. `--fail-package` and `--disable-storage`
provide separate local fault-injection environments.

## Launching and switching

The page selects a default from the viewport orientation. `?mode=landscape` or
`?mode=portrait` overrides the default. Users can change the selection before
pressing Start; only that edition's packages are downloaded. Rotating the device
never changes the running edition. One Module instance runs per page.

The portrait manifest retains schema `version: 1` and reads `gameVersion` from
CMake. It adds `displayMode`, `logicalWidth` and `logicalHeight`; `buildId` and
SHA-256 hashes identify the actual package bytes. The launcher accepts the old
landscape manifest with its known 1280×720 dimensions. New portrait manifests
must report 640×960. Canvas geometry is configured before SDL starts.

The page scales each edition at its own aspect ratio. Fullscreen always targets
the outer host, avoiding conflicting SDL canvas resizes and lower-left cropping.
Portrait Start enters a focused view; Back to Page retains the instance.
The current game's own loading confirmation and all in-game UI remain intact.
The native Web bridge cancels pointers and requests the existing pause menu on
backgrounding or focus loss; returning never toggles an existing pause off.
The preserved landscape binary retains its original gameplay pause behavior.

## Shared saves

Both editions mount IDBFS at `/player_data`, restore it before initializing the
game and automatically persist writes. Existing files and schema are unchanged.
The browser's origin (protocol, hostname and port) and mount path identify the
store, not the runtime directory. Localhost, a LAN address and GitHub Pages have
different stores; different devices and browsers do not synchronize.

Switch Version and Reload await `FS.syncfs(false)` before navigating to a fresh
page. A failed flush retains the current instance and exposes a retryable error.
If storage is unavailable, temporary play is allowed with a visible warning;
in-page switching is blocked to avoid silently discarding temporary progress.
Multiple simultaneous game tabs are unsupported. Do not clear IndexedDB or
provide unlocked saves. An immediate external browser/process termination may
interrupt the last asynchronous write.

## Staging and publishing

After package and local browser checks:

```powershell
& 'G:\DevelopTools\Toolchains\emsdk\python\3.13.3_64bit\python.exe' game/tools/deploy_web.py --site-root 'G:\Coding\Projects\Akil0814.github.io'
```

The script backs up managed page files and both existing runtime directories
under `out/web-portrait/backups/`. It copies the new edition into
`runtime-portrait/`, updates the shared launcher and three language dictionaries,
and verifies that every existing landscape file is unchanged. It never commits,
pushes or changes GitHub Pages settings.

Commit the source compatibility changes on `web-portrait`; rebuild/repackage so
the manifest records that source revision. Commit the website on `main` and use
its existing Pages deployment from the root of that branch. Verify both online
editions after Pages completes. Preserve unrelated local modifications.

Native tests compare against the pre-change baseline. The current baseline's
`fly_idle_visuals` already fails its old tutorial coordinate assertion; do not
change the redesigned tutorial to satisfy it. `fly_web_input` verifies pointer
projection, cancellation and one-way pause requests. The Node test verifies lazy
loading, legacy manifests, one instance, flush ordering, failed-save retry and
touch cancellation. Browser viewport emulation is not physical Android testing;
record hardware, audio and full-level-playthrough limits in the release evidence.
