/*
 * Star Bean Adventure - C++ Console Homework Edition
 * ---------------------------------------------------
 * A compact but complete console game inspired by the web version.
 *
 * Features:
 *   - Random maze with loops (different every level)
 *   - Collect all beans to open the exit; clear 3 levels to win
 *   - Patrol / chase enemies with BFS pathfinding
 *   - Power bean, star shield, stun bean, trap
 *   - Attack (J) and dash (K) skills with cooldowns
 *   - Safe house: stay 3 seconds to become hidden from enemies
 *   - Score, lives, level progression, pause and restart
 *
 * Build (any C++ compiler):
 *   g++ StarBeanAdventure.cpp -o StarBeanAdventure.exe
 *   or just create a new Console Application project and add this file.
 *
 * Controls:
 *   WASD / Arrow keys : move (hold to keep moving)
 *   J : stun strike   K : dash   P : pause   Q : quit   R : restart
 *
 * Note: the interface uses ASCII only, so it works in any Chinese Windows
 * console without garbled text, no matter GBK or UTF-8 source encoding.
 */

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <conio.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <queue>
#include <sstream>
#include <stack>
#include <string>
#include <vector>

using namespace std;

/* ------------------------------ constants ------------------------------ */

const int MAP_W = 39;
const int MAP_H = 21;
const int LINE_W = 90;            // wide enough to overwrite old console text
const int TICK_MS = 70;
const int MAX_LEVEL = 3;          // change to 999 if you want more levels
const int MAX_LIVES = 3;
const int DIR_NONE = 0;
const int DIR_UP = 1;
const int DIR_DOWN = 2;
const int DIR_LEFT = 3;
const int DIR_RIGHT = 4;

const int DX[5] = {0, 0, 0, -1, 1};
const int DY[5] = {0, -1, 1, 0, 0};

int toTicks(int seconds) { return seconds * 1000 / TICK_MS; }
int toSeconds(int ticks) { return (ticks * TICK_MS + 999) / 1000; }

string secLabel(int ticks)
{
    if (ticks <= 0) return "READY";
    ostringstream os;
    os << toSeconds(ticks) << "s";
    return os.str();
}

int rnd(int n) { return n <= 0 ? 0 : rand() % n; }

/* ------------------------------ data types ----------------------------- */

struct Pos
{
    int x;
    int y;

    Pos() : x(0), y(0) {}
    Pos(int xx, int yy) : x(xx), y(yy) {}

    bool eq(const Pos& other) const { return x == other.x && y == other.y; }
};

bool operator==(const Pos& a, const Pos& b) { return a.eq(b); }
bool operator!=(const Pos& a, const Pos& b) { return !a.eq(b); }

int manhattan(const Pos& a, const Pos& b)
{
    return abs(a.x - b.x) + abs(a.y - b.y);
}

struct Player
{
    Pos pos;
    int lives;
    int score;
    int power;       // enemies turn frightened
    int shield;      // star bean: blocks one hit
    int invincible;  // short blink time after being hurt
    int dash;
    int dashCd;
    int attackCd;
    int safeHold;
    int hidden;

    Player()
    {
        pos = Pos(1, 1);
        lives = MAX_LIVES;
        score = 0;
        power = shield = invincible = 0;
        dash = dashCd = attackCd = 0;
        safeHold = hidden = 0;
    }
};

struct Enemy
{
    Pos pos;
    Pos home;
    int dir;
    int stun;
    int respawn;

    Enemy() : dir(DIR_UP), stun(0), respawn(0) {}
};

enum GameState
{
    ST_INTRO = 0,
    ST_PLAY = 1,
    ST_CLEAR = 2,
    ST_OVER = 3,
    ST_WIN = 4
};

void shufflePositions(vector<Pos>& v)
{
    for (int i = (int)v.size() - 1; i > 0; --i)
    {
        int j = rnd(i + 1);
        Pos tmp = v[i];
        v[i] = v[j];
        v[j] = tmp;
    }
}

