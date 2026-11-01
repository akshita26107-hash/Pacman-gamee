// pacman_win.cpp - Pac-Man for the Windows console (Dev-C++ / TDM-GCC)
#include <windows.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MW 19            /* maze width  (cells)  */
#define MH 21            /* maze height (cells)  */
#define OX 2             /* maze offset on screen */
#define OY 1
#define SW 64            /* screen buffer size   */
#define SH 24
#define WALL_ATTR 0x90   /* wall colour: bright blue background (try 0x10 for darker) */
#define PAC_DELAY 130    /* ms between Pac-Man moves */

/* # wall  . pellet  o power pellet  - ghost door  G ghost house  P Pac-Man start */
static const char *LEVEL_MAP[MH] = {
    "###################",
    "#........#........#",
    "#o##.###.#.###.##o#",
    "#.................#",
    "#.##.#.#####.#.##.#",
    "#....#...#...#....#",
    "####.### # ###.####",
    "####.#       #.####",
    "####.# ##-## #.####",
    "     . #GGG# .     ",
    "####.# ##### #.####",
    "####.#       #.####",
    "####.### # ###.####",
    "#........#........#",
    "#.##.###.#.###.##.#",
    "#o.#.....P.....#.o#",
    "##.#.#.#####.#.#.##",
    "#....#...#...#....#",
    "#.######.#.######.#",
    "#.................#",
    "###################"
};

enum { D_UP, D_LEFT, D_DOWN, D_RIGHT };
static const int DX[4] = {0, -1, 0, 1};
static const int DY[4] = {-1, 0, 1, 0};

enum { GH_NORMAL, GH_FRIGHT, GH_EATEN };
enum { ST_READY, ST_PLAY, ST_DYING, ST_CLEAR, ST_OVER };

typedef struct {
    int x, y, dir, mode, inHouse;
    DWORD last, releaseAt;
} Ghost;

/* ghost body colours: red, magenta, cyan, brown/orange */
static const WORD GHOST_ATTR[4] = {0xCF, 0xDF, 0xBF, 0x6F};
static const int SCX[4] = {MW - 2, 1, MW - 2, 1};   /* scatter corners */
static const int SCY[4] = {0, 0, MH - 1, MH - 1};
static const DWORD RELEASE[4] = {0, 2000, 5000, 8000};

static char maze[MH][MW + 1];
static int homeDist[MH][MW];       /* BFS distance to the ghost house */
static CHAR_INFO scr[SW * SH];
static Ghost gh[4];

static int px, py, pdir, want, mouth;
static int score, lives, level, pellets, state, ghostsEaten;
static DWORD pLast, frightUntil, stateUntil, phaseStart;

/* ---------- maze helpers ---------- */

static int walkable(int x, int y, int door) {
    if (y < 0 || y >= MH) return 0;
    x = (x + MW) % MW;                       /* horizontal tunnel wrap */
    char c = maze[y][x];
    if (c == '#') return 0;
    if (c == '-') return door;               /* only ghosts may use the door */
    return 1;
}

static void build_home_dist(void) {
    int qx[MH * MW], qy[MH * MW], head = 0, tail = 0;
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++) homeDist[y][x] = -1;
    qx[tail] = 9; qy[tail] = 9; tail++;
    homeDist[9][9] = 0;
    while (head < tail) {
        int x = qx[head], y = qy[head]; head++;
        for (int d = 0; d < 4; d++) {
            int nx = (x + DX[d] + MW) % MW, ny = y + DY[d];
            if (!walkable(nx, ny, 1) || homeDist[ny][nx] != -1) continue;
            homeDist[ny][nx] = homeDist[y][x] + 1;
            qx[tail] = nx; qy[tail] = ny; tail++;
        }
    }
}

static void load_level(void) {
    pellets = 0;
    for (int y = 0; y < MH; y++) {
        strcpy(maze[y], LEVEL_MAP[y]);
        for (int x = 0; x < MW; x++) {
            char c = maze[y][x];
            if (c == '.' || c == 'o') pellets++;
            else if (c == 'P' || c == 'G') maze[y][x] = ' ';
        }
    }
    build_home_dist();
}

