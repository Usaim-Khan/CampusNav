// CampusNav - Campus Route Finder (CLI) - C++17
// Group project: Usaim, Memoona, Amna.

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif
using namespace std;

// ============================================================ Usaim: hash table + stack
// Hash table: name -> node id, using separate chaining.
struct HNode { char key[48]; int val; HNode* next; };

class HashTable {
public:
    HashTable() { for (int i = 0; i < B; i++) bucket[i] = nullptr; }
    ~HashTable() {
        for (int i = 0; i < B; i++)
            for (HNode* p = bucket[i]; p;) { HNode* nx = p->next; delete p; p = nx; }
    }
    HashTable(const HashTable&) = delete;
    HashTable& operator=(const HashTable&) = delete;

    void put(const char* key, int val) {   // inserts or updates
        unsigned h = hashOf(key);
        for (HNode* p = bucket[h]; p; p = p->next)
            if (strcmp(p->key, key) == 0) { p->val = val; return; }
        HNode* nd = new HNode;
        snprintf(nd->key, sizeof nd->key, "%s", key);
        nd->val = val;
        nd->next = bucket[h];
        bucket[h] = nd;
    }
    int get(const char* key) const {       // -1 if missing
        for (HNode* p = bucket[hashOf(key)]; p; p = p->next)
            if (strcmp(p->key, key) == 0) return p->val;
        return -1;
    }
private:
    static const int B = 64;               // number of buckets
    HNode* bucket[B];
    static unsigned hashOf(const char* s) {   // djb2
        unsigned h = 5381;
        for (; *s; s++) h = h * 33 + (unsigned char)*s;
        return h % B;
    }
};

// Integer stack (growable array). Used to reverse the path (parent chain gives dst -> src).
class IntStack {
public:
    IntStack() : data(new int[16]), cap(16), n(0) {}
    ~IntStack() { delete[] data; }
    IntStack(const IntStack&) = delete;
    IntStack& operator=(const IntStack&) = delete;

    void push(int x) {
        if (n == cap) grow();
        data[n++] = x;
    }
    int pop() { return data[--n]; }        // caller checks empty() first
    bool empty() const { return n == 0; }
    int size() const { return n; }
private:
    int* data;
    int cap, n;
    void grow() {                           // double the capacity
        int* bigger = new int[cap * 2];
        for (int i = 0; i < n; i++) bigger[i] = data[i];
        delete[] data;
        data = bigger;
        cap *= 2;
    }
};

// ============================================================ Memoona: map data
// Pixel coordinates are taken from the campus map image.
struct Node { const char* name; int x, y; };

const int NUM_PLACES = 13;   // selectable places come first
const int N = 33;            // places + junctions
const Node NODES[N] = {
    {"Main Gate", 498, 118},
    {"EE Building", 299, 452},
    {"AI Department", 479, 524},
    {"Fresh Juice Corner", 376, 577},
    {"FAST Mosque", 558, 670},
    {"CS Building (North Entrance)", 760, 370},
    {"CS Building (Courtyard)", 760, 525},
    {"Basketball / Volleyball", 880, 321},
    {"Car Parking", 1058, 203},
    {"Multipurpose Hall", 1145, 373},
    {"Badminton Court", 1262, 313},
    {"Football / Futsal", 1262, 482},
    {"Sports Room", 1342, 648},
    // junctions (road / walkway intersections)
    {"a", 498, 340}, {"b", 498, 452}, {"q", 498, 524}, {"c", 498, 605},
    {"o", 498, 670}, {"p", 498, 700}, {"d", 760, 340}, {"e", 880, 340},
    {"f", 1008, 118}, {"g", 1008, 203}, {"h", 1008, 340}, {"i", 1008, 373},
    {"j", 1008, 605}, {"m", 1008, 700}, {"k", 760, 605}, {"l", 950, 605},
    {"n", 1342, 700}, {"r", 498, 577}, {"t", 1262, 373}, {"u", 1262, 648},
};

struct EdgeDef { const char *a, *b; };
const int NUM_EDGES = 36;
const EdgeDef EDGES[NUM_EDGES] = {
    {"Main Gate", "a"}, {"Main Gate", "f"}, {"a", "b"}, {"b", "q"}, {"q", "r"},
    {"r", "c"}, {"c", "o"}, {"o", "p"}, {"p", "m"}, {"o", "FAST Mosque"},
    {"b", "EE Building"}, {"q", "AI Department"}, {"r", "Fresh Juice Corner"},
    {"a", "d"}, {"d", "e"}, {"e", "h"},
    {"d", "CS Building (North Entrance)"}, {"e", "Basketball / Volleyball"},
    {"c", "k"}, {"k", "l"}, {"l", "j"}, {"k", "CS Building (Courtyard)"},
    {"f", "g"}, {"g", "h"}, {"h", "i"}, {"i", "j"}, {"j", "m"},
    {"g", "Car Parking"}, {"i", "Multipurpose Hall"},
    {"Multipurpose Hall", "t"}, {"t", "Badminton Court"},
    {"t", "Football / Futsal"}, {"Football / Futsal", "u"}, {"u", "Sports Room"},
    {"m", "n"}, {"n", "Sports Room"},
};

