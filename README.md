# FlyMe2TheMoon

English | [简体中文](README.zh-CN.md) | [日本語](README.ja.md)

This is a **level-based 2D flight game recreation** built on the independently developed [ElysiaEngine](https://github.com/Akil0814/ElysiaEngine), using **C++23, SDL3, Box2D, and Lua**.

Using the level data, story scripts, and audiovisual assets from miHoYo's early title *FlyMe2TheMoon* as references, this project reimplements flight controls, physics-based mechanisms, collectibles, story sequences, and menus for desktop. Players generate thrust by jetting downward or diagonally, balance gravity and inertia to adjust their flight path, avoid obstacles, collect stars and gems, and ultimately reach the moon.

This project is an **unofficial recreation**, intended solely for learning game development and studying the original game's mechanics.

The original plan was to try **WDD (Wish-Driven Development)**: hand the requirements, engine, and assets over to AI and let it finish the game on its own. But excessive token consumption, along with problems with the quality and readability of the generated code, eventually led to manual intervention.

And even after that intervention, a fair amount of the Game Layer code is still pretty shit to read

If you like this project, go spend some money in [miHoYo](https://www.mihoyo.com/)'s games

![main](docs/main.png)

## Project Features

- **Flight and physics**: Keyboard, mouse, and gamepad input, with variable thrust, steering, gravity, inertia, and collisions. Gamepads can be connected or disconnected while the game is running.
- **Levels and mechanisms**: Terrain and object data for 60 levels have been imported, with moving platforms, rotating mechanisms, spikes, wind fields, portals, lasers, launchers, bombs, and other mechanics implemented.
- **Story and presentation**: Lua drives level events, dialogue, tutorials, and camera movement. Character animations and effects for spawning, death, teleportation, reaching the goal, and collecting items are integrated.
- **Collectibles and progress**: Each level has three stars, with large and small gems distinguished. The game records level scores, best times, death counts, story flags, and local achievements, and unlocks later content according to level dependencies.
- **Desktop interface**: Includes the main menu, level selection, achievement gallery, settings, credits, and in-game HUD, pause, results, and new-record notifications. The interface combines original assets with a landscape layout.
- **Settings and audio**: Window mode, resolution, frame rate, VSync, volume, and language settings, with background music, voices, and sound effects integrated.
- **Development tools**: The F2 panel provides debug drawing, frame-rate statistics, time scaling, and level camera controls. Automated tests, input replay, and asset-checking tools are also included.

## Quick Start

### Requirements

The primary development and validation environment is currently **Windows x64**:

- MSVC with **C++23** support and the Windows SDK, available through Visual Studio's "Desktop development with C++" workload.
- **CMake 3.24** or later.
- **Ninja**.
- **Python 3**: Used to process MSVC compiler output, check assets, and run some development tools.

Third-party dependency source code is included in `thirdparty/` and `game/vendor/`. No additional dependency downloads are needed during normal configuration.

### Build and Run

Open Visual Studio's **x64 Native Tools Command Prompt**, navigate to the project root, and run the following commands in order:

```bat
cmake -S . -B out/build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build out/build/debug --target FlyMe2TheMoon --parallel
out\build\debug\FlyMe2TheMoon.exe
```

Once loading finishes, press any key to confirm and enter the main menu, then open level selection from there.

You can also use "Open Folder" in Visual Studio to open the project and set `FlyMe2TheMoon` as the startup target.

MSVC Debug builds use `/Z7` embedded debugging information. Windows builds automatically copy `SDL3.dll` next to the executable.

**Keep the complete `assets/` directory available at runtime**, including `assets/.elysia_root` and `assets/content_registry.json`. The program searches parent directories for assets, so it can be launched from the project root or the build directory shown above.

- Logs: `logs/`
- User configuration and local saves: `player_data/`

### Build Options

Append the desired options to the CMake configuration command:

| Option | Default | Description |
| --- | --- | --- |
| `ELYSIA_ENABLE_IMGUI` | `ON` | Whether to compile the F2 development panel. Global idle cursor hiding still works when this is disabled. |
| `BUILD_TESTING` | `ON` | Whether to generate automated test targets. |
| `FLY_DEBUG_SYMBOLS` | `ON` | Retain debugging symbols for diagnostics in optimized MSVC builds. |

For Release builds, use a separate build directory and set `CMAKE_BUILD_TYPE` to `Release`.

## Controls

### Keyboard and Mouse

| Context | Controls |
| --- | --- |
| Menus | Click with the mouse; use the arrow keys to navigate, Enter to confirm, and Esc to go back. |
| Vertical thrust | Hold W, ↑, or Space. |
| Diagonal flight | Hold A / D or ← / → to apply thrust diagonally up-left / up-right. |
| Mouse flight | Hold the left mouse button to apply thrust away from the pointer relative to the character. Distance and angle affect thrust. |
| Pause / resume | Press Esc or P, or click the in-game pause button. |
| Retry the current level | Press R, or select the retry option in the pause or results menu. |
| Story | Press Enter or the left mouse button to continue. Tab skips previously read story content, subject to the story's waiting and skipping restrictions. |
| Credits | Use ↑ / ↓ to scroll, and Space to pause / resume automatic scrolling. |

### Gamepad

Gamepads recognized by SDL3 are supported. On first use, press any gamepad button to take control of the menu. Button names below follow the Xbox layout; use the corresponding button positions on other gamepads.

| Context | Controls |
| --- | --- |
| Menus, pause, and results | Use the D-pad to navigate, A to confirm, and B to go back. On the results screen, A activates the currently selected button. |
| Flight | Push the left stick up, left, or right to apply thrust; horizontal deflection controls the tilt angle. D-pad ↑ / ← / → also supports flight. |
| Vertical thrust | Gradually press RT. You can use the left stick to control the tilt angle at the same time. |
| Pause / resume | B or Start opens the pause menu. In the pause menu, B or Start returns to the game. |
| Retry the current level | Select the retry option in the pause or results menu, then press A to confirm. |
| Story | A continues; Y skips previously read story content, subject to the story's waiting and skipping restrictions. |
| Credits | Use the left stick to scroll, X to pause / resume automatic scrolling, and B to go back. |

The left stick and RT both have a **20%** dead zone. Beyond it, thrust increases continuously. Pushing the stick down alone does not apply thrust. Gamepad A is used for confirmation and story continuation, not thrust; X is not bound to restarting a level.

Keyboard flight input takes priority over gamepad input. Mouse thrust takes priority over both while the mouse button is held. Flight and story prompts change with the input device in use.

The cursor automatically hides after **2 seconds** without movement on all screens, and reappears immediately when it actually moves. Holding a mouse button, or using the keyboard or gamepad, does not reset the timer. The cursor is restored when the window loses focus, the pointer leaves the window, or the program closes.

## Development and Testing

### F2 Development Panel

In builds with ImGui enabled, press **F2** to show or hide the development panel. It is available in both menus and levels, with level camera controls integrated into the same panel.

- **Debug drawing**: View colliders, contact points and normals, AABBs, velocities, pose history, joints, and more. Select all categories, clear them, or restore the defaults. Individual primitives must be submitted by the corresponding systems.
- **Performance statistics**: View FPS, frame time, and a frame-time graph for the most recent 120 frames. Once the top-right FPS display is enabled, its statistics remain visible when the F2 panel is hidden.
- **Time controls**: Adjust simulation speed from 0–2×, with pause and resume controls. The panel remains usable while paused.
- **Level camera**: Adjust zoom from 0.1–10×, with preset and reset buttons. HUD size is unaffected, and the default zoom is restored when re-entering a level.

The project also includes automated tests, input replay, and asset-checking tools. Refer to the repository's current build configuration for specific test targets and commands.

## About the Engine

This project is built on **ElysiaEngine**. ElysiaEngine provides the underlying runtime and general-purpose engine capabilities, while FlyMe2TheMoon's game logic and Lua-related implementation reside in the Game Layer.

For further technical documentation and engine source code, see [ElysiaEngine](https://github.com/Akil0814/ElysiaEngine).

## License and Asset Sources

Code written for this project that we have the right to license is released under the [MIT License](LICENSE). Third-party libraries follow their respective licenses; see the [third-party dependency documentation](thirdparty/SDL3-DEPENDENCIES.md) and the license files distributed with the relevant components.

The original *FlyMe2TheMoon* title, characters, artwork, audio, story, and other protected content belong to their respective rights holders and are **not covered by this project's MIT License**. This is an unofficial recreation for learning and research, with no official affiliation with miHoYo.

The original materials are used for learning, research, and local validation. A copyright notice does not constitute authorization to reproduce, adapt, or redistribute the original content.


### Disclosure of AI-Assisted Asset Editing

Some artwork in this project has been adjusted with the assistance of AI tools, including but not limited to image expansion, background completion, size adaptation, and localized visual modifications.

These edits primarily adapt assets to ElysiaEngine's rendering, screen aspect ratios, and game interface requirements. They are not intended to replace the original artwork's design.

The original assets and their associated intellectual property remain with the original rights holders. AI-assisted modifications do not give this project ownership of the original assets or authorization to redistribute them. These assets are also outside the scope of this project's MIT License.
