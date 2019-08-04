#include <stdlib.h>
#include <curses.h>
#include <unistd.h>
#include <errno.h>

typedef enum TileState {
    DEAD = 0, ALIVE = 1
} TileState;

typedef struct Board {
    unsigned int nrows;
    unsigned int ncols;
    TileState *tiles;
    unsigned int nalive;
} Board;

typedef struct Point {
    unsigned int row;
    unsigned int col;
} Point;

typedef struct TileChange {
    Point point;
    TileState newState;
} TileChange;

typedef struct TileChangeNode {
    TileChange change;
    struct TileChangeNode *next;
} TileChangeNode;

typedef struct TileChangeStack {
    TileChangeNode *top;
} TileChangeStack;

void pushTileChange(TileChangeStack *stack, TileChange change) {
    TileChangeNode *newNode = (TileChangeNode *) malloc(sizeof(TileChangeNode));
    newNode->change = change;
    newNode->next = stack->top;
    stack->top = newNode;
}

bool tileChangeStackIsEmpty(TileChangeStack *stack) {
    return stack->top == NULL;
}

TileChange popTileChange(TileChangeStack *stack) {
    TileChangeNode *prevTop = stack->top;
    stack->top = prevTop->next;
    TileChange result = prevTop->change;
    free(prevTop);
    return result;
}

typedef struct GameState {
    Board logicalBoard;
    Point logicalCur;
    WINDOW *physicalBoard;
    WINDOW *promptWin;
    unsigned int ticksPerSec;
    unsigned int tick;
    TileChangeStack pendingChanges;
} GameState;

void showCursor(const GameState * const gameState) {
    wmove(gameState->physicalBoard, gameState->logicalCur.row, gameState->logicalCur.col);
    wrefresh(gameState->physicalBoard);
}

TileState getTileState(const Board * const board, const unsigned int row, const unsigned int col) {
    return board->tiles[row * board->ncols + col];
}

void setTileState(Board * const board, TileState val, const unsigned int row, const unsigned int col) {
    TileState prev = board->tiles[row * board->ncols + col];
    board->tiles[row * board->ncols + col] = val;
    if (prev == ALIVE && val == DEAD) {
        board->nalive--;
    } else if (prev == DEAD && val == ALIVE) {
        board->nalive++;
    }
}

void toggleTileState(GameState * const gameState) {
    TileState current = getTileState(&gameState->logicalBoard, gameState->logicalCur.row, gameState->logicalCur.col);
    switch (current) {
    case DEAD:
        setTileState(&gameState->logicalBoard, ALIVE, gameState->logicalCur.row, gameState->logicalCur.col);
        waddch(gameState->physicalBoard, 'X');
        break;
    case ALIVE:
        setTileState(&gameState->logicalBoard, DEAD, gameState->logicalCur.row, gameState->logicalCur.col);
        waddch(gameState->physicalBoard, ' ');
        break;
    default:
        exit(1);
    }
    wmove(gameState->physicalBoard, gameState->logicalCur.row, gameState->logicalCur.col);
}

bool setUpBoard(GameState * const gameState) {
    wprintw(gameState->promptWin, "Use arrow keys and spacebar to set tiles. Then press enter to continue.");
    wrefresh(gameState->promptWin);

    while (true) {
        int c = getch();
        switch (c) {
        // Toggling tiles
        case KEY_RIGHT:
            if (gameState->logicalCur.col == gameState->logicalBoard.ncols - 1) {
                break;
            }
            gameState->logicalCur.col += 1;
            showCursor(gameState);
            break;
        case KEY_LEFT:
            if (gameState->logicalCur.col == 0) {
                break;
            }
            gameState->logicalCur.col -= 1;
            showCursor(gameState);
            break;
        case KEY_DOWN:
            if (gameState->logicalCur.row == gameState->logicalBoard.nrows - 1) {
                break;
            }
            gameState->logicalCur.row += 1;
            showCursor(gameState);
            break;
        case KEY_UP:
            if (gameState->logicalCur.row == 0) {
                break;
            }
            gameState->logicalCur.row -= 1;
            showCursor(gameState);
            break;
        case ' ':
            toggleTileState(gameState);
            wrefresh(gameState->physicalBoard);
            break;
        case KEY_ENTER: case '\n':
            return true;
        case 'q':
            return false;
        default:
            break;
        }
    }
}

bool getTicksPerSecond(GameState * const gameState) {
    wclear(gameState->promptWin);
    mvwprintw(gameState->promptWin, 0, 0, "Now type the desired ticks/sec and press enter to confirm: ");
    wrefresh(gameState->promptWin);
    echo();

    char buf[256];
    while (true) {
        wgetnstr(gameState->promptWin, buf, 256);
        char *end;
        unsigned int ticks = strtoul(buf, &end, 10);
        int err = errno;
        if (err == EINVAL || err == ERANGE) {
            return false;
        }
        if (buf != end) {
            gameState->ticksPerSec = ticks;
            noecho();
            return true;
        }

        wprintw(gameState->promptWin, "Invalid value");
        wrefresh(gameState->promptWin);
    }
}

