// CampusNav - Campus Route Finder (CLI) - C++17
// Group project: Usaim, Memoona, Amna.

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

int main() {
    printf("=== v1: hash table and stack self-test ===\n");
    HashTable ids;
    ids.put("Main Gate", 0);
    ids.put("EE Building", 1);
    ids.put("Sports Room", 12);
    printf("Main Gate    -> %d\n", ids.get("Main Gate"));
    printf("Sports Room  -> %d\n", ids.get("Sports Room"));
    printf("Unknown      -> %d (-1 means missing)\n", ids.get("Unknown"));

    IntStack st;
    for (int i = 1; i <= 5; i++) st.push(i);
    printf("Stack pops   :");
    while (!st.empty()) printf(" %d", st.pop());
    printf("\n");
    return 0;
}