// Scale: the campus is 270 m wide (measured on Google Maps) and spans 1475 px
// in the map image (boundary x = 25 .. 1500), so 1 px = 270 / 1475 m.
const double CAMPUS_WIDTH_M = 270.0;
const double PX_TO_M = CAMPUS_WIDTH_M / 1475.0;
const double WALK_SPEED = 1.3;   // meters per second

// Walking time in minutes for a distance in meters.
double walkMinutes(double meters) { return meters / WALK_SPEED / 60.0; }

// Building outlines and the campus boundary, used when drawing the map.
struct Box { int x1, y1, x2, y2; const char* label; };
const int NUM_BOXES = 7;
const Box BOXES[NUM_BOXES] = {
    {92, 250, 250, 655, "EE BLOCK"},     {542, 395, 978, 600, "CS BUILDING"},
    {1038, 443, 1220, 655, "MULTI HALL"}, {1066, 161, 1425, 245, "PARKING"},
    {798, 216, 963, 312, "BASKETBALL"},   {1292, 280, 1425, 368, "COURT"},
    {1292, 385, 1425, 580, "FOOTBALL"},
};
const int BOUND_X1 = 25, BOUND_Y1 = 95, BOUND_X2 = 1500, BOUND_Y2 = 727;

// ============================================================ Memoona: graph
// Adjacency list built from hand-written singly linked lists.
struct Edge { int to; double w; Edge* next; };

class Graph {
public:
    Edge* head[N];
    Graph() : used(0) { for (int i = 0; i < N; i++) head[i] = nullptr; }
    void addEdge(int u, int v, double w) {   // undirected: add both directions
        link(u, v, w);
        link(v, u, w);
    }
private:
    Edge pool[2 * NUM_EDGES];   // fixed storage for edge nodes
    int used;
    void link(int u, int v, double w) {
        Edge* e = &pool[used++];
        e->to = v; e->w = w; e->next = head[u]; head[u] = e;
    }
};

// Name -> node id (hash table built once, on first use).
HashTable& nameTable() {
    static HashTable table;
    static bool built = false;
    if (!built) {
        for (int i = 0; i < N; i++) table.put(NODES[i].name, i);
        built = true;
    }
    return table;
}

int nodeId(const char* name) { return nameTable().get(name); }

void buildCampusGraph(Graph& g) {
    for (int i = 0; i < NUM_EDGES; i++) {
        int u = nodeId(EDGES[i].a), v = nodeId(EDGES[i].b);
        double w = hypot(NODES[u].x - NODES[v].x, NODES[u].y - NODES[v].y) * PX_TO_M;   // meters
        g.addEdge(u, v, w);
    }
}

// ============================================================ Amna: min-heap
struct HeapItem { double d; int node; };

class MinHeap {
public:
    MinHeap() : n(0) {}
    bool empty() const { return n == 0; }
    void push(int node, double d) {
        a[n] = {d, node};
        int i = n++;
        while (i > 0 && a[(i - 1) / 2].d > a[i].d) {   // sift up
            swapItems(a[i], a[(i - 1) / 2]);
            i = (i - 1) / 2;
        }
    }
    HeapItem pop() {                                  // caller checks empty() first
        HeapItem top = a[0];
        a[0] = a[--n];
        int i = 0;
        while (true) {                                // sift down
            int l = 2 * i + 1, r = l + 1, m = i;
            if (l < n && a[l].d < a[m].d) m = l;
            if (r < n && a[r].d < a[m].d) m = r;
            if (m == i) break;
            swapItems(a[i], a[m]);
            i = m;
        }
        return top;
    }
private:
    HeapItem a[256];   // enough for all relaxations in this graph
    int n;
    static void swapItems(HeapItem& x, HeapItem& y) { HeapItem t = x; x = y; y = t; }
};

// ============================================================ Amna: Dijkstra + path building
const double INF = 1e18;

