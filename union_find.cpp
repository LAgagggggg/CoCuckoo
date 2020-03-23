#include "union_find.h"
#include "spinlock.h"

struct UFSet* newUFSet(int N) {
    struct UFSet* pset = (struct UFSet*) malloc(sizeof(struct UFSet) + sizeof(int) * N);
    pset->re_edge = new vector<unordered_set<int>>(N);
    pset->N = N;
    pset->count = N;
    for (int i = 0; i < N; i++) {
        pset->id[i] = -1;
    }
    return pset;
}

void freeUFSet(struct UFSet* p) {
    free(p->re_edge);
    free(p);
}

int find(struct UFSet* pset, int idx) {
    int parent = pset->id[idx];
    if (parent < 0)
        return idx;
    else
        return pset->id[idx] = find(pset, parent);
}

int connected(struct UFSet* pset, int p, int q) {
    return find(pset, p) == find(pset, q);
}

// merge two set
void merge(struct UFSet* pset, int p, int q) {
    int i = find(pset, p);
    int j = find(pset, q);
    if (i == j) return;

    if (pset->id[i] < pset->id[j]) {
        pset->id[i] += pset->id[j];
        pset->id[j] = i;
    } else {
        pset->id[j] += pset->id[i];
        pset->id[i] = j;
    }
    pset->count--;
}

spinlock lockForReEdge;

void addReEdge(UFSet & ufset, int from, int to) {
    spin_lock(&lockForReEdge);
    ufset.re_edge->at(from).insert(to);
    spin_unlock(&lockForReEdge);
}

void removeReEdge(UFSet & ufset, int from, int to) {
    spin_lock(&lockForReEdge);
    ufset.re_edge->at(from).erase(to);
    spin_unlock(&lockForReEdge);
}
