# Dual-version browser release

The `web` branch builds the 1280x720 landscape edition; `web-portrait` builds the
640x960 portrait edition with its existing layouts and 1.8 camera zoom. Version
1.1.2 includes the public changes from master/dc41d06 (tag v.1.1.2), including the
level-48 portal object-ID fix, settings language rendering, ordinary projectile
bursts and aspect-correct dialogue portraits. Keep the two branches' layouts.

## Build and preview

Use Emscripten 5.0.6, C++23 and Wasm exceptions. WebGL 2 and static SDL are used;
ImGui, unused ENet and pthreads are disabled for Web. Native worker loading and
pacing remain; browser preparation is bounded per frame.

Run on each edition's clean source branch:

```powershell
& 'G:\DevelopTools\Toolchains\emsdk\python\3.13.3_64bit\python.exe' game/tools/build_web.py
& 'G:\DevelopTools\Toolchains\emsdk\python\3.13.3_64bit\python.exe' game/tests/web_package_tests.py
& 'G:\DevelopTools\Toolchains\emsdk\node\22.16.0_64bit\bin\node.exe' game/tests/web_launcher_tests.mjs
```

The script reads logical dimensions from GameModule, uses out/build/web/ or
out/build/web-portrait/, and packages into out/web/runtime/ or
out/web-portrait/runtime/. It records the CMake game version, display mode,
logical dimensions, clean source revision, build ID and SHA-256 hashes in a
schema-version-1 manifest. Keep all 60 levels and hidden root markers; stable
path-ordered data packages must each remain at most 32 MiB. Include licenses.

`--package-only` updates packaging and metadata from an existing binary; use it
only when compiled C++ and all build inputs match the recorded source.

```powershell
& 'G:\DevelopTools\Toolchains\emsdk\python\3.13.3_64bit\python.exe' game/tools/preview_web.py --host 0.0.0.0 --port 8782
```

Preview uses the actual /projects/FlyMe2TheMoon/web/ subpath, both local packages
and current shared launcher. `--deployed` reads the existing website page and
packages. Without --host, bind only to loopback. --fail-package and
--disable-storage provide separate local failure cases. A LAN address is a
different browser origin and therefore has its own saves.

## Entry and persistence

The user chooses landscape/runtime/ or portrait/runtime-portrait/ before Start.
Only the selected edition downloads. Viewport direction supplies an initial
default; rotation never replaces an active instance. Keep the game's loading
confirmation, three-language page, visitor counter and outer-host fullscreen.
Canvas geometry is configured before SDL initialization and ignores saved native
window sizes. Scale with each edition's true ratio; do not fullscreen SDL's
canvas directly. Portrait Back to Page preserves the instance; Continue returns.
The Web bridge cancels pointers and requests pause on focus loss/backgrounding;
returning leaves the player to resume manually.

Both editions restore IDBFS /player_data before game startup and automatically
persist writes. Preserve existing save file names and schema. Switch Version and
Reload await FS.syncfs(false); failure retains the instance and offers retry.
Unavailable storage allows temporary play with a visible warning and blocks
in-page switching to avoid silently discarding progress. Do not ship unlocked
saves or clear IndexedDB. Multiple simultaneous game tabs are unsupported.

Every visit fetches the small manifest without cache. Build IDs invalidate
runtime-resource URLs; normal browser HTTP caching is used. Reading cached bytes
still updates the loading progress. There is no persistent offline resource
cache, PWA or cloud save. Site-data deletion can remove saves.

## Stage and publish

Build, test and commit both source branches. Repackage from matching compiled
inputs so manifests record clean final source commits, then run:

```powershell
& 'G:\DevelopTools\Toolchains\emsdk\python\3.13.3_64bit\python.exe' game/tools/deploy_web.py --site-root 'G:\Coding\Projects\Akil0814.github.io'
```

The script validates both versions, dimensions, hashes and package sizes before
copying. It backs up both existing runtimes and managed page/dictionary files in
out/web-release/backups/, stages both editions together, removes only obsolete
files listed by the previous manifests, and verifies copied packages. It never
commits, pushes, changes Pages settings or touches unrelated website files.

Push ordinary commits to web and web-portrait; commit only managed website files
on main. Preserve unrelated working-tree edits. Wait for the existing main-root
GitHub Pages deployment, then verify both online versions and startup/fullscreen.

## Acceptance evidence

Record native baselines before merging and rerun settings languages, portal
references/real scene links, projectile bursts, progress, story, mechanisms and
Web input checks. Run the normal-input level48-route01 replay; fixture portal
positioning is separate evidence from a complete gameplay replay. Do not roll
back portrait UI to satisfy a pre-existing coordinate assertion.

Check local browser startup, confirmation, menu, dialogue, settings language
switching, pause/retry/result, fullscreen and input cancellation. Inspect portrait
360x640, 390x844 and 412x915. In an isolated origin run old 1.0.1 first, save
progress/settings, replace packages, then verify 1.1.2 restoration in both modes
and both switching directions. Fault tests must retain recoverable UI. Do not
change production saves for level-48 acceptance. Record Android hardware, Firefox,
audio, gamepad and all-level-playthrough limits honestly. Keep logs and screenshots
under out/qa/web112/ and the completed release record in out/web-release/validation.md.
