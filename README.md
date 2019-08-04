# Conway's Game of Life
I've been programming in modern C++ for work for several months now, without working in C.
I want to make sure I don't get too rusty with idiomatic C, so I coded up a little Game of Life implementation.
Being contained within a single file, this isn't the most representative C project I could have done, with little opportunity for modular design etc.
Please see the awesome [Wikipedia page](https://en.wikipedia.org/wiki/Conway%27s_Game_of_Life) for more information about this unusual but interesting "game."

## Dependencies and Building
This should be pretty portable.
The only major dependency is ncurses which is widely available on POSIX-y systems.
I've provided a CMakeLists.txt file, which should handle builds on most platforms.
I have only tested on MacOS though.

## Notes
For a similar afternoon project in C++ that provides an ncurses minesweeper game, see my [minesweeper repository](https://github.com/jeresch/minesweeper).