void shuffleInts(vector<int>& v)
{
    for (int i = (int)v.size() - 1; i > 0; --i)
    {
        int j = rnd(i + 1);
        int tmp = v[i];
        v[i] = v[j];
        v[j] = tmp;
    }
}

/* ------------------------------- the game ------------------------------ */

class Game
{
public:
    explicit Game(HANDLE console) : hOut(console)
    {
        hasCursorInfo = (GetConsoleCursorInfo(hOut, &oldCursor) != 0);
        state = ST_INTRO;
        level = 1;
        beansLeft = 0;
        ticks = 0;
        paused = 0;
        wantQuit = 0;
        toastTicks = 0;
        prevJ = prevK = prevP = prevQ = false;
        heldDirs.clear();
    }

    bool quit() const { return wantQuit != 0; }

    void restoreCursor()
    {
        if (hasCursorInfo)
        {
            SetConsoleCursorInfo(hOut, &oldCursor);
        }
    }

    void handleInput()
    {
        if (state != ST_PLAY)
        {
            handleMenuInput();
            return;
        }

        bool qPressed = edgeKey('Q', prevQ);
        if (qPressed)
        {
            wantQuit = 1;
            return;
        }

        bool pPressed = edgeKey('P', prevP);
        if (pPressed)
        {
            paused = paused ? 0 : 1;
            heldDirs.clear();
            if (paused)
            {
                setToast("PAUSED");
            }
        }

        if (paused)
        {
            return;
        }

        updateHeldDirections();

        if (edgeKey('J', prevJ))
        {
            useAttack();
        }
        if (edgeKey('K', prevK))
        {
            useDash();
        }
    }

    void update()
    {
        if (state != ST_PLAY || paused)
        {
            if (toastTicks > 0) --toastTicks;
            return;
        }

        ++ticks;
        if (toastTicks > 0) --toastTicks;
        if (player.invincible > 0) --player.invincible;
        if (player.power > 0) --player.power;
        if (player.shield > 0) --player.shield;
        if (player.dash > 0) --player.dash;
        if (player.dashCd > 0) --player.dashCd;
        if (player.attackCd > 0) --player.attackCd;

        updateSafeHouse();

        int dir = heldDirs.empty() ? DIR_NONE : heldDirs.back();
        if (dir != DIR_NONE)
        {
            movePlayer(dir);
        }

        if (state != ST_PLAY) return;

        for (size_t i = 0; i < enemies.size(); ++i)
        {
            updateEnemy(enemies[i]);
        }

        for (size_t i = 0; i < enemies.size(); ++i)
        {
            Enemy& e = enemies[i];
            if (e.respawn > 0 || !e.pos.eq(player.pos)) continue;

            if (player.power > 0)
            {
                defeatEnemy(e);
                setToast("ENEMY DEFEATED  +200");
            }
            else if (player.hidden <= 0 && player.shield <= 0 && player.invincible <= 0)
            {
                loseLife("CAUGHT BY AN ENEMY");
            }
        }

        if (state != ST_PLAY) return;

        if (beansLeft == 0 && terrain[player.pos.y][player.pos.x] == 'E')
        {
            player.score += 1000 + player.lives * 200;
            if (level >= MAX_LEVEL)
            {
                state = ST_WIN;
            }
            else
            {
                state = ST_CLEAR;
            }
            heldDirs.clear();
        }
    }

