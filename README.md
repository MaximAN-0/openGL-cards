# Card Room

A small 3D card game scene written in C++17 and OpenGL 3.3 (core profile). You sit at a low table in a room with a bookshelf, a bonsai, a lamp, an armor stand and a torn poster. You and an opponent draw tarot-style cards from a stock pile, and you can discard from your hand.

Everything in the room is built from one shared quad and one shared box, stretched and moved with transform matrices. Textures are either loaded from `assets/` or generated in code, so the project runs even with no image files.

## Controls

| Key | Action |
|---|---|
| `1` | Top-down view |
| `2` | First-person view (your hand fans out in front of you) |
| `3` | Table view from the corner |
| `4` | Mirrored corner view, behind the opponent |
| `0` | Toggle the cinematic camera that tours the room |
| `G` | Draw a card (you and the opponent each draw one) |
| `Left` / `Right` | Move the selection along your hand |
| `D` | Discard the selected card (the opponent discards a random one) |
| `Esc` | Quit |

The window title shows live counters: cards in the stock, your hand, the opponent's hand, draws used (out of 5), the discard pile size and the name of the selected card.

## Requirements

- A C++17 compiler (GCC, Clang or MSVC)
- CMake 3.16 or newer
- An OpenGL 3.3 capable GPU and driver
- [GLFW 3](https://www.glfw.org/)
- [GLM](https://github.com/g-truc/glm)
- [glad](https://glad.dav1d.de/) generated for OpenGL 3.3 core (glad v1: it must provide `glad/glad.h`, `KHR/khrplatform.h` and `glad.c`)
- [stb_image.h](https://github.com/nothings/stb) (single header)

## Project layout

```
opengl-cards/
├── CMakeLists.txt
├── GLProject/            all the source code
│   ├── main.cpp          entry point
│   ├── application.h     window, input, main loop, camera switching
│   ├── game.h            rules, deck / players / discard pile, window title text
│   ├── player.h          a hand of cards: layout, selection, first-person fan
│   ├── deck.h            the stock pile
│   ├── card.h            a card: pose, flip, lift, animation
│   ├── camera.h          camera poses, transitions, cinematic camera
│   ├── room.h, decor.h   room, walls, poster and props
│   ├── table.h           table, felt, zones and pile slots
│   ├── mesh.h            vertex data, quad and box shapes, Transform
│   ├── polygon.h         GPU buffers and draw calls
│   ├── shader.h          GLSL source and the Shader wrapper
│   ├── texture.h         OpenGL texture wrapper
│   ├── procedural.h      generated textures, image loading, poster tear
│   ├── config.h          layout constants
│   └── stb_impl.cpp      compiles the stb_image implementation
├── third_party/          glad and stb (or your own shared folder, see below)
└── assets/               optional images
```

## Building

Place glad and stb where `CMakeLists.txt` expects them. The default is:

```
third_party/glad/include/glad/glad.h
third_party/glad/include/KHR/khrplatform.h
third_party/glad/src/glad.c
third_party/stb/stb_image.h
```

If your shared code lives in a differently named folder (for example `OpenGL.SharedModule`), change these two lines in `CMakeLists.txt`:

```cmake
set(GLAD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad")
set(STB_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb")
```

Then build and run:

```
cmake -S . -B build
cmake --build build
./build/card_room
```

On Windows with Visual Studio, run the same commands and start `card_room.exe` from the build configuration folder.

## Assets

Textures are optional. Any file that is missing is replaced by a procedurally generated one, and a message like `[assets] wood.png not found - using a procedural texture` is printed to the console.

| File (inside `assets/`) | Used for |
|---|---|
| `wood.png` | Floor |
| `wallpaper.png` | Walls |
| `poster.png` | Poster on the wall (torn automatically, see below) |
| `cards/back.png` | Card back |
| `cards/<index>_<name>.png` | Card faces, for example `cards/0_thefool.png` |

The poster is cut with a ragged torn edge and a missing top corner whether it is generated or loaded from `poster.png`. The image does not need transparency.

Paths are relative to the folder you run the program from, because `ASSET_DIR` defaults to `assets/` in `config.h`. `CMakeLists.txt` can set an absolute path instead:

```cmake
target_compile_definitions(card_room PRIVATE ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/")
```

## How the pieces fit together

1. **`Application`** creates the window and the OpenGL context, then loops: poll events, update, render, swap buffers.
2. **`Game`** owns the `Deck`, two `Player` objects and the discard pile. A card is owned by exactly one of these at a time; drawing or discarding moves it from one container to another.
3. A **`Card`** stores a pose (position, yaw, flip, tilt, roll, scale, lift). Moving a card means starting an animation from its current pose to a target pose. Each frame the pose becomes a model matrix.
4. **Selection** is a single index into your hand. The selected card slides up and turns warmer in colour. There is no mouse picking.
5. **Rendering** uses one shader. Each object sets its model matrix, tint and texture, and the shared shape is drawn. Pixels with very low alpha are discarded, which is how cut-out shapes like the torn poster work.

## Customising

- **Layout:** edit `config.h` (window size, draw limit, pile positions, hand spacing).
- **New props:** add a subclass of `Decor` in `decor.h` and add it in the `Room` constructor.
- **Poster tear:** the shape is in `tearAt` in `procedural.h`.
- **New card images:** drop PNG files into `assets/cards/`.

## Troubleshooting

- **Black window or a crash at start:** the GPU or driver may not support OpenGL 3.3 core. The console prints the reason.
- **Card faces look generated instead of loaded:** the code looks for lowercase file names such as `0_thefool.png`. Windows ignores case, but Linux and macOS do not, so rename files like `0_theFool.png` to lowercase.
- **Textures not loading at all:** you are probably running from a different folder than the one that contains `assets/`. Set `ASSET_DIR` as shown above.
- **`glad/glad.h` not found:** generate glad v1 for OpenGL 3.3 core. Files generated by glad v2 (`gl.h`) will not work.