/*
 * Snake Game using Data Structures (C)
 *
 * Runs natively on Windows (console API, no extra libraries) and on
 * Linux / macOS (ncurses).
 *
 * Data structures used:
 *   - Queue (linked list) for the snake body:
 *       enqueue at the rear    = snake's new head
 *       dequeue from the front = snake's tail leaves
 *   - Queue (linked list) for the numbers eaten, printed in FIFO order
 *     at the end of the game.
 *   - 2D occupancy grid for O(1) collision checks / food placement.
 */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <ncurses.h>
#endif

#define MAX_BOARD_W 60
#define MAX_BOARD_H 24
#define MIN_BOARD_W 20
#define MIN_BOARD_H 10
#define BASE_DELAY_MS 130
#define MIN_DELAY_MS 55

/* ====================================================================== */
/*  Tiny terminal backend (Windows console  /  ncurses)                   */
/* ====================================================================== */

enum { K_NONE = -1, K_UP = 1000, K_DOWN, K_LEFT, K_RIGHT, K_ENTER, K_RESIZE };
enum { ATTR_TEXT = 0, ATTR_SNAKE, ATTR_FOOD, ATTR_BORDER, ATTR_TITLE, ATTR_COUNT };

static int g_rows, g_cols;

#ifdef _WIN32
/* ------------------------------ Windows ------------------------------- */

static HANDLE g_out = INVALID_HANDLE_VALUE;
static CHAR_INFO *g_buf = NULL;
static SMALL_RECT g_win;
static CONSOLE_CURSOR_INFO g_oldCursor;
static const WORD g_winAttr[ATTR_COUNT] = {
    FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,                         /* text   */
    FOREGROUND_GREEN | FOREGROUND_INTENSITY,                                     /* snake  */
    FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,                    /* food   */
    FOREGROUND_GREEN | FOREGROUND_BLUE,                                          /* border */
    FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY   /* title  */
};

static void term_clear(void) {
    if (!g_buf) return;
    for (int i = 0; i < g_rows * g_cols; i++) {
        g_buf[i].Char.AsciiChar = ' ';
        g_buf[i].Attributes = g_winAttr[ATTR_TEXT];
    }
}

/* Re-read the visible window size. Returns true if it changed. */
static bool term_update_size(void) {
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (!GetConsoleScreenBufferInfo(g_out, &info)) return false;
    int cols = info.srWindow.Right - info.srWindow.Left + 1;
    int rows = info.srWindow.Bottom - info.srWindow.Top + 1;
    g_win = info.srWindow;
    if (cols == g_cols && rows == g_rows && g_buf) return false;
    CHAR_INFO *nb = realloc(g_buf, (size_t)cols * rows * sizeof(CHAR_INFO));
    if (!nb) return false;
    g_buf = nb;
    g_cols = cols;
    g_rows = rows;
    term_clear();
    return true;
}

static bool term_init(void) {
    g_out = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (g_out == INVALID_HANDLE_VALUE || !GetConsoleScreenBufferInfo(g_out, &info)) {
        fprintf(stderr, "This game must be run inside a console window (cmd, PowerShell or Windows Terminal).\n");
        return false;
    }
    GetConsoleCursorInfo(g_out, &g_oldCursor);
    CONSOLE_CURSOR_INFO hidden = g_oldCursor;
    hidden.bVisible = FALSE;
    SetConsoleCursorInfo(g_out, &hidden);
    g_buf = NULL;
    g_rows = g_cols = 0;
    return term_update_size();
}

static void term_shutdown(void) {
    if (g_out == INVALID_HANDLE_VALUE) return;
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(g_out, &info)) {
        DWORD written;
        COORD home = { info.srWindow.Left, info.srWindow.Top };
        for (int r = 0; r < info.srWindow.Bottom - info.srWindow.Top + 1; r++) {
            COORD line = { info.srWindow.Left, (SHORT)(info.srWindow.Top + r) };
            FillConsoleOutputCharacterA(g_out, ' ', (DWORD)(info.srWindow.Right - info.srWindow.Left + 1), line, &written);
            FillConsoleOutputAttribute(g_out, info.wAttributes, (DWORD)(info.srWindow.Right - info.srWindow.Left + 1), line, &written);
        }
        SetConsoleCursorPosition(g_out, home);
    }
    SetConsoleCursorInfo(g_out, &g_oldCursor);
    free(g_buf);
    g_buf = NULL;
}