    void render()
    {
        gotoHome();

        if (state == ST_INTRO)
        {
            renderIntro();
            return;
        }

        printPadded("=== STAR BEAN ADVENTURE  (C++ Console Edition) ===");

        ostringstream hud1;
        hud1 << "Level " << level << "/" << MAX_LEVEL
             << "    Score " << player.score
             << "    Lives " << livesBar()
             << "    Beans left " << beansLeft;
        printPadded(hud1.str());

        ostringstream hud2;
        hud2 << "Power " << toSeconds(player.power) << "s"
             << "   Shield " << toSeconds(player.shield) << "s"
             << "   Dash " << secLabel(player.dashCd)
             << "   Attack " << secLabel(player.attackCd)
             << "   Hidden " << toSeconds(player.hidden) << "s";
        printPadded(hud2.str());

        printPadded(string(MAP_W, '-'));

        for (int y = 0; y < MAP_H; ++y)
        {
            string line;
            line.reserve(MAP_W);
            for (int x = 0; x < MAP_W; ++x)
            {
                char ch = terrain[y][x];
                if (items[y][x] != ' ' && ch == ' ')
                {
                    ch = items[y][x];
                }
                line += ch;
            }

            for (size_t i = 0; i < enemies.size(); ++i)
            {
                Enemy& e = enemies[i];
                if (e.respawn == 0 && e.pos.y == y &&
                    e.pos.x >= 0 && e.pos.x < MAP_W)
                {
                    line[e.pos.x] = (e.stun > 0 ? 'x' : 'M');
                }
            }

            if (player.pos.y == y &&
                player.pos.x >= 0 && player.pos.x < MAP_W)
            {
                line[player.pos.x] = '@';
            }

            printPadded(line);
        }

        printPadded(string(MAP_W, '-'));
        printPadded("Move: WASD / Arrows   J: Stun Strike   K: Dash   P: Pause   Q: Quit");
        printPadded("Legend: @ you   M enemy   . bean   P power   * star   T stun   ^ trap   H safe house   E exit");

        if (toastTicks > 0 && !toast.empty())
        {
            printPadded(">> " + toast);
        }

        if (paused && state == ST_PLAY)
        {
            printPadded(">>> PAUSED - press P to continue <<<");
        }
        if (state == ST_CLEAR)
        {
            printPadded(">>> LEVEL CLEAR! Press ENTER for the next level. <<<");
        }
        if (state == ST_OVER)
        {
            printPadded(">>> GAME OVER. Press R to restart, Q to quit. <<<");
        }
        if (state == ST_WIN)
        {
            printPadded(">>> YOU WIN! Press R to play again, Q to quit. <<<");
        }
        cout.flush();
    }

private:
    HANDLE hOut;
    CONSOLE_CURSOR_INFO oldCursor;
    bool hasCursorInfo;

    vector<string> terrain;
    vector<string> items;
    vector<Enemy> enemies;
    vector<int> heldDirs;

    Player player;
    GameState state;
    int level;
    int beansLeft;
    int ticks;
    int paused;
    int wantQuit;
    string toast;
    int toastTicks;
    bool prevJ;
    bool prevK;
    bool prevP;
    bool prevQ;

    /* -------------------------- console helpers ------------------------- */

    void gotoHome()
    {
        COORD c;
        c.X = 0;
        c.Y = 0;
        SetConsoleCursorPosition(hOut, c);
    }

    void printPadded(const string& s)
    {
        cout << s;
        int used = (int)s.length();
        if (used < LINE_W) cout << string(LINE_W - used, ' ');
        cout << "\n";
    }

    bool keyDown(int vk) const
    {
        return (GetAsyncKeyState(vk) & 0x8000) != 0;
    }

    bool edgeKey(int vk, bool& previous)
    {
        bool now = keyDown(vk);
        bool edge = now && !previous;
        previous = now;
        return edge;
    }

    string livesBar() const
    {
        int n = player.lives;
        if (n < 0) n = 0;
        if (n > MAX_LIVES) n = MAX_LIVES;
        return string(n, '*') + string(MAX_LIVES - n, '.');
    }

    void setToast(const string& message)
    {
        toast = message;
        toastTicks = toTicks(3);
    }

    /* ----------------------------- input -------------------------------- */

    void updateHeldDirections()
    {
        const int vks[8] = {VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT,
                            'W', 'S', 'A', 'D'};
        const int dirs[8] = {DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT,
                             DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT};

        for (int i = 0; i < 8; ++i)
        {
            bool down = keyDown(vks[i]);
            vector<int>::iterator it = find(heldDirs.begin(), heldDirs.end(), dirs[i]);
            bool held = (it != heldDirs.end());

            if (down && !held)
            {
                heldDirs.push_back(dirs[i]);
            }
            else if (!down && held)
            {
                heldDirs.erase(it);
            }
        }
    }

