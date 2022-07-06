#include <iostream>

#include <clover/clover.h>
using namespace clover;

ConcolicMemory::ConcolicMemory(Solver &_solver)
    : solver(_solver)
{
	return;
}

void
ConcolicMemory::reset(void)
{
	data.clear();
}

std::shared_ptr<ConcolicValue>
ConcolicMemory::load(Addr addr, unsigned bytesize)
{
	std::shared_ptr<ConcolicValue> result = nullptr;
	for (uint32_t off = 0; off < bytesize; off++) {
		auto read_addr = addr + off;

		std::shared_ptr<ConcolicValue> byte;
		if (data.count(read_addr)) {
			byte = data.at(read_addr);
		} else {
			std::cerr << "WARNING: Uninitialized memory accessed at 0x"
			          << std::hex << read_addr << " initializing with zero" << std::endl;
			byte = solver.BVC(std::nullopt, (uint8_t)0);
		}

		if (!result) {
			result = byte;
		} else {
			result = byte->concat(result);
		}
	}

	return result;
}

std::shared_ptr<ConcolicValue>
ConcolicMemory::load(std::shared_ptr<ConcolicValue> addr, unsigned bytesize)
{
	auto base_addr = solver.getValue<ConcolicMemory::Addr>(addr->concrete);
	return load(base_addr, bytesize);
}

void
ConcolicMemory::store(Addr addr, std::shared_ptr<ConcolicValue> value, unsigned bytesize)
{
	if (value->getWidth() < bytesize * 8)
		value = value->zext(bytesize * 8);

	for (size_t off = 0; off < bytesize; off++) {
		// Extract expression works on bit indicies, not bytes.
		data[addr + off] = value->extract(off * 8, klee::Expr::Int8);
	}
}

void
ConcolicMemory::store(std::shared_ptr<ConcolicValue> addr, std::shared_ptr<ConcolicValue> value, unsigned bytesize)
{
	auto base_addr = solver.getValue<ConcolicMemory::Addr>(addr->concrete);
	return store(base_addr, value, bytesize);
}