static void reset_positions(void) {
    DWORD now = GetTickCount();
    px = 9; py = 15; pdir = D_LEFT; want = D_LEFT; mouth = 0;
    int sx[4] = {9, 9, 8, 10};
    int sy[4] = {7, 9, 9, 9};
    for (int i = 0; i < 4; i++) {
        gh[i].x = sx[i]; gh[i].y = sy[i];
        gh[i].dir = (i == 0) ? D_LEFT : D_UP;
        gh[i].mode = GH_NORMAL;
        gh[i].inHouse = (i == 0) ? 0 : 1;
        gh[i].last = now;
        gh[i].releaseAt = now;
    }
    frightUntil = 0;
    state = ST_READY;
    stateUntil = now + 2000;
}

/* ---------- ghost AI ---------- */

static int scatter_phase(void) {             /* 7 s scatter, 20 s chase, repeat */
    return ((GetTickCount() - phaseStart) % 27000) < 7000;
}

static void ghost_target(int i, int *tx, int *ty) {
    if (scatter_phase()) { *tx = SCX[i]; *ty = SCY[i]; return; }
    switch (i) {
    case 0:                                   /* red: chase Pac-Man directly */
        *tx = px; *ty = py; break;
    case 1:                                   /* pink: aim 4 cells ahead */
        *tx = px + 4 * DX[pdir]; *ty = py + 4 * DY[pdir]; break;
    case 2: {                                 /* cyan: mirror red around a point ahead */
        int ax = px + 2 * DX[pdir], ay = py + 2 * DY[pdir];
        *tx = 2 * ax - gh[0].x; *ty = 2 * ay - gh[0].y; break;
    }
    default: {                                /* orange: chase when far, retreat when close */
        int dx = px - gh[i].x, dy = py - gh[i].y;
        if (dx * dx + dy * dy > 64) { *tx = px; *ty = py; }
        else { *tx = SCX[i]; *ty = SCY[i]; }
    }
    }
}

static int ghost_delay(const Ghost *g) {
    int base = 170 - (level - 1) * 10;
    if (base < 80) base = 80;
    if (g->mode == GH_EATEN) return 50;
    if (g->mode == GH_FRIGHT) return base + 110;
    return base;
}

static void step_ghost(int i) {
    Ghost *g = &gh[i];
    int allowDoor = (g->inHouse || g->mode == GH_EATEN);
    int d = -1, k;

    if (g->mode == GH_EATEN) {                          /* eyes: shortest path home */
        int best = 1000000;
        for (k = 0; k < 4; k++) {
            int nx = (g->x + DX[k] + MW) % MW, ny = g->y + DY[k];
            if (!walkable(nx, ny, 1)) continue;
            if (homeDist[ny][nx] >= 0 && homeDist[ny][nx] < best) {
                best = homeDist[ny][nx]; d = k;
            }
        }
    } else if (g->mode == GH_FRIGHT && !g->inHouse) {   /* frightened: wander randomly */
        int opts[4], n = 0;
        for (k = 0; k < 4; k++) {
            if (k == (g->dir + 2) % 4) continue;
            int nx = (g->x + DX[k] + MW) % MW, ny = g->y + DY[k];
            if (walkable(nx, ny, 0)) opts[n++] = k;
        }
        if (n > 0) d = opts[rand() % n];
    } else {                                            /* greedy: minimise distance to target */
        int tx, ty;
        if (g->inHouse) { tx = 9; ty = 7; }             /* head for the exit */
        else ghost_target(i, &tx, &ty);
        long best = 1L << 30;
        for (k = 0; k < 4; k++) {
            if (k == (g->dir + 2) % 4) continue;        /* ghosts never reverse on their own */
            int nx = (g->x + DX[k] + MW) % MW, ny = g->y + DY[k];
            if (!walkable(nx, ny, allowDoor)) continue;
            long dd = (long)(nx - tx) * (nx - tx) + (long)(ny - ty) * (ny - ty);
            if (dd < best) { best = dd; d = k; }
        }
    }
    if (d < 0) d = (g->dir + 2) % 4;                    /* dead end: turn around */

    int nx = (g->x + DX[d] + MW) % MW, ny = g->y + DY[d];
    if (!walkable(nx, ny, allowDoor)) return;
    g->dir = d; g->x = nx; g->y = ny;

    if (g->mode == GH_EATEN && g->x == 9 && g->y == 9) {   /* eyes reached home: revive */
        g->mode = GH_NORMAL; g->inHouse = 1; g->dir = D_UP;
    } else if (g->inHouse && g->y <= 7) {
        g->inHouse = 0;                                    /* left the house */
    }
}

/* ---------- Pac-Man ---------- */

