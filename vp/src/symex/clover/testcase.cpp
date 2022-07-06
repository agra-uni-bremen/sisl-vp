#include <fstream>
#include <iostream>
#include <regex>

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>

#include <clover/clover.h>
using namespace clover;

typedef std::pair<std::string, IntValue> Assignment;
typedef enum {
	UINT8,
	UINT32,
} AssignType;

#define PARSE_INT(STR, FMT, TYPE)                    \
	{                                            \
		TYPE v;                              \
		int r = sscanf(STR, "%" FMT "", &v); \
                                                     \
		if (r == 1)                          \
			return (TYPE)v;              \
		else                                 \
			return std::nullopt;         \
	}

static std::optional<IntValue>
parseIntVal(AssignType type, std::string input)
{
	switch (type) {
	case UINT8:
		PARSE_INT(input.c_str(), SCNu8, uint8_t);
		break;
	case UINT32:
		PARSE_INT(input.c_str(), SCNu32, uint32_t);
		break;
	}

	return std::nullopt;
}

static std::optional<AssignType>
parseAssignType(std::string type)
{
	if (type == "uint8_t") {
		return UINT8;
	} else if (type == "uint32_t") {
		return UINT32;
	} else {
		return std::nullopt;
	}
}

static std::optional<Assignment>
parseAssign(std::string assign)
{
	std::regex re("(..*)	(uint8_t|uint32_t)	([0-9][0-9]*)");

	std::smatch match;
	if (regex_search(assign, match, re)) {
		if (match.size() != 4) /* match includes the entire string */
			return std::nullopt;

		auto name = match[1].str();
		auto type = parseAssignType(match[2].str());
		if (!type.has_value())
			return std::nullopt;

		auto ival = parseIntVal(*type, match[3].str());
		if (!ival.has_value())
			return std::nullopt;

		return std::make_pair(name, *ival);
	}

	return std::nullopt;
}

ConcreteStore
TestCase::fromFile(std::string name, std::ifstream &stream)
{
	ConcreteStore assigns;

	std::string line;
	size_t lineNum = 1;

	while (std::getline(stream, line)) {
		std::optional<Assignment> assign = parseAssign(line);
		if (!assign.has_value()) {
			throw TestCase::ParserError(name, lineNum, "invalid assignment");
		}

		assigns[std::get<0>(*assign)] = std::get<1>(*assign);
		lineNum++;
	}

	return assigns;
}

void
TestCase::toFile(ConcreteStore store, std::ofstream &stream)
{
	for (auto assign : store) {
		// Output variable name
		stream << assign.first << "\t";

		IntValue v = assign.second;
		if (std::get_if<uint8_t>(&v)) {
			stream << "uint8_t\t" << std::dec << +std::get<uint8_t>(v);
		} else if (std::get_if<uint32_t>(&v)) {
			stream << "uint32_t\t" << std::dec << +std::get<uint32_t>(v);
		} else {
			assert(0);
		}

		stream << std::endl;
	}
}