    void handleMenuInput()
    {
        if (!_kbhit()) return;
        int c = _getch();

        if (c == 'q' || c == 'Q' || c == 27)
        {
            wantQuit = 1;
            return;
        }

        if (state == ST_INTRO && (c == 13 || c == 32))
        {
            startGame();
        }
        else if (state == ST_CLEAR && c == 13)
        {
            generate(level + 1);
            state = ST_PLAY;
            setToast("NEW LEVEL - KEEP GOING!");
        }
        else if ((state == ST_OVER || state == ST_WIN) && (c == 'r' || c == 'R'))
        {
            startGame();
        }
    }

    /* --------------------------- maze / level --------------------------- */

    void generate(int lv)
    {
        level = lv;
        srand((unsigned)time(NULL) + (unsigned)(lv * 7919));

        terrain.assign(MAP_H, string(MAP_W, '#'));
        items.assign(MAP_H, string(MAP_W, ' '));
        enemies.clear();
        heldDirs.clear();
        ticks = 0;
        paused = 0;

        // Randomized depth-first search on odd grid cells.
        vector<vector<bool> > visited(MAP_H / 2, vector<bool>(MAP_W / 2, false));
        stack<Pos> st;
        terrain[1][1] = ' ';
        visited[0][0] = true;
        st.push(Pos(1, 1));

        const int dx[4] = {0, 0, -1, 1};
        const int dy[4] = {-1, 1, 0, 0};

        while (!st.empty())
        {
            Pos cur = st.top();
            vector<int> dirs;
            for (int d = 0; d < 4; ++d) dirs.push_back(d);
            shuffleInts(dirs);

            bool advanced = false;
            for (size_t k = 0; k < dirs.size(); ++k)
            {
                int d = dirs[k];
                int nx = cur.x + dx[d] * 2;
                int ny = cur.y + dy[d] * 2;
                if (nx <= 0 || nx >= MAP_W - 1 || ny <= 0 || ny >= MAP_H - 1)
                {
                    continue;
                }
                int cx = nx / 2;
                int cy = ny / 2;
                if (visited[cy][cx]) continue;

                terrain[cur.y + dy[d]][cur.x + dx[d]] = ' ';
                terrain[ny][nx] = ' ';
                visited[cy][cx] = true;
                st.push(Pos(nx, ny));
                advanced = true;
                break;
            }
            if (!advanced) st.pop();
        }

        // Knock down a few walls to create loops, so chasing is not trivial.
        for (int y = 2; y < MAP_H - 1; ++y)
        {
            for (int x = 2; x < MAP_W - 1; ++x)
            {
                if (terrain[y][x] != '#') continue;
                bool horizontal = terrain[y][x - 1] == ' ' && terrain[y][x + 1] == ' ';
                bool vertical = terrain[y - 1][x] == ' ' && terrain[y + 1][x] == ' ';
                if ((horizontal || vertical) && rnd(100) < 10)
                {
                    terrain[y][x] = ' ';
                }
            }
        }

        vector<Pos> floors;
        for (int y = 1; y < MAP_H - 1; ++y)
        {
            for (int x = 1; x < MAP_W - 1; ++x)
            {
                if (terrain[y][x] != ' ') continue;
                Pos here(x, y);
                if (manhattan(here, Pos(1, 1)) <= 3) continue;
                if (manhattan(here, Pos(MAP_W - 2, MAP_H - 2)) <= 2) continue;
                floors.push_back(here);
            }
        }
        shufflePositions(floors);

        // Four safe houses spread across the maze.
        vector<Pos> safeHouses;
        for (size_t i = 0; i < floors.size() && safeHouses.size() < 4; ++i)
        {
            Pos p = floors[i];
            if (manhattan(p, Pos(1, 1)) < 8) continue;
            bool farEnough = true;
            for (size_t j = 0; j < safeHouses.size(); ++j)
            {
                if (manhattan(p, safeHouses[j]) < 6)
                {
                    farEnough = false;
                    break;
                }
            }
            if (farEnough)
            {
                terrain[p.y][p.x] = 'H';
                safeHouses.push_back(p);
            }
        }

        terrain[MAP_H - 2][MAP_W - 2] = 'E';

        // Beans: keep the level reasonably short and homework-friendly.
        int target = min((int)floors.size() - 8, 55 + lv * 12);
        if (target < 20) target = (int)floors.size();
        int placed = 0;
        for (size_t i = 0; i < floors.size() && placed < target; ++i)
        {
            Pos p = floors[i];
            if (terrain[p.y][p.x] != ' ') continue;

            int r = rnd(100);
            char bean = '.';
            if (r < 6) bean = 'P';
            else if (r < 10) bean = '*';
            else if (r < 15) bean = 'T';
            else if (r < 18) bean = '^';

            items[p.y][p.x] = bean;
            ++placed;
        }
        beansLeft = placed;

        // Enemies spawn far away from the player.
        int enemyCount = min(2 + lv, 5);
        for (int k = 0; k < enemyCount; ++k)
        {
            int index = (int)floors.size() - 1 - k * 3;
            if (index < 0) index = rnd((int)floors.size());
            Pos home = floors[index];
            if (terrain[home.y][home.x] != ' ')
            {
                for (size_t t = 0; t < floors.size(); ++t)
                {
                    Pos candidate = floors[t];
                    if (terrain[candidate.y][candidate.x] == ' ')
                    {
                        home = candidate;
                        break;
                    }
                }
            }
            Enemy e;
            e.pos = home;
            e.home = home;
            e.dir = DIR_UP + rnd(4);
            enemies.push_back(e);
        }

        player.pos = Pos(1, 1);
        player.power = 0;
        player.shield = 0;
        player.invincible = toTicks(2);
        player.dash = 0;
        player.dashCd = 0;
        player.attackCd = 0;
        player.safeHold = 0;
        player.hidden = 0;
        toast.clear();
        toastTicks = 0;
    }

