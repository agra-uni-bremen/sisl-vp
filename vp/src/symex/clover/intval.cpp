#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <variant>
#include <vector>

#include "fns.h"

using namespace clover;

size_t
intByteSize(IntValue v)
{
	if (std::get_if<uint8_t>(&v) != nullptr)
		return sizeof(uint8_t);
	else if (std::get_if<uint32_t>(&v) != nullptr)
		return sizeof(uint32_t);

	assert(0); /* unreachable */
	return 0;
}

uint64_t
intToUint(IntValue v)
{
	uint64_t exprValue;
	if (std::get_if<uint8_t>(&v) != nullptr) {
		exprValue = std::get<uint8_t>(v);
	} else if (std::get_if<uint32_t>(&v) != nullptr) {
		exprValue = std::get<uint32_t>(v);
	} else {
		assert(0);
	}

	return exprValue;
}

IntValue
intFromVector(std::vector<unsigned char> vector)
{
	IntValue intval;

	switch (vector.size()) {
	case 1: {
		uint8_t v;
		memcpy(&v, &vector[0], sizeof(uint8_t));
		intval = v;
	} break;
	case 4: {
		uint32_t v;
		memcpy(&v, &vector[0], sizeof(uint32_t));
		intval = v;
	} break;
	default:
		assert(0);
	}

	return intval;
}
