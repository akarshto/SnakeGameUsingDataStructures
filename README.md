# Snake Game Using Data Structures (C)

A terminal snake game written in C (Windows console or ncurses) where the core pieces are built on data structures:

| Structure | Used for |
|-----------|----------|
| Linked-list **queue** | The snake body. Moving = enqueue the new head at the rear, dequeue the tail from the front. |
| Linked-list **queue** | The numbers the snake eats. They are shown (FIFO order) on the game-over screen and printed to the terminal on exit. |
| 2D occupancy grid | O(1) wall/self-collision checks and food placement that never lands on the snake. |

## Run it

### Windows (no libraries needed)

The game uses the native Windows console, so there is nothing to install.

- **Quickest:** open `cmd`, PowerShell or Windows Terminal in this folder and run `snake.exe` (a prebuilt 64-bit exe is included).
- **Build it yourself:** run `build.bat`. It uses `gcc` (MinGW-w64) if found, otherwise `cl` from a Visual Studio Developer Command Prompt. With MinGW you can also run `mingw32-make`.

Run it from a console window rather than double-clicking, so you can see the final output after quitting.

### Linux / macOS (ncurses)

```bash
# Debian / Ubuntu:  sudo apt install build-essential libncurses-dev
# Fedora:           sudo dnf install gcc make ncurses-devel
# macOS:            Xcode command line tools (ncurses is built in)
make
./snake
```

The terminal must be at least 22 columns x 14 rows; the board scales up to 60 x 24 cells.

## Controls

| Key | Action |
|-----|--------|
| Arrow keys or W A S D | Steer the snake |
| P | Pause / resume |
| ENTER | Start from the title screen |
| R | Play again after game over |
| Q | Quit |

## Rules

- Each food item is a digit 1-9. Eating it adds `digit x 10` to your score, grows the snake, and enqueues the digit.
- Hitting a wall or yourself ends the game. Filling the whole board wins.
- The snake speeds up as it eats; vertical moves are slowed slightly so speed looks even in tall terminal cells.
- On quit, the final score and every number eaten are printed in the order they were eaten.

## What was fixed from the original version

- The snake moved twice per frame (once in the main loop, once in `draw`), and input was blocking. Replaced with a clean time-based game loop and non-blocking input.
- Food could spawn on the snake or on the border, and the number shown on the board was not the number stored. Food now spawns only on free cells, and the stored number is the one displayed.
- The "numbers eaten" array held only 10 entries and was not a queue. It is now an unbounded linked-list queue.
- Reversing direction into yourself was possible, and `lastDirection` was applied inconsistently. Direction handling is now safe against 180-degree turns.
- Missing `#include <time.h>`, an unused `pthread` include, uninitialised `numbersEaten`, and memory leaks (the snake was never freed). All fixed; verified clean with AddressSanitizer/UBSan/LeakSanitizer.
- Added a title screen, pause, restart, high score, win condition, colours, and a too-small-terminal check.