    void startGame()
    {
        player.lives = MAX_LIVES;
        player.score = 0;
        generate(1);
        state = ST_PLAY;
        setToast("COLLECT ALL BEANS, THEN REACH THE EXIT");
    }

    /* ---------------------------- movement ------------------------------ */

    bool walkable(int x, int y) const
    {
        return x > 0 && x < MAP_W - 1 && y > 0 && y < MAP_H - 1 &&
               terrain[y][x] != '#';
    }

    void movePlayer(int dir)
    {
        int nx = player.pos.x + DX[dir];
        int ny = player.pos.y + DY[dir];
        if (!walkable(nx, ny)) return;

        player.pos = Pos(nx, ny);
        collectItem();
        if (state != ST_PLAY) return;

        // Dash covers one extra cell per tick.
        if (player.dash > 0)
        {
            int nx2 = player.pos.x + DX[dir];
            int ny2 = player.pos.y + DY[dir];
            if (walkable(nx2, ny2))
            {
                player.pos = Pos(nx2, ny2);
                collectItem();
            }
        }
    }

    void collectItem()
    {
        int x = player.pos.x;
        int y = player.pos.y;
        char bean = items[y][x];
        if (bean == ' ') return;

        items[y][x] = ' ';
        if (beansLeft > 0) --beansLeft;

        switch (bean)
        {
        case 'P':
            player.power = toTicks(6);
            player.score += 50;
            setToast("POWER BEAN - ENEMIES ARE AFRAID!");
            break;
        case '*':
            player.shield = toTicks(6);
            player.score += 100;
            setToast("STAR SHIELD - ONE HIT BLOCKED");
            break;
        case 'T':
            for (size_t i = 0; i < enemies.size(); ++i)
            {
                if (enemies[i].respawn == 0) enemies[i].stun = toTicks(3);
            }
            player.score += 50;
            setToast("STUN BEAN - ENEMIES FROZEN");
            break;
        case '^':
            setToast("OUCH! A TRAP");
            loseLife("TRAP TRIGGERED");
            break;
        default:
            player.score += 10;
            break;
        }
    }

