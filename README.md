# Simple raylib C++ Tetris

A small Tetris game using raylib:

- Move and rotate tetrominoes
- Clear lines to score points
- Game ends when new pieces can no longer spawn
- Press `R` to restart

Window size is `800x450` at `60 FPS`.

## Requirements (Windows + MSYS2 UCRT64)

Install MSYS2 and packages in the **UCRT64** shell:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-raylib
```

## Build

From PowerShell in this folder:

```powershell
.\build.bat
```

`build.bat` also copies required runtime DLLs next to `game.exe`:
`libraylib.dll`, `glfw3.dll`, `libstdc++-6.dll`, `libgcc_s_seh-1.dll`, `libwinpthread-1.dll`.

Exact compile command used by `build.bat`:

```powershell
g++ main.cpp -o game.exe -std=c++17 -O2 -Wall -Wextra -IC:/msys64/ucrt64/include -LC:/msys64/ucrt64/lib -lraylib -lopengl32 -lgdi32 -lwinmm
```

`build.bat` runs that command inside:

```powershell
C:\msys64\msys2_shell.cmd -defterm -no-start -ucrt64 -c "<compile command>"
```

## Run

```powershell
C:\msys64\msys2_shell.cmd -defterm -here -no-start -ucrt64 -c "cd 'C:/Users/krist/raylib-game' && ./game.exe"
```

## Controls

- `Left/Right`: move piece
- `Up`: rotate piece clockwise
- `Down`: soft drop
- `Space`: hard drop
- `R`: restart game
# Tetris