static void term_put(int row, int col, char ch, int attr) {
    if (!g_buf || row < 0 || col < 0 || row >= g_rows || col >= g_cols) return;
    g_buf[row * g_cols + col].Char.AsciiChar = ch;
    g_buf[row * g_cols + col].Attributes = g_winAttr[attr];
}

static void term_flush(void) {
    if (!g_buf) return;
    COORD size = { (SHORT)g_cols, (SHORT)g_rows };
    COORD zero = { 0, 0 };
    SMALL_RECT region = g_win;
    WriteConsoleOutputA(g_out, g_buf, size, zero, &region);
}

static int term_getkey(void) {
    if (term_update_size()) return K_RESIZE;
    if (_kbhit()) {
        int c = _getch();
        if (c == 0 || c == 0xE0) {
            int c2 = _getch();
            switch (c2) {
                case 72: return K_UP;
                case 80: return K_DOWN;
                case 75: return K_LEFT;
                case 77: return K_RIGHT;
                default: return 0; /* unused key */
            }
        }
        if (c == '\r' || c == '\n') return K_ENTER;
        return c;
    }
    Sleep(10);
    return K_NONE;
}

static long now_ms(void) { return (long)GetTickCount64(); }

#else
/* ------------------------------ ncurses ------------------------------- */

static bool g_colors = false;

static void term_clear(void) { erase(); }

static bool term_init(void) {
    if (!initscr()) return false;
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    timeout(10);
    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(1, COLOR_GREEN, -1);
        init_pair(2, COLOR_YELLOW, -1);
        init_pair(3, COLOR_CYAN, -1);
        init_pair(4, COLOR_WHITE, -1);
        g_colors = true;
    }
    getmaxyx(stdscr, g_rows, g_cols);
    return true;
}

static void term_shutdown(void) { endwin(); }

static void term_put(int row, int col, char ch, int attr) {
    if (row < 0 || col < 0 || row >= g_rows || col >= g_cols) return;
    chtype a = A_NORMAL;
    switch (attr) {
        case ATTR_TEXT:   a = g_colors ? COLOR_PAIR(4) : A_NORMAL; break;
        case ATTR_SNAKE:  a = g_colors ? COLOR_PAIR(1) : A_NORMAL; break;
        case ATTR_FOOD:   a = (g_colors ? COLOR_PAIR(2) : 0) | A_BOLD; break;
        case ATTR_BORDER: a = g_colors ? COLOR_PAIR(3) : A_NORMAL; break;
        case ATTR_TITLE:  a = A_BOLD; break;
    }
    mvaddch(row, col, (chtype)(unsigned char)ch | a);
}

static void term_flush(void) { refresh(); }

static int term_getkey(void) {
    int ch = getch();
    switch (ch) {
        case ERR: return K_NONE;
        case KEY_UP: return K_UP;
        case KEY_DOWN: return K_DOWN;
        case KEY_LEFT: return K_LEFT;
        case KEY_RIGHT: return K_RIGHT;
        case KEY_ENTER: case '\n': case '\r': return K_ENTER;
        case KEY_RESIZE: getmaxyx(stdscr, g_rows, g_cols); return K_RESIZE;
        default: return ch;
    }
}

static long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}
#endif

static void term_puts(int row, int col, const char *s, int attr) {
    for (; *s; s++, col++) term_put(row, col, *s, attr);
}

static void term_printf(int row, int col, int attr, const char *fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    term_puts(row, col, buf, attr);
}

/* ====================================================================== */
/*  Queue of snake cells                                                  */
/* ====================================================================== */

typedef struct Cell {
    int x, y;
    struct Cell *next;
} Cell;

typedef struct {
    Cell *front; /* snake tail */
    Cell *rear;  /* snake head */
    int size;
} CellQueue;

static void cq_init(CellQueue *q) { q->front = q->rear = NULL; q->size = 0; }

static bool cq_enqueue(CellQueue *q, int x, int y) {
    Cell *c = malloc(sizeof *c);
    if (!c) return false;
    c->x = x; c->y = y; c->next = NULL;
    if (q->rear) q->rear->next = c; else q->front = c;
    q->rear = c;
    q->size++;
    return true;
}

static bool cq_dequeue(CellQueue *q, int *x, int *y) {
    if (!q->front) return false;
    Cell *c = q->front;
    if (x) *x = c->x;
    if (y) *y = c->y;
    q->front = c->next;
    if (!q->front) q->rear = NULL;
    free(c);
    q->size--;
    return true;
}

static void cq_clear(CellQueue *q) {
    while (cq_dequeue(q, NULL, NULL)) {}
}

