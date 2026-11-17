# Modern OpenGL Card Room

A small C++17 / OpenGL 3.3 Core card-game scene. It uses OOP classes (`Shader`,
`Texture`, `Renderer`, `Deck`, `Game`, and `CardRoom`) and a deliberately small
dependency set.

## Features
- `1`: top-down camera
- `2`: angled camera focused on the table and player's cards
- `G`: draw one random character card
- Exactly six draws are allowed; subsequent `G` presses do nothing
- Cards are drawn without replacement
- Rectangular stock pile, player zone, and empty opponent zone
- Wallpaper, wood floor, torn poster, bookshelf, floor lamp, and potted plant

The opponent zone is intentionally kept empty to honor the layout requirement.
The draw rule is implemented for the player; no opponent AI is included.

## Dependencies
- C++17 compiler
- CMake 3.20+
- OpenGL 3.3 driver
- GLFW 3
- GLAD
- GLM
- stb_image

## Build with vcpkg

Install vcpkg, then from this directory:

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

Run `build/card_room` (Linux/macOS) or `build/Release/card_room.exe` (Windows).
The CMake post-build step copies `assets/` beside the executable.

## Notes / extension points
- Card faces are six original generated character illustrations in `assets/cards/`.
- `Deck` owns the remaining cards; `Game` owns the hand as pointers into the deck's
  card storage. This is safe here because the deck is populated before drawing and
  is not modified afterward.
- The current renderer uses simple textured quads and boxes to keep the example
  easy to understand. Add lighting, shadows, card flipping, mouse interaction,
  and opponent AI as separate features.
- For production use, add shader/program error handling, resize-aware projection,
  resource cleanup (RAII wrappers), and a UI text library such as Dear ImGui.
