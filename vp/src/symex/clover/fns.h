#ifndef CLOVER_FNS_H
#define CLOVER_FNS_H

#include <clover/clover.h>

#include <vector>

/* TODO: Create a wrapper class around IntVal */

size_t intByteSize(clover::IntValue v);
uint64_t intToUint(clover::IntValue v);
clover::IntValue intFromVector(std::vector<unsigned char> vector);

#endif