/* ====================================================================== */
/*  Queue of eaten numbers                                                */
/* ====================================================================== */

typedef struct Num {
    int value;
    struct Num *next;
} Num;

typedef struct {
    Num *front, *rear;
    int size;
} NumQueue;

static void nq_init(NumQueue *q) { q->front = q->rear = NULL; q->size = 0; }

static bool nq_enqueue(NumQueue *q, int v) {
    Num *n = malloc(sizeof *n);
    if (!n) return false;
    n->value = v; n->next = NULL;
    if (q->rear) q->rear->next = n; else q->front = n;
    q->rear = n;
    q->size++;
    return true;
}

static bool nq_dequeue(NumQueue *q, int *v) {
    if (!q->front) return false;
    Num *n = q->front;
    if (v) *v = n->value;
    q->front = n->next;
    if (!q->front) q->rear = NULL;
    free(n);
    q->size--;
    return true;
}

static void nq_clear(NumQueue *q) {
    while (nq_dequeue(q, NULL)) {}
}

/* ====================================================================== */
/*  Game state and logic                                                  */
/* ====================================================================== */

typedef enum { S_MENU, S_PLAY, S_PAUSE, S_OVER, S_WIN } State;
typedef enum { D_UP, D_DOWN, D_LEFT, D_RIGHT } Dir;

typedef struct {
    int w, h;                /* board size in cells */
    bool *occupied;          /* w*h grid, true where the snake is */
    CellQueue snake;
    NumQueue eaten;
    int foodX, foodY, foodValue;
    Dir dir;                 /* direction used by the last move */
    Dir nextDir;             /* direction requested for the next move */
    int score;
    int highScore;
    State state;
} Game;

static bool *cell_at(Game *g, int x, int y) { return &g->occupied[y * g->w + x]; }

static bool place_food(Game *g) {
    int free_cells = g->w * g->h - g->snake.size;
    if (free_cells <= 0) return false;
    int pick = rand() % free_cells;
    for (int y = 0; y < g->h; y++) {
        for (int x = 0; x < g->w; x++) {
            if (*cell_at(g, x, y)) continue;
            if (pick-- == 0) {
                g->foodX = x;
                g->foodY = y;
                g->foodValue = 1 + rand() % 9;
                return true;
            }
        }
    }
    return false;
}

static bool game_reset(Game *g) {
    cq_clear(&g->snake);
    nq_clear(&g->eaten);
    memset(g->occupied, 0, (size_t)g->w * g->h * sizeof(bool));
    int sx = g->w / 2, sy = g->h / 2;
    /* Start with a length-3 snake heading right: tail ... head */
    for (int i = -2; i <= 0; i++) {
        if (!cq_enqueue(&g->snake, sx + i, sy)) return false;
        *cell_at(g, sx + i, sy) = true;
    }
    g->dir = g->nextDir = D_RIGHT;
    g->score = 0;
    if (!place_food(g)) return false;
    return true;
}

static void game_free(Game *g) {
    cq_clear(&g->snake);
    nq_clear(&g->eaten);
    free(g->occupied);
    g->occupied = NULL;
}

/* Advance the snake one step. Updates g->state on death or win. */
static void game_step(Game *g) {
    g->dir = g->nextDir;
    int dx = 0, dy = 0;
    switch (g->dir) {
        case D_UP: dy = -1; break;
        case D_DOWN: dy = 1; break;
        case D_LEFT: dx = -1; break;
        case D_RIGHT: dx = 1; break;
    }
    int nx = g->snake.rear->x + dx;
    int ny = g->snake.rear->y + dy;

    if (nx < 0 || nx >= g->w || ny < 0 || ny >= g->h) {
        g->state = S_OVER;
        return;
    }

    bool eating = (nx == g->foodX && ny == g->foodY);

    /* Moving into the cell the tail is about to vacate is legal. */
    if (*cell_at(g, nx, ny)) {
        bool tailLeaves = !eating && g->snake.front->x == nx && g->snake.front->y == ny;
        if (!tailLeaves) {
            g->state = S_OVER;
            return;
        }
    }

    if (!eating) {
        int tx = 0, ty = 0;
        if (cq_dequeue(&g->snake, &tx, &ty)) *cell_at(g, tx, ty) = false;
    }

    if (!cq_enqueue(&g->snake, nx, ny)) {
        g->state = S_OVER;
        return;
    }
    *cell_at(g, nx, ny) = true;

    if (eating) {
        g->score += g->foodValue * 10;
        if (g->score > g->highScore) g->highScore = g->score;
        nq_enqueue(&g->eaten, g->foodValue);
        if (!place_food(g)) g->state = S_WIN;
    }
}