static void frighten(void) {
    int dur = 7000 - (level - 1) * 800;
    if (dur < 2500) dur = 2500;
    frightUntil = GetTickCount() + dur;
    ghostsEaten = 0;
    for (int i = 0; i < 4; i++)
        if (gh[i].mode != GH_EATEN) {
            gh[i].mode = GH_FRIGHT;
            if (!gh[i].inHouse) gh[i].dir = (gh[i].dir + 2) % 4;
        }
}

static void move_pacman(void) {
    int nx = (px + DX[want] + MW) % MW, ny = py + DY[want];
    if (walkable(nx, ny, 0)) pdir = want;               /* buffered turn */

    nx = (px + DX[pdir] + MW) % MW; ny = py + DY[pdir];
    if (walkable(nx, ny, 0)) {
        px = nx; py = ny; mouth ^= 1;
        char c = maze[py][px];
        if (c == '.') { maze[py][px] = ' '; score += 10; pellets--; }
        else if (c == 'o') { maze[py][px] = ' '; score += 50; pellets--; frighten(); }
    }
}

static void check_collisions(void) {
    for (int i = 0; i < 4; i++) {
        if (gh[i].x != px || gh[i].y != py) continue;
        if (gh[i].mode == GH_FRIGHT) {
            ghostsEaten++;
            score += 100 << ghostsEaten;                /* 200, 400, 800, 1600 */
            gh[i].mode = GH_EATEN;
        } else if (gh[i].mode == GH_NORMAL) {
            state = ST_DYING;
            stateUntil = GetTickCount() + 1500;
            lives--;
            return;
        }
    }
}

static void shift_timers(DWORD dt) {                    /* used after pausing */
    if (frightUntil) frightUntil += dt;
    stateUntil += dt; phaseStart += dt; pLast += dt;
    for (int i = 0; i < 4; i++) { gh[i].releaseAt += dt; gh[i].last += dt; }
}

/* ---------- drawing ---------- */

static void clear_buf(void) {
    for (int i = 0; i < SW * SH; i++) {
        scr[i].Char.AsciiChar = ' ';
        scr[i].Attributes = 0x07;
    }
}

static void put_str(int x, int y, const char *s, WORD attr) {
    for (; *s; s++, x++)
        if (x >= 0 && x < SW && y >= 0 && y < SH) {
            scr[y * SW + x].Char.AsciiChar = *s;
            scr[y * SW + x].Attributes = attr;
        }
}

static void present(HANDLE h) {
    COORD size = {SW, SH}, zero = {0, 0};
    SMALL_RECT region = {0, 0, SW - 1, SH - 1};
    WriteConsoleOutputA(h, scr, size, zero, &region);
}

static void draw(HANDLE h) {
    DWORD now = GetTickCount();
    char buf[32];
    int x, y, i;
    clear_buf();

    for (y = 0; y < MH; y++)
        for (x = 0; x < MW; x++) {
            char c = maze[y][x];
            int sx = OX + x * 2, sy = OY + y;
            if (c == '#')      put_str(sx, sy, "  ", WALL_ATTR);
            else if (c == '.') put_str(sx, sy, ". ", 0x0F);
            else if (c == 'o') put_str(sx, sy, "* ", ((now / 250) & 1) ? 0x0F : 0x0B);
            else if (c == '-') put_str(sx, sy, "--", 0x0D);
        }

    for (i = 0; i < 4; i++) {                           /* ghosts */
        WORD a;
        const char *s = "oo";
        if (gh[i].mode == GH_EATEN) {
            a = 0x0F;                                   /* just the eyes */
        } else if (gh[i].mode == GH_FRIGHT) {
            int flash = ((int)(frightUntil - now) < 2000) && ((now / 200) & 1);
            a = flash ? 0xCF : 0xF9;
            s = "~~";
        } else {
            a = GHOST_ATTR[i];
        }
        put_str(OX + gh[i].x * 2, OY + gh[i].y, s, a);
    }

    if (state != ST_DYING || ((now / 120) & 1)) {       /* Pac-Man (blinks when dying) */
        const char *s = "  ";
        if (mouth) switch (pdir) {
            case D_RIGHT: s = " <";   break;
            case D_LEFT:  s = "> ";   break;
            case D_UP:    s = "\\/";  break;
            default:      s = "/\\";  break;
        }
        put_str(OX + px * 2, OY + py, s, 0xE0);
    }

    int sx = OX + MW * 2 + 3;                           /* side panel */
    put_str(sx, OY, "PAC-MAN", 0x0E);
    sprintf(buf, "SCORE %d", score); put_str(sx, OY + 2, buf, 0x07);
    sprintf(buf, "LEVEL %d", level); put_str(sx, OY + 3, buf, 0x07);
    put_str(sx, OY + 4, "LIVES", 0x07);
    for (i = 0; i < lives; i++) put_str(sx + 6 + i * 3, OY + 4, "  ", 0xE0);
    put_str(sx, OY + 7,  "Arrows/WASD  move", 0x07);
    put_str(sx, OY + 8,  "P pause  Q quit",   0x07);
    put_str(sx, OY + 10, "Eat * to scare the", 0x07);
    put_str(sx, OY + 11, "ghosts, then eat",   0x07);
    put_str(sx, OY + 12, "them for bonus.",   0x07);

    if (state == ST_READY) put_str(OX + 16, OY + 11, "READY!", 0x0E);
    if (state == ST_CLEAR) put_str(OX + 14, OY + 11, "LEVEL CLEAR!", 0x0A);
    if (state == ST_OVER)  put_str(OX + 14, OY + 11, "GAME OVER", 0x4F);

    present(h);
}

