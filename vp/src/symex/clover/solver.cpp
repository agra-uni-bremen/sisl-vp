#include <assert.h>
#include <stdlib.h>

#include <clover/clover.h>

#include <klee/Expr/Constraints.h>
#include <klee/Expr/ExprUtil.h>
#include <klee/Expr/Parser/Parser.h>

#include "fns.h"

using namespace clover;

Solver::Solver(klee::Solver *_solver)
{
	if (!_solver)
		_solver = klee::createCoreSolver(klee::CoreSolverType::Z3_SOLVER);

	// Create fancy solver chain based on given core solver.
	// Taken from lib/Solver/ConstructSolverChain.cpp
	_solver = klee::createFastCexSolver(_solver);
	_solver = klee::createCexCachingSolver(_solver);
	_solver = klee::createCachingSolver(_solver);
	_solver = klee::createIndependentSolver(_solver);

	// Copied from tools/kleaver/main.cpp
	builder = klee::createDefaultExprBuilder();
	builder = createConstantFoldingExprBuilder(builder);
	builder = createSimplifyingExprBuilder(builder);

	this->solver = _solver;
	return;
}

Solver::~Solver(void)
{
	delete this->solver;
	delete this->builder;
}

void
Solver::setTimeout(klee::time::Span timeout)
{
	this->solver->setCoreSolverTimeout(timeout);
}

std::optional<klee::Assignment>
Solver::getAssignment(const klee::Query &query)
{
	/* KLEE is concerned with validity of queries. To find a
	 * statisfiable assignment for a query it needs to be negated. */
	auto nq = query.negateExpr();

	std::vector<const klee::Array *> objects;
	klee::findSymbolicObjects(nq.expr, objects);
	for (auto e : nq.constraints) {
		klee::findSymbolicObjects(e, objects);
	}

	// Check if the is alwyas false (e.g. true due to the negation)
	// and return if it is. Otherwise triggers an assert statement
	// in the getAllIndependentConstraintsSets function.
	auto ce = dyn_cast<klee::ConstantExpr>(nq.expr);
	if (ce && ce->isTrue())
		return std::nullopt;

	std::vector<std::vector<unsigned char>> values;
	if (!solver->getInitialValues(nq, objects, values))
		return std::nullopt; /* unsat */

	return klee::Assignment(objects, values);
}

bool
Solver::eval(const klee::Query &query)
{
	klee::Solver::Validity v;

	if (!solver->evaluate(query, v))
		throw std::runtime_error("solver failed to evaluate query");

	switch (v) {
	case klee::Solver::True:
		return true;
	case klee::Solver::False:
		return false;
	case klee::Solver::Unknown:
		throw std::logic_error("unknown solver result");
	}

	assert(0); /* unreachable */
	return false;
}

std::shared_ptr<ConcolicValue>
Solver::BVC(std::optional<std::string> name, IntValue value)
{
	auto concrete = std::make_shared<BitVector>(BitVector(value));
	if (!name.has_value()) {
		auto concolic = ConcolicValue(builder, concrete);
		return std::make_shared<ConcolicValue>(concolic);
	}

	auto array = array_cache.CreateArray(*name, intByteSize(value));
	auto symbolic = std::make_shared<BitVector>(BitVector(array));

	auto concolic = ConcolicValue(builder, concrete, symbolic);
	return std::make_shared<ConcolicValue>(concolic);
}

std::shared_ptr<ConcolicValue>
Solver::BVC(uint8_t *buf, size_t buflen, bool lsb)
{
	std::shared_ptr<ConcolicValue> result = nullptr;
	for (size_t i = 0; i < buflen; i++) {
		auto byte = BVC(std::nullopt, (uint8_t)buf[i]);
		if (!result) {
			result = byte;
		} else {
			if (lsb) // ReadLSB
				result = result->concat(byte);
			else // ReadMSB
				result = byte->concat(result);
		}
	}

	return result;
}

void
Solver::BVCToBytes(std::shared_ptr<ConcolicValue> value, uint8_t *buf, size_t buflen)
{
	if (value->getWidth() < buflen * 8)
		value = value->zext(buflen * 8);

	for (size_t i = 0; i < buflen; i++) {
		// Extract expression works on bit indicies and bit sizes
		auto byte = value->extract(i * 8, klee::Expr::Int8);
		buf[i] = getValue<uint8_t>(byte->concrete);
	}
}

std::shared_ptr<BitVector>
Solver::fromString(Env env, std::string kquery)
{
	auto mb = llvm::MemoryBuffer::getMemBuffer(kquery.c_str());
	auto par = klee::expr::Parser::Create(__FUNCTION__, mb.get(), builder, false);

	std::map<std::string, klee::expr::ExprHandle> vars;
	for (auto p : env)
		vars[p.first] = p.second->expr;

	auto expr = par->ParseSingleConstraint(vars);
	return std::make_shared<BitVector>(BitVector(expr));
}