static bool is_opposite(Dir a, Dir b) {
    return (a == D_UP && b == D_DOWN) || (a == D_DOWN && b == D_UP) ||
           (a == D_LEFT && b == D_RIGHT) || (a == D_RIGHT && b == D_LEFT);
}

static int step_delay_ms(const Game *g) {
    int d = BASE_DELAY_MS - g->eaten.size * 3;
    if (d < MIN_DELAY_MS) d = MIN_DELAY_MS;
    /* Terminal cells are ~2x taller than wide; slow vertical moves to match. */
    if (g->dir == D_UP || g->dir == D_DOWN) d = d * 9 / 5;
    return d;
}

/* ====================================================================== */
/*  Drawing                                                               */
/* ====================================================================== */

static void center_text_attr(int row, const char *s, int attr) {
    int col = (g_cols - (int)strlen(s)) / 2;
    if (col < 0) col = 0;
    term_puts(row, col, s, attr);
}

static void center_text(int row, const char *s) { center_text_attr(row, s, ATTR_TEXT); }

static void draw_eaten_list(const Game *g, int startRow, int maxRows) {
    char line[256];
    int row = startRow;
    int maxCols = g_cols - 4 < 250 ? g_cols - 4 : 250;
    if (maxCols < 4) return;
    line[0] = '\0';
    int len = 0;
    int shown = 0;
    for (const Num *n = g->eaten.front; n; n = n->next) {
        char tok[8];
        int tl = snprintf(tok, sizeof tok, "%d ", n->value);
        if (len + tl > maxCols) {
            if (row - startRow + 1 >= maxRows) break;
            center_text(row++, line);
            line[0] = '\0';
            len = 0;
        }
        memcpy(line + len, tok, (size_t)tl + 1);
        len += tl;
        shown++;
    }
    if (len > 0 && row - startRow < maxRows) center_text(row++, line);
    if (shown < g->eaten.size && row - startRow < maxRows) {
        char more[48];
        snprintf(more, sizeof more, "(+%d more, printed on exit)", g->eaten.size - shown);
        center_text(row, more);
    }
}

static void draw(Game *g) {
    term_clear();
    int needRows = g->h + 4, needCols = g->w + 2;
    if (g_rows < needRows || g_cols < needCols) {
        center_text(g_rows / 2, "Terminal too small - enlarge it or press Q");
        term_flush();
        return;
    }
    int offY = 2 + (g_rows - needRows) / 2;
    int offX = 1 + (g_cols - needCols) / 2;

    term_printf(offY - 2, offX - 1, ATTR_TEXT, "Score: %d   High: %d   Length: %d",
                g->score, g->highScore, g->snake.size);

    /* Border */
    term_put(offY - 1, offX - 1, '+', ATTR_BORDER);
    term_put(offY - 1, offX + g->w, '+', ATTR_BORDER);
    term_put(offY + g->h, offX - 1, '+', ATTR_BORDER);
    term_put(offY + g->h, offX + g->w, '+', ATTR_BORDER);
    for (int x = 0; x < g->w; x++) {
        term_put(offY - 1, offX + x, '-', ATTR_BORDER);
        term_put(offY + g->h, offX + x, '-', ATTR_BORDER);
    }
    for (int y = 0; y < g->h; y++) {
        term_put(offY + y, offX - 1, '|', ATTR_BORDER);
        term_put(offY + y, offX + g->w, '|', ATTR_BORDER);
    }

    /* Snake: body 'o', head '@' */
    for (const Cell *c = g->snake.front; c; c = c->next)
        term_put(offY + c->y, offX + c->x, c == g->snake.rear ? '@' : 'o', ATTR_SNAKE);

    /* Food shows the number you will collect */
    term_put(offY + g->foodY, offX + g->foodX, (char)('0' + g->foodValue), ATTR_FOOD);

    int mid = g_rows / 2;
    if (g->state == S_PAUSE) {
        center_text(mid, " PAUSED - press P to continue ");
    } else if (g->state == S_OVER || g->state == S_WIN) {
        char buf[64];
        center_text(mid - 3, g->state == S_WIN ? " YOU WIN! The board is full. " : " GAME OVER ");
        snprintf(buf, sizeof buf, " Final score: %d ", g->score);
        center_text(mid - 2, buf);
        center_text(mid - 1, " Numbers eaten (in order): ");
        if (g->eaten.size == 0) center_text(mid, "(none)");
        else draw_eaten_list(g, mid, 3);
        center_text(mid + 4, " R: play again    Q: quit ");
    }
    term_flush();
}

