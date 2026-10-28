# Desktop browser build

The browser edition includes the current game's full 60-level content and normal
unlock rules. It uses WebAssembly, WebGL 2, SDL3, and a browser-driven frame loop.
Touch controls are not included. ImGui, ENet, and pthreads are disabled for this
build. The Windows build retains its existing overlay, loading workers and frame
pacing.

## Build

The tested toolchain is Emscripten **5.0.6**. On this workstation:

```powershell
& 'G:\DevelopTools\Toolchains\emsdk\python\3.13.3_64bit\python.exe' game/tools/build_web.py
```

Use `--emsdk`, `--cmake`, and `--ninja` to select other tool locations. Run the
script with a Python interpreter supported by the SDK. `--fresh` clears CMake's
cached feature checks; `--package-only` repackages an already compiled runtime.
Build, cache, temporary files and packages are confined to `out/`.

The generated `out/web/runtime/manifest.json` records `gameVersion` from the CMake
project version (currently **1.0.1**). Its `version` remains the manifest schema
version, independently of game releases. The page displays the game version.
The manifest also records the source revision, whether
it included local changes, toolchain, runtime hashes, resource sizes and a build
identifier. `asset-manifest.json` lists every asset and its SHA-256. Resources,
including `assets/.elysia_root`, are sorted by path and split into packages of at
most 32 MiB without changing their contents or virtual paths. The first download
contains approximately 145 MiB of assets plus the runtime.

## Preview and prepare the website

```powershell
& 'G:\DevelopTools\Toolchains\emsdk\python\3.13.3_64bit\python.exe' game/tools/preview_web.py
```

Open <http://127.0.0.1:8765/projects/FlyMe2TheMoon/web/>. The preview serves the
existing website with the local build and page overlaid at their real deployment
paths. It does not modify the website checkout. Use `--port` and `--site-root`
when needed. QA-only flags `--fail-package` and `--disable-storage` simulate
download and IndexedDB failures; neither changes the deployment files.

After validation:

```powershell
& 'G:\DevelopTools\Toolchains\emsdk\python\3.13.3_64bit\python.exe' game/tools/deploy_web.py --site-root 'G:\Coding\Projects\Akil0814.github.io'
```

This verifies runtime hashes, copies the page, scripts, styles and runtime to
`projects/FlyMe2TheMoon/web/`, and updates only this page's English, Chinese and
Japanese dictionaries. Overwritten page files are backed up under
`out/web/backups/`, together with the old runtime and build notes. Existing project
description pages remain the entry point.
The script never runs Git commit, push, or a remote deployment. Review the
website changes and publish with its existing GitHub Pages workflow.

Runtime requests use the manifest's build identifier for cache invalidation.
Serve over HTTP or HTTPS, including the `.wasm` and `.data` files; opening the
HTML directly with `file://` is unsupported. No cross-origin isolation headers
or service worker are required. Keep third-party notices and licenses with the
runtime. A rebuild can leave older, unused package files in the output directory;
the manifest identifies which packages are actually used.

## Browser behavior

Click **Start Game** to download and launch. The startup confirmation inside the
game remains. Click the canvas to focus it and enable audio; keyboard capture is
restricted to that canvas. Space/W/Up supplies thrust, A/D steers, P pauses,
R retries and Escape returns. Existing mouse and gamepad controls are retained.
Fullscreen is requested through the page button and requires a browser gesture.
The outer game host enters fullscreen while the SDL canvas keeps its drawing
buffer size. CSS fits the complete 16:9 canvas inside the screen with letterbox
space when needed; this avoids competing browser and SDL viewport resizes that
can crop rendering to the lower-left corner. Escape or the in-screen exit
button leaves fullscreen without sending Escape to the game's navigation.
The Web settings panel exposes audio and language; canvas size, refresh cadence
and fullscreen are controlled by the browser instead of desktop window settings.

`/player_data` is mounted with IDBFS and populated before the game initializes.
Both progress and user settings are automatically written to IndexedDB. Saves
are specific to the browser and origin; a localhost save does not transfer to
GitHub Pages. Clearing site data removes it. If storage initialization fails,
the game runs with temporary data and displays a warning. The last asynchronous
write may not survive an immediate browser/process termination.

The loader prepares up to four textures/atlas frames per browser update, stopping
after approximately four milliseconds between jobs. Large individual decodes
can exceed that budget. GPU upload and existing content validation still use
the same resource/error handling as the native game. The browser controls frame
cadence; its background-tab policy may reduce or suspend updates.

## Validation records

See `out/web/validation.md` in the game workspace for this build's actual checks
and remaining manual coverage. Bundling all levels does not establish that every
level has been manually completed.
