# Night at the Table

A modern OpenGL 3.3 core-profile card game using native Win32 and WGL.

The renderer uses GLSL vertex/fragment shaders, VAO/VBO/EBO geometry, uniform
orthographic transforms, and a texture-backed stock/card draw path. It uses
only the Windows SDK and system OpenGL library. Modern OpenGL functions are
loaded directly with `wglGetProcAddress`; no third-party libraries are needed.

## Controls

- `1`: top-down view with the stock pile arranged horizontally
- `2`: table view with cards in hand
- `3`: clear card layout view
- `4`: clear card layout view
- `0`: cinematic view of the room
- `G`: both players draw one random card; draws stop after five presses
- `D`: both players discard a selected card
- `Esc`: quit

The scene includes a three-zone table, wooden flooring, striped wallpaper, a torn poster, and three decor pieces: a lamp, plant, and side cabinet. Cards use procedural character/suit textures and do not require external assets.