// Single-source Dijkstra. Fills dist[] and parent[] for every node reached.
// If stopAt >= 0 it stops as soon as that node is finalized (faster for one target).
void dijkstra(const Graph& g, int src, double dist[], int parent[], int stopAt = -1) {
    for (int i = 0; i < N; i++) { dist[i] = INF; parent[i] = -1; }
    dist[src] = 0;
    MinHeap heap;
    heap.push(src, 0);
    while (!heap.empty()) {
        HeapItem cur = heap.pop();
        if (cur.d > dist[cur.node]) continue;   // stale entry
        if (cur.node == stopAt) break;
        for (Edge* e = g.head[cur.node]; e; e = e->next) {
            double nd = cur.d + e->w;
            if (nd < dist[e->to]) {
                dist[e->to] = nd;
                parent[e->to] = cur.node;
                heap.push(e->to, nd);
            }
        }
    }
}

// The route being built (node ids in walking order, places and junctions).
const int PATHCAP = 400;
int pathNodes[PATHCAP];
int pathLen = 0;
bool isStop[N];   // intermediate stops of a multi-stop route
void clearPath() {
    pathLen = 0;
    for (int i = 0; i < N; i++) isStop[i] = false;
}

// Appends the shortest path src -> dst (from parent[]) to the route.
// The stack reverses the parent chain; the first leg also adds src.
void appendLeg(const int parent[], int src, int dst) {
    IntStack st;
    for (int cur = dst; cur != src; cur = parent[cur]) st.push(cur);
    if (pathLen == 0) pathNodes[pathLen++] = src;
    while (!st.empty()) pathNodes[pathLen++] = st.pop();
}

// ============================================================ Usaim: terminal colors, menu and CLI
const char *RESET = "\033[0m", *DIM = "\033[90m", *BOLD = "\033[1m";
const char *CYANB = "\033[96m\033[1m", *YELLOWB = "\033[93m\033[1m";
const char *START_C = "\033[1;7;92m", *END_C = "\033[1;7;91m";
const char* WAY_C = "\033[1;7;95m";   // intermediate stops (reverse magenta)

// ---- map drawing: the grid is printed with ANSI colors, roads use Bresenham's line algorithm
const int COL_DIV = 15, ROW_DIV = 28;   // pixel -> terminal cell scale
const int GW = 104, GH = 28;            // grid size in cells

struct Cell { const char* ch; const char* col; };
Cell grid[GH][GW];

int cellCol(int x) { return (int)lround((double)x / COL_DIV); }
int cellRow(int y) { return (int)lround((double)y / ROW_DIV); }

void put(int c, int r, const char* ch, const char* col) {
    if (c >= 0 && c < GW && r >= 0 && r < GH) grid[r][c] = {ch, col};
}

bool isPathEdge(int u, int v) {
    for (int i = 0; i + 1 < pathLen; i++)
        if ((pathNodes[i] == u && pathNodes[i + 1] == v) ||
            (pathNodes[i] == v && pathNodes[i + 1] == u))
            return true;
    return false;
}

bool onPath(int u) {
    for (int i = 0; i < pathLen; i++) if (pathNodes[i] == u) return true;
    return false;
}

void drawEdge(int u, int v, bool highlight) {
    int c1 = cellCol(NODES[u].x), r1 = cellRow(NODES[u].y);
    int c2 = cellCol(NODES[v].x), r2 = cellRow(NODES[v].y);
    const char* ch = (c1 == c2) ? (highlight ? "┃" : "│")
                   : (r1 == r2) ? (highlight ? "━" : "─")
                                : (highlight ? "•" : "·");
    int dc = abs(c2 - c1), dr = abs(r2 - r1);
    int sc = c1 < c2 ? 1 : -1, sr = r1 < r2 ? 1 : -1;
    int err = dc - dr, c = c1, r = r1;
    while (true) {   // Bresenham line
        Cell& cell = grid[r][c];
        if (highlight || strcmp(cell.ch, " ") == 0 || strcmp(cell.col, DIM) == 0)
            put(c, r, ch, highlight ? YELLOWB : DIM);
        if (c == c2 && r == r2) break;
        int e2 = 2 * err;
        if (e2 > -dr) { err -= dr; c += sc; }
        if (e2 < dc) { err += dc; r += sr; }
    }
}

void putText(int c, int r, const char* text, const char* col) {
    // ASCII only (labels), one cell per character; each char needs its own
    // persistent C string because grid cells store pointers.
    static char one[128][2];
    for (int i = 0; text[i]; i++) {
        unsigned char ch = (unsigned char)text[i] & 127;
        one[ch][0] = (char)ch; one[ch][1] = 0;
        put(c + i, r, one[ch], col);
    }
}

