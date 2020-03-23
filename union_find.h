#ifndef _UNION_FIND_H_
#define _UNION_FIND_H_

#include <stdlib.h>
#include <vector>
#include <unordered_set>

using namespace std;

struct UFSet {
    int N;
    int count; 
    vector<unordered_set<int>> * re_edge; // Used to handle delete and graph splitting;
    int id[]; 
};

struct UFSet* newUFSet(int N);

int find(struct UFSet* pset, int idx);

int connected(struct UFSet* pset, int p, int q);

// merge two set
void merge(struct UFSet* pset, int p, int q);

void freeUFSet(struct UFSet* p);

void addReEdge(UFSet & ufset, int from, int to);
void removeReEdge(UFSet & ufset, int from, int to);

#endif // _UNION_FIND_H_
