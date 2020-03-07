#ifndef _UNION_FIND_H_
#define _UNION_FIND_H_

#include <stdlib.h>

struct UFSet {
    int N;
    int count; 
    int id[]; 
};

struct UFSet* newUFSet(int N);

int find(struct UFSet* pset, int idx);

int connected(struct UFSet* pset, int p, int q);

// merge two set
void merge(struct UFSet* pset, int p, int q);

#endif // _UNION_FIND_H_