// Makes the Windows console show UTF-8 box characters and ANSI colors.
void setupConsole() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(h, &mode)) SetConsoleMode(h, mode | 0x0004);   // enable ANSI colors
#endif
}

void drawMap(bool withPath) {
    for (int r = 0; r < GH; r++)
        for (int c = 0; c < GW; c++) grid[r][c] = {" ", ""};

    // campus boundary (open at the main gate)
    int bc1 = cellCol(BOUND_X1), br1 = cellRow(BOUND_Y1);
    int bc2 = cellCol(BOUND_X2), br2 = cellRow(BOUND_Y2);
    int gateCol = cellCol(NODES[0].x);
    for (int c = bc1; c <= bc2; c++) {
        if (c != gateCol) put(c, br1, "═", DIM);
        put(c, br2, "═", DIM);
    }
    for (int r = br1; r <= br2; r++) { put(bc1, r, "║", DIM); put(bc2, r, "║", DIM); }
    put(bc1, br1, "╔", DIM); put(bc2, br1, "╗", DIM);
    put(bc1, br2, "╚", DIM); put(bc2, br2, "╝", DIM);
    const char* title = " UNIVERSITY CAMPUS ";
    putText((bc1 + bc2 - (int)strlen(title)) / 2, br2, title, DIM);

    // building boxes
    for (int i = 0; i < NUM_BOXES; i++) {
        const Box& b = BOXES[i];
        int c1 = cellCol(b.x1), r1 = cellRow(b.y1), c2 = cellCol(b.x2), r2 = cellRow(b.y2);
        for (int c = c1; c <= c2; c++) { put(c, r1, "─", DIM); put(c, r2, "─", DIM); }
        for (int r = r1; r <= r2; r++) { put(c1, r, "│", DIM); put(c2, r, "│", DIM); }
        put(c1, r1, "┌", DIM); put(c2, r1, "┐", DIM);
        put(c1, r2, "└", DIM); put(c2, r2, "┘", DIM);
        putText((c1 + c2 - (int)strlen(b.label)) / 2 + 1, (r1 + r2) / 2, b.label, DIM);
    }

    // walkways (route edges drawn highlighted)
    for (int i = 0; i < NUM_EDGES; i++) {
        int u = nodeId(EDGES[i].a), v = nodeId(EDGES[i].b);
        drawEdge(u, v, withPath && isPathEdge(u, v));
    }

    // nodes: places show their number, junctions show '+'
    static char labels[N][4];
    for (int i = 0; i < N; i++) {
        const char* col;
        const char* text;
        if (i < NUM_PLACES) {
            snprintf(labels[i], 4, "%d", i + 1);
            text = labels[i]; col = CYANB;
        } else {
            text = "+"; col = DIM;
        }
        if (withPath && onPath(i)) {
            col = YELLOWB;
            if (isStop[i]) col = WAY_C;
            if (i == pathNodes[0]) col = START_C;
            else if (i == pathNodes[pathLen - 1]) col = END_C;
        }
        int c = cellCol(NODES[i].x), r = cellRow(NODES[i].y);
        putText(c - (int)strlen(text) / 2, r, text, col);
    }

    // print the grid, skipping trailing blanks on each row
    for (int r = 0; r < GH; r++) {
        int last = -1;
        for (int c = 0; c < GW; c++) if (strcmp(grid[r][c].ch, " ") != 0) last = c;
        for (int c = 0; c <= last; c++) {
            if (grid[r][c].col[0]) { fputs(grid[r][c].col, stdout); fputs(grid[r][c].ch, stdout); fputs(RESET, stdout); }
            else fputs(grid[r][c].ch, stdout);
        }
        fputc('\n', stdout);
    }
}

// ---- input helpers
void printLegend() {
    printf("\n%sLocations%s\n", BOLD, RESET);
    int half = (NUM_PLACES + 1) / 2;
    for (int i = 0; i < half; i++) {
        printf("  %s%2d%s. %-30s", CYANB, i + 1, RESET, NODES[i].name);
        int j = i + half;
        if (j < NUM_PLACES) printf("%s%2d%s. %s", CYANB, j + 1, RESET, NODES[j].name);
        printf("\n");
    }
}

