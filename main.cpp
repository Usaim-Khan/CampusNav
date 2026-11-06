// CampusNav - Campus Route Finder (CLI) - C++17
// Group project: Usaim, Memoona, Amna.

#include <cmath>
#include <cstdio>
#include <cstring>
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
        double w = hypot(NODES[u].x - NODES[v].x, NODES[u].y - NODES[v].y);   // pixels for now
        g.addEdge(u, v, w);
    }
}

int main() {
    Graph g;
    buildCampusGraph(g);
    printf("Campus graph: %d nodes, %d walkways\n\n", N, NUM_EDGES);
    for (int i = 0; i < N; i++) {
        printf("%2d %-30s ->", i, NODES[i].name);
        for (Edge* e = g.head[i]; e; e = e->next) printf(" %d", e->to);
        printf("\n");
    }
    return 0;
}