    void loseLife(const string& reason)
    {
        if (player.invincible > 0 || player.shield > 0) return;

        --player.lives;
        player.pos = Pos(1, 1);
        player.invincible = toTicks(2);
        player.safeHold = 0;
        player.hidden = 0;
        heldDirs.clear();
        setToast(reason + "  -1 LIFE");

        if (player.lives <= 0)
        {
            state = ST_OVER;
        }
    }

    void useAttack()
    {
        if (player.attackCd > 0)
        {
            setToast("ATTACK IS COOLING DOWN");
            return;
        }
        player.attackCd = toTicks(3);

        bool hit = false;
        for (size_t i = 0; i < enemies.size(); ++i)
        {
            Enemy& e = enemies[i];
            if (e.respawn > 0 || manhattan(e.pos, player.pos) > 3) continue;

            hit = true;
            if (player.power > 0)
            {
                defeatEnemy(e);
                player.score += 200;
            }
            else
            {
                e.stun = toTicks(3);
            }
        }
        setToast(hit ? "STUN STRIKE HIT!" : "STUN STRIKE - NO TARGET");
    }

    void useDash()
    {
        if (player.dashCd > 0)
        {
            setToast("DASH IS COOLING DOWN");
            return;
        }
        player.dash = toTicks(1);
        player.dashCd = toTicks(5);
        setToast("DASH!");
    }

    void defeatEnemy(Enemy& e)
    {
        e.respawn = toTicks(5);
        e.stun = 0;
    }

    void updateSafeHouse()
    {
        if (terrain[player.pos.y][player.pos.x] == 'H')
        {
            ++player.safeHold;
            if (player.safeHold >= toTicks(3) && player.hidden < toTicks(2))
            {
                player.hidden = toTicks(8);
                setToast("HIDDEN - ENEMIES CANNOT DETECT YOU");
            }
        }
        else
        {
            player.safeHold = 0;
        }

        // Once triggered, hiding keeps counting down even after leaving.
        if (player.hidden > 0) --player.hidden;
    }

    /* ------------------------------- enemy AI --------------------------- */

    Pos patrolStep(const Enemy& e) const
    {
        vector<int> options;
        for (int d = 1; d <= 4; ++d)
        {
            if (walkable(e.pos.x + DX[d], e.pos.y + DY[d]))
            {
                options.push_back(d);
            }
        }
        if (options.empty()) return e.pos;

        for (size_t i = 0; i < options.size(); ++i)
        {
            if (options[i] == e.dir && rnd(100) < 70)
            {
                return Pos(e.pos.x + DX[e.dir], e.pos.y + DY[e.dir]);
            }
        }
        int d = options[rnd((int)options.size())];
        return Pos(e.pos.x + DX[d], e.pos.y + DY[d]);
    }

    Pos chaseStep(const Enemy& e) const
    {
        // BFS from the player: parent[cell] is one step closer to the player.
        vector<vector<Pos> > parent(MAP_H, vector<Pos>(MAP_W, Pos(-1, -1)));
        queue<Pos> q;
        parent[player.pos.y][player.pos.x] = player.pos;
        q.push(player.pos);

        while (!q.empty())
        {
            Pos cur = q.front();
            q.pop();
            if (cur.eq(e.pos)) break;

            for (int d = 1; d <= 4; ++d)
            {
                int nx = cur.x + DX[d];
                int ny = cur.y + DY[d];
                if (!walkable(nx, ny)) continue;
                if (parent[ny][nx].x != -1) continue;
                parent[ny][nx] = cur;
                q.push(Pos(nx, ny));
            }
        }

        Pos step = parent[e.pos.y][e.pos.x];
        if (step.x == -1) return patrolStep(e);
        return step;
    }