string lower(string s) {
    for (char& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

// Asks for a place by number or (unique part of) name.
// If allowEmpty is true, an empty line returns -1.
int pickPlace(const char* prompt, bool allowEmpty = false) {
    while (true) {
        cout << prompt << flush;
        string s;
        getline(cin, s);
        if (s.empty() && allowEmpty) return -1;
        bool digits = !s.empty();
        for (char c : s) if (!isdigit((unsigned char)c)) digits = false;
        if (digits) {
            int v = atoi(s.c_str());
            if (v >= 1 && v <= NUM_PLACES) return v - 1;
        } else if (!s.empty()) {
            string q = lower(s);
            int found = -1, count = 0;
            for (int i = 0; i < NUM_PLACES; i++)
                if (lower(NODES[i].name).find(q) != string::npos) { found = i; count++; }
            if (count == 1) return found;
        }
        printf("  Invalid or ambiguous. Enter a number from the list or a unique name.\n");
    }
}

// ---- output helpers
void printPlacesOnPath() {
    printf("\n%sRoute:%s ", BOLD, RESET);
    bool first = true;
    for (int i = 0; i < pathLen; i++) {
        if (pathNodes[i] >= NUM_PLACES) continue;   // skip junctions
        printf("%s%s", first ? "" : " → ", NODES[pathNodes[i]].name);
        first = false;
    }
    printf("\n");
}

void printTotals(double d) {
    printf("%sDistance:%s %.0f m   %sWalking time:%s ~%.1f min\n",
           BOLD, RESET, d, BOLD, RESET, walkMinutes(d));
}

void printLegendLine(bool withStops) {
    printf("Legend: %s start %s %s end %s ", START_C, RESET, END_C, RESET);
    if (withStops) printf("%s stop %s ", WAY_C, RESET);
    printf("%s━ path%s\n", YELLOWB, RESET);
}

// ---- feature 1: shortest route between two places
void findRoute(const Graph& g) {
    printLegend();
    int src = pickPlace("\nStart (number or name): ");
    int dst = pickPlace("Destination (number or name): ");
    if (src == dst) { printf("Start and destination are the same.\n"); return; }

    double dist[N];
    int parent[N];
    dijkstra(g, src, dist, parent, dst);
    if (dist[dst] >= INF) { printf("No route found.\n"); return; }

    clearPath();
    appendLeg(parent, src, dst);
    printf("\n");
    drawMap(true);
    printPlacesOnPath();
    printTotals(dist[dst]);
    printLegendLine(false);
    clearPath();
}

// ---- feature 2: multi-stop route (A -> B -> C ...): one Dijkstra run per leg
const int MAX_STOPS = 10;

void multiStopRoute(const Graph& g) {
    printLegend();
    printf("\nEnter your stops in order (2 to %d). Press Enter on an empty line to finish.\n", MAX_STOPS);
    int stops[MAX_STOPS];
    int ns = 0;
    while (ns < MAX_STOPS) {
        char prompt[48];
        if (ns == 0) snprintf(prompt, sizeof prompt, "Start: ");
        else snprintf(prompt, sizeof prompt, "Stop %d%s: ", ns + 1, ns >= 2 ? " (Enter to finish)" : "");
        int s = pickPlace(prompt, ns >= 2);
        if (s < 0) break;
        stops[ns++] = s;
    }

    clearPath();
    double legDist[MAX_STOPS], total = 0;
    for (int k = 0; k + 1 < ns; k++) {
        double dist[N];
        int parent[N];
        dijkstra(g, stops[k], dist, parent, stops[k + 1]);
        if (dist[stops[k + 1]] >= INF) { printf("No route found for leg %d.\n", k + 1); clearPath(); return; }
        legDist[k] = dist[stops[k + 1]];
        total += legDist[k];
        appendLeg(parent, stops[k], stops[k + 1]);
    }
    for (int k = 1; k + 1 < ns; k++) isStop[stops[k]] = true;

    printf("\n");
    drawMap(true);
    printf("\n%sLegs:%s\n", BOLD, RESET);
    for (int k = 0; k + 1 < ns; k++)
        printf("  %d. %s → %s   %.0f m\n", k + 1, NODES[stops[k]].name, NODES[stops[k + 1]].name, legDist[k]);
    printPlacesOnPath();
    printTotals(total);
    printLegendLine(true);
    clearPath();
}

int main() {
    setupConsole();
    Graph g;
    buildCampusGraph(g);
    clearPath();

    while (true) {
        printf("\n%s=== CampusNav ===%s\n", BOLD, RESET);
        printf("1. Show map\n2. Shortest route\n3. Multi-stop route\n4. List locations\n5. Quit\nChoice: ");
        fflush(stdout);
        string choice;
        getline(cin, choice);
        if (choice == "1") { clearPath(); drawMap(false); printLegend(); }
        else if (choice == "2") findRoute(g);
        else if (choice == "3") multiStopRoute(g);
        else if (choice == "4") printLegend();
        else if (choice == "5") { printf("Goodbye!\n"); break; }
        else printf("Please enter 1-5.\n");
    }
    return 0;
}
