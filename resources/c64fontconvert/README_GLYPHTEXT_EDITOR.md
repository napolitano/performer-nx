# glyphtext_editor

`glyphtext_editor` is an interactive ncurses-based editor for glyph text files.

It is designed for fast pixel editing of bitmap fonts in the text format used by this toolchain.

## Features

- Edit glyph pixels directly in a zoomed grid
- Supports flexible glyph sizes (e.g. `5x5`, `5x8`, `8x8`, `12x12`)
- Optional `Size: WxH` header parsing and saving
- Glyph list navigation by index
- Popup menu for common operations
- Inverted header, status, and help footer for better readability
- Single preview area: zoomed source preview
- Atomic save (`.tmp` + rename)

## Input File Format

Supported format:

```text
Size: 8x8

Glyph 0:
--------
--****--
-*--*-*-
-*-*-**-
-*-****-
-*------
--****--
--------
```

Notes:

- `Size: WxH` is optional.
- If missing, size is inferred from the first glyph block.
- All glyphs in one file must have the same dimensions.
- `*` means bit `1`, `-` means bit `0`.

## Build

```bash
g++ -std=c++17 -O2 -Wall -Wextra -pedantic glyphtext_editor.cpp -lncurses -o glyphtext_editor
```

## Run

```bash
./glyphtext_editor ./font-source/8x8font.txt
```

You can also start the editor with a directory instead of a file:

```bash
./glyphtext_editor ./font-source
```

In that mode, an ncurses file picker opens and lists:

- subdirectories
- parent directory (`[..]`)
- `.txt` files only

Recommendation:

- Prefer running in a dedicated system terminal (`xterm`, `gnome-terminal`, `Windows Terminal`, etc.).
- Running inside some IDE-integrated terminals can cause ncurses repaint artifacts or partially corrupted visuals.

## Keyboard Controls

- `Arrow keys` / `h j k l`: move cursor or selection
- `TAB`: switch focus between editor and glyph list
- `SPACE`: toggle current pixel
- `n` / `p`: next / previous glyph index
- `g`: jump to glyph index
- `o`: open/load a glyph text file via file picker
- `m`: open popup menu
- `s`: save
- `q`: quit

Direct transform shortcuts:

- `c`: clear glyph
- `d`: delete glyph
- `H`: mirror horizontal
- `V`: mirror vertical
- `R`: rotate clockwise (square glyphs only)
- `I`: invert glyph

## UI Layout

- **Header (inverted):** file path, size, current glyph, focus
- **Left pane:** zoomed editable glyph grid (dark gray editable background)
- **Middle pane:** zoomed preview
- **Right pane:** glyph index table (`10` indices per row)
- **Status line:** action feedback and errors
- **Footer (inverted):** quick help

## File Picker

The file picker is available:

- automatically when the editor is started with a directory
- from the popup menu via `Load...`
- directly via the `o` shortcut

Navigation inside the picker:

- `Up` / `Down` / `k` / `j`: move selection
- `PageUp` / `PageDown`: scroll faster
- `Enter`: open directory or load selected `.txt` file
- `Backspace`: go to parent directory
- `Esc` / `q`: cancel

## Important Behavior

- The **Zoomed Preview** keeps the existing ncurses/textual representation.
- Glyph list shows index range `0..maxIndex` where `maxIndex` is the maximum glyph index from the source file.
- Undefined indices are shown dim/gray; defined indices are shown normal.
- If an undefined glyph index is edited, it becomes defined and list styling updates accordingly.
- Saving always writes `Size: WxH` to make non-8x8 documents explicit and stable.
- Rotation is limited to square glyph sizes (`5x5`, `8x8`, `12x12`, ...).

## Troubleshooting

- If UI looks clipped, increase terminal size.
- Minimum practical size is around `80x12` for 10-column glyph list layout.
- If parsing fails, check row width/height consistency and invalid characters.

## TODO

- Add non-square rotate modes (e.g. rotate with crop or resize policy)
- Add copy/paste between glyphs
- Add undo/redo stack
- Add optional mouse editing