static void show_menu(void) {
    term_clear();
    int r = g_rows / 2 - 4;
    center_text_attr(r, "THE SNAKE GAME!", ATTR_TITLE);
    center_text(r + 2, "Arrow keys / WASD: move the snake");
    center_text(r + 3, "P: pause     Q: quit");
    center_text(r + 4, "Eat the numbers - each one is stored in a queue");
    center_text(r + 5, "and printed when the game ends.");
    center_text(r + 7, "Press ENTER to start");
    term_flush();
}

/* ====================================================================== */
/*  Main                                                                  */
/* ====================================================================== */

static void print_eaten_summary(Game *g) {
    printf("Final score: %d\n", g->score);
    printf("Numbers eaten (%d): ", g->eaten.size);
    if (g->eaten.size == 0) {
        printf("none");
    } else {
        int v;
        bool first = true;
        while (nq_dequeue(&g->eaten, &v)) { /* dequeue in FIFO order */
            printf(first ? "%d" : " %d", v);
            first = false;
        }
    }
    printf("\n");
}

int main(void) {
    srand((unsigned)time(NULL));

    if (!term_init()) return 1;

    Game g;
    memset(&g, 0, sizeof g);
    g.w = g_cols - 2 > MAX_BOARD_W ? MAX_BOARD_W : g_cols - 2;
    g.h = g_rows - 4 > MAX_BOARD_H ? MAX_BOARD_H : g_rows - 4;
    if (g.w < MIN_BOARD_W || g.h < MIN_BOARD_H) {
        int c = g_cols, r = g_rows;
        term_shutdown();
        fprintf(stderr, "Terminal too small (need at least %dx%d, have %dx%d).\n",
                MIN_BOARD_W + 2, MIN_BOARD_H + 4, c, r);
        return 1;
    }
    g.occupied = calloc((size_t)g.w * g.h, sizeof(bool));
    if (!g.occupied) {
        term_shutdown();
        fprintf(stderr, "Out of memory.\n");
        return 1;
    }
    cq_init(&g.snake);
    nq_init(&g.eaten);
    if (!game_reset(&g)) {
        term_shutdown();
        fprintf(stderr, "Failed to initialise the game.\n");
        game_free(&g);
        return 1;
    }
    g.state = S_MENU;

    bool quit = false;
    long lastStep = now_ms();
    show_menu();

    while (!quit) {
        int ch;
        while ((ch = term_getkey()) != K_NONE) {
            if (ch == 'q' || ch == 'Q') { quit = true; break; }
            if (ch == K_RESIZE) {
                if (g.state == S_MENU) show_menu();
                continue;
            }
            switch (g.state) {
                case S_MENU:
                    if (ch == K_ENTER || ch == ' ') {
                        g.state = S_PLAY;
                        lastStep = now_ms();
                    }
                    break;
                case S_PLAY: {
                    Dir want = g.nextDir;
                    bool isDir = true;
                    switch (ch) {
                        case K_UP: case 'w': case 'W': want = D_UP; break;
                        case K_DOWN: case 's': case 'S': want = D_DOWN; break;
                        case K_LEFT: case 'a': case 'A': want = D_LEFT; break;
                        case K_RIGHT: case 'd': case 'D': want = D_RIGHT; break;
                        default: isDir = false; break;
                    }
                    if (isDir && !is_opposite(want, g.dir)) g.nextDir = want;
                    if (ch == 'p' || ch == 'P') g.state = S_PAUSE;
                    break;
                }
                case S_PAUSE:
                    if (ch == 'p' || ch == 'P') {
                        g.state = S_PLAY;
                        lastStep = now_ms();
                    }
                    break;
                case S_OVER:
                case S_WIN:
                    if (ch == 'r' || ch == 'R' || ch == K_ENTER) {
                        if (!game_reset(&g)) { quit = true; break; }
                        g.state = S_PLAY;
                        lastStep = now_ms();
                    }
                    break;
            }
        }
        if (quit) break;

        if (g.state == S_PLAY) {
            long t = now_ms();
            if (t - lastStep >= step_delay_ms(&g)) {
                lastStep = t;
                game_step(&g);
            }
        }
        if (g.state != S_MENU) draw(&g);
    }

    term_shutdown();
    print_eaten_summary(&g);
    game_free(&g);
    return 0;
}
