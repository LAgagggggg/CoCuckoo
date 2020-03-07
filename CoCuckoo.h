#ifndef _COCUCKOO_H_
#define _COCUCKOO_H_

#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <string>

#include "union_find.h"
#include "MurmurHash3.h"
#include <inttypes.h>
#include <stdint.h>
#include <vector>

typedef std::string DataType;

void cocuckoo_init();
void cocuckoo_insert();



#endif