    Pos fleeStep(const Enemy& e) const
    {
        // Distance field from the player; move to the farthest neighbour.
        vector<vector<int> > dist(MAP_H, vector<int>(MAP_W, -1));
        queue<Pos> q;
        dist[player.pos.y][player.pos.x] = 0;
        q.push(player.pos);

        while (!q.empty())
        {
            Pos cur = q.front();
            q.pop();
            for (int d = 1; d <= 4; ++d)
            {
                int nx = cur.x + DX[d];
                int ny = cur.y + DY[d];
                if (!walkable(nx, ny)) continue;
                if (dist[ny][nx] != -1) continue;
                dist[ny][nx] = dist[cur.y][cur.x] + 1;
                q.push(Pos(nx, ny));
            }
        }

        int best = -1;
        int bestDist = -1;
        for (int d = 1; d <= 4; ++d)
        {
            int nx = e.pos.x + DX[d];
            int ny = e.pos.y + DY[d];
            if (!walkable(nx, ny)) continue;
            if (dist[ny][nx] > bestDist)
            {
                bestDist = dist[ny][nx];
                best = d;
            }
        }
        if (best == -1) return e.pos;
        return Pos(e.pos.x + DX[best], e.pos.y + DY[best]);
    }

    void updateEnemy(Enemy& e)
    {
        if (e.respawn > 0)
        {
            --e.respawn;
            if (e.respawn == 0)
            {
                e.pos = e.home;
                e.stun = toTicks(1);
            }
            return;
        }

        if (e.stun > 0)
        {
            --e.stun;
            return;
        }

        Pos step;
        if (player.hidden > 0)
        {
            step = patrolStep(e);
        }
        else if (player.power > 0)
        {
            step = fleeStep(e);
        }
        else if (manhattan(e.pos, player.pos) <= 10)
        {
            step = chaseStep(e);
        }
        else
        {
            step = patrolStep(e);
        }

        if (walkable(step.x, step.y))
        {
            if (step.x > e.pos.x) e.dir = DIR_RIGHT;
            else if (step.x < e.pos.x) e.dir = DIR_LEFT;
            else if (step.y > e.pos.y) e.dir = DIR_DOWN;
            else if (step.y < e.pos.y) e.dir = DIR_UP;

            e.pos = step;

            // Enemies can also step on traps and get frozen.
            if (items[e.pos.y][e.pos.x] == '^')
            {
                items[e.pos.y][e.pos.x] = ' ';
                if (beansLeft > 0) --beansLeft;
                e.stun = toTicks(2);
            }
        }
    }

    /* ------------------------------- screens ---------------------------- */

    void renderIntro()
    {
        printPadded("=== STAR BEAN ADVENTURE  (C++ Console Edition) ===");
        cout << "\n";
        printPadded("A little star person is lost in the bean maze.");
        printPadded("Eat every bean, then step on E to leave the maze.");
        printPadded("Monsters chase you. Stay in a safe house for 3s to hide.");
        cout << "\n";
        printPadded("CONTROLS");
        printPadded("  WASD / Arrow keys : move (hold to keep moving)");
        printPadded("  J : stun strike     K : dash");
        printPadded("  P : pause           Q : quit            R : restart");
        cout << "\n";
        printPadded("BEANS");
        printPadded("  . normal   P power   * star shield   T stun   ^ trap");
        cout << "\n";
        printPadded("Press ENTER to start. Press Q to quit.");
        cout.flush();
    }
};

/* --------------------------------- main -------------------------------- */

int main()
{
    srand((unsigned)time(NULL));

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);

    // Construct the game first so it can remember the original cursor state.
    Game game(hOut);

    CONSOLE_CURSOR_INFO cursor;
    if (GetConsoleCursorInfo(hOut, &cursor))
    {
        cursor.bVisible = FALSE;
        SetConsoleCursorInfo(hOut, &cursor);
    }

    system("cls");
    while (!game.quit())
    {
        game.handleInput();
        game.update();
        game.render();
        Sleep(TICK_MS);
    }

    system("cls");
    game.restoreCursor();
    cout << "Thanks for playing Star Bean Adventure!\n";
    return 0;
}
