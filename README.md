# FlyMe2TheMoon

English | [简体中文](README.zh-CN.md) | [日本語](README.ja.md)

This is a **level-based 2D flight game recreation** built on the independently developed [ElysiaEngine](https://github.com/Akil0814/ElysiaEngine), using **C++23, SDL3, Box2D, and Lua**.

Using the level data, story scripts, and audiovisual assets from miHoYo's early title *FlyMe2TheMoon* as references, this project reimplements flight controls, physics-based mechanisms, collectibles, story sequences, and menus for Android phones. Players generate thrust by jetting downward or diagonally, balance gravity and inertia to adjust their flight path, avoid obstacles, collect stars and gems, and ultimately reach the moon.

This project is an **unofficial recreation**, intended solely for learning game development and studying the original game's mechanics.

The original plan was to try **WDD (Wish-Driven Development)**: hand the requirements, engine, and assets over to AI and let it finish the game on its own. But excessive token consumption, along with problems with the quality and readability of the generated code, eventually led to manual intervention.

And even after that intervention, a fair amount of the Game Layer code is still pretty shit to read

If you like this project, go spend some money in [miHoYo](https://www.mihoyo.com/)'s games

![main](docs/main.png)

## Project Features

- **Flight and physics**: Touch input with variable thrust, steering, gravity, inertia, and collisions.
- **Levels and mechanisms**: Terrain and object data for 60 levels have been imported, with moving platforms, rotating mechanisms, spikes, wind fields, portals, lasers, launchers, bombs, and other mechanics implemented.
- **Story and presentation**: Lua drives level events, dialogue, tutorials, and camera movement. Character animations and effects for spawning, death, teleportation, reaching the goal, and collecting items are integrated.
- **Collectibles and progress**: Each level has three stars, with large and small gems distinguished. The game records level scores, best times, death counts, story flags, and local achievements, and unlocks later content according to level dependencies.
- **Landscape interface**: Includes the main menu, level selection, achievement gallery, settings, credits, and in-game HUD, pause, results, and new-record notifications.
- **Settings and audio**: Frame rate, VSync, volume, and language settings, with background music, voices, and sound effects integrated.
- **Development tools**: The F2 panel provides debug drawing, frame-rate statistics, time scaling, and level camera controls. Automated tests, input replay, and asset-checking tools are also included.

## Android build and controls

This branch targets **Android 8.0+ ARM64 phones**. It produces offline Debug and signed Release APKs; desktop builds are not maintained on this branch.

Use PowerShell 7 and JDK 21 from the repository root:

```powershell
./android/tools/setup-android.ps1 -AcceptLicenses
./android/tools/new-release-key.ps1
./android/tools/test-host.ps1
./android/tools/build-android.ps1 -Variant All
```

Run the release-key generator only once. Keep a secure backup of the signing directory outside Git. Future updates must use the same key. APKs and checksums are written to `out/android/dist/`.

Hold a screen position to generate thrust; lift to stop. Touch menus and settings directly. Android Back opens/closes pause in a level, returns from submenus, and opens exit confirmation from the main menu. Returning from the background leaves a level paused until you continue.

All game assets are bundled. First launch prepares about 145 MiB of resources in private application storage. Resources and saved progress use separate directories, so resource updates preserve progress. Uninstalling or clearing application data deletes saves.

See [Android build, signing and verification](docs/development/android.md) for pinned tools, installation, storage behavior and test limits. The F2 overlay and desktop window options are disabled in Android builds.

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
