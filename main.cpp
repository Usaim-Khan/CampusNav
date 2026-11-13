// CampusNav - Campus Route Finder (CLI) - C++17
// Group project: Usaim, Memoona, Amna.

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
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
void clearPath() { pathLen = 0; }

// Appends the shortest path src -> dst (from parent[]) to the route.
// The stack reverses the parent chain; the first leg also adds src.
void appendLeg(const int parent[], int src, int dst) {
    IntStack st;
    for (int cur = dst; cur != src; cur = parent[cur]) st.push(cur);
    if (pathLen == 0) pathNodes[pathLen++] = src;
    while (!st.empty()) pathNodes[pathLen++] = st.pop();
}

// ============================================================ Usaim: terminal colors, menu and CLI
const char *RESET = "\033[0m", *BOLD = "\033[1m";
const char* CYANB = "\033[96m\033[1m";
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
int pickPlace(const char* prompt) {
    while (true) {
        cout << prompt << flush;
        string s;
        getline(cin, s);
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
    printPlacesOnPath();
    printTotals(dist[dst]);
    clearPath();
}

int main() {
    Graph g;
    buildCampusGraph(g);
    clearPath();

    while (true) {
        printf("\n%s=== CampusNav ===%s\n", BOLD, RESET);
        printf("1. Shortest route\n2. List locations\n3. Quit\nChoice: ");
        fflush(stdout);
        string choice;
        getline(cin, choice);
        if (choice == "1") findRoute(g);
        else if (choice == "2") printLegend();
        else if (choice == "3") { printf("Goodbye!\n"); break; }
        else printf("Please enter 1-3.\n");
    }
    return 0;
}