void handleTile(GameState * const gameState, const unsigned int row, const unsigned int col) {
    TileState currentState = getTileState(&gameState->logicalBoard, row, col);
    unsigned int numAliveNeighbors = 0;
    const int offsets[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1}};
    for (unsigned int i = 0; i < 8; ++i) {
        int r = row + offsets[i][0];
        int c = col + offsets[i][1];
        if (r < 0 || r > gameState->logicalBoard.nrows - 1 || c < 0 || c > gameState->logicalBoard.ncols - 1) {
            continue;
        }

        TileState neighborState = getTileState(&gameState->logicalBoard, r, c);
        if (neighborState == ALIVE) {
            ++numAliveNeighbors;
        }
    }

    TileChange change;
    change.point.row = row;
    change.point.col = col;

    if (currentState == ALIVE && numAliveNeighbors < 2) {
        change.newState = DEAD;
        pushTileChange(&gameState->pendingChanges, change);
    } else if (currentState == ALIVE && (numAliveNeighbors == 2 || numAliveNeighbors == 3)) {
    } else if (currentState == ALIVE && numAliveNeighbors > 3) {
        change.newState = DEAD;
        pushTileChange(&gameState->pendingChanges, change);
    } else if (currentState == DEAD && numAliveNeighbors == 3) {
        change.newState = ALIVE;
        pushTileChange(&gameState->pendingChanges, change);
    }
}

void doChanges(GameState * const gameState) {
    while (!tileChangeStackIsEmpty(&gameState->pendingChanges)) {
        TileChange change = popTileChange(&gameState->pendingChanges);
        setTileState(&gameState->logicalBoard, change.newState, change.point.row, change.point.col);
        if (change.newState == ALIVE) {
            mvwaddch(gameState->physicalBoard, change.point.row, change.point.col, 'X');
        } else if (change.newState == DEAD) {
            mvwaddch(gameState->physicalBoard, change.point.row, change.point.col, ' ');
        }
    }
}

bool doTick(GameState * const gameState) {
    wclear(gameState->promptWin);
    mvwprintw(gameState->promptWin, 0, 0, "On tick %u", gameState->tick++);
    wrefresh(gameState->promptWin);

    for (unsigned int row = 0; row < gameState->logicalBoard.nrows; ++row) {
        for (unsigned int col = 0; col < gameState->logicalBoard.ncols; ++col) {
            handleTile(gameState, row, col);
        }
    }
    bool anyChanged = !tileChangeStackIsEmpty(&gameState->pendingChanges);
    if (anyChanged) {
        doChanges(gameState);
        wrefresh(gameState->physicalBoard);
    }
    return anyChanged;
}

void simulationLoop(GameState * const gameState) {
    curs_set(0);
    const int period_us = 1000000 / gameState->ticksPerSec;

    while (doTick(gameState)) {
        usleep(period_us);
    }
}

int main(const int argc, const char * const argv[]) {
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, true);

    int maxY, maxX;
    getmaxyx(stdscr, maxY, maxX);

    WINDOW *boardWinBox = derwin(stdscr, maxY - 1, maxX, 0, 0);
    WINDOW *boardWin = derwin(boardWinBox, maxY - 3, maxX - 2, 1, 1);
    WINDOW *promptWin = derwin(stdscr, 1, maxX, maxY - 1, 0);
    nodelay(boardWin, false);

    touchwin(stdscr);
    refresh();
    touchwin(boardWinBox);
    wrefresh(boardWinBox);

    box(boardWinBox, 0, 0);
    wrefresh(boardWinBox);

    GameState gameState;
    gameState.logicalBoard.ncols = maxX - 2;
    gameState.logicalBoard.nrows = maxY - 3;
    gameState.logicalBoard.tiles =
            (TileState *) calloc(gameState.logicalBoard.ncols * gameState.logicalBoard.nrows, sizeof(TileState));
    gameState.logicalBoard.nalive = 0;
    gameState.physicalBoard = boardWin;
    gameState.logicalCur.row = 0;
    gameState.logicalCur.col = 0;
    gameState.ticksPerSec = 2;
    gameState.promptWin = promptWin;
    gameState.tick = 0;
    gameState.pendingChanges.top = NULL;

    bool shouldContinue = setUpBoard(&gameState);
    if (!shouldContinue) {
        goto quit;
    }

    shouldContinue = getTicksPerSecond(&gameState);
    if (!shouldContinue) {
        goto quit;
    }

    simulationLoop(&gameState);

    wclear(gameState.promptWin);
    mvwprintw(gameState.promptWin, 0, 0, "Terminated after %d ticks.  Press 'q' to quit", gameState.tick + 1);
    wrefresh(gameState.promptWin);

    while (getch() != 'q') {}

quit:
    endwin();
    return 0;
}