/* ---------- main ---------- */

int main(void) {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO ci = {1, FALSE};
    SetConsoleCursorInfo(h, &ci);
    system("cls");
    srand((unsigned)time(NULL));

    score = 0; lives = 3; level = 1;
    load_level();
    reset_positions();
    int quit = 0;

    while (!quit) {
        while (_kbhit()) {
            int ch = _getch();
            if (ch == 0 || ch == 224) {                 /* arrow keys */
                ch = _getch();
                if (ch == 72) want = D_UP;
                else if (ch == 75) want = D_LEFT;
                else if (ch == 80) want = D_DOWN;
                else if (ch == 77) want = D_RIGHT;
            }
            else if (ch == 'w' || ch == 'W') want = D_UP;
            else if (ch == 'a' || ch == 'A') want = D_LEFT;
            else if (ch == 's' || ch == 'S') want = D_DOWN;
            else if (ch == 'd' || ch == 'D') want = D_RIGHT;
            else if (ch == 'q' || ch == 'Q') quit = 1;
            else if (ch == 'p' || ch == 'P') {
                DWORD t0 = GetTickCount();
                put_str(OX + 16, OY + 11, "PAUSED", 0x0F);
                present(h);
                while (1) {
                    int c = _getch();
                    if (c == 'p' || c == 'P') break;
                    if (c == 'q' || c == 'Q') { quit = 1; break; }
                }
                shift_timers(GetTickCount() - t0);
            }
        }

        DWORD now = GetTickCount();
        switch (state) {
        case ST_READY:
            if (now >= stateUntil) {
                state = ST_PLAY;
                pLast = now; phaseStart = now;
                for (int i = 0; i < 4; i++) {
                    gh[i].last = now;
                    gh[i].releaseAt = now + RELEASE[i];
                }
            }
            break;

        case ST_PLAY:
            if (frightUntil && now >= frightUntil) {    /* power-up wore off */
                frightUntil = 0;
                for (int i = 0; i < 4; i++)
                    if (gh[i].mode == GH_FRIGHT) gh[i].mode = GH_NORMAL;
            }
            if (now - pLast >= PAC_DELAY) {
                pLast = now;
                move_pacman();
                check_collisions();
            }
            for (int i = 0; i < 4 && state == ST_PLAY; i++) {
                if (now < gh[i].releaseAt) continue;
                if (now - gh[i].last >= (DWORD)ghost_delay(&gh[i])) {
                    gh[i].last = now;
                    step_ghost(i);
                    check_collisions();
                }
            }
            if (state == ST_PLAY && pellets == 0) {
                state = ST_CLEAR;
                stateUntil = now + 2000;
            }
            break;

        case ST_DYING:
            if (now >= stateUntil) {
                if (lives <= 0) state = ST_OVER;
                else reset_positions();
            }
            break;

        case ST_CLEAR:
            if (now >= stateUntil) {
                level++;
                load_level();
                reset_positions();
            }
            break;

        case ST_OVER:
            draw(h);
            while (_kbhit()) _getch();
            _getch();
            quit = 1;
            break;
        }

        if (!quit) draw(h);
        Sleep(10);
    }

    ci.bVisible = TRUE;
    SetConsoleCursorInfo(h, &ci);
    system("cls");
    printf("Final score: %d   Level reached: %d\n", score, level);
    system("pause");
    return 0;
}
