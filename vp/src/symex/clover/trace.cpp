#include <queue>

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#include <iostream>

#include <clover/clover.h>
#include <klee/Expr/Constraints.h>
#include <klee/Expr/ExprUtil.h>

#include "fns.h"

using namespace clover;

Trace::Trace(Solver &_solver)
    : solver(_solver), cm(cs)
{
	pathCondsRoot = new Node;
	pathCondsCurrent = nullptr;
}

Trace::~Trace(void)
{
	std::queue<Node *> nodes;

	nodes.push(pathCondsRoot);
	while (!nodes.empty()) {
		Node *node = nodes.front();
		nodes.pop();

		if (node->true_branch)
			nodes.push(node->true_branch);
		if (node->false_branch)
			nodes.push(node->false_branch);

		delete node;
	}
}

void
Trace::reset(void)
{
	cs = klee::ConstraintSet();
	pathCondsCurrent = nullptr;
}

bool
Trace::addBranch(std::shared_ptr<Trace::Branch> branch, bool condition)
{
	bool ret = false;

	Node *node = nullptr;
	if (pathCondsCurrent != nullptr) {
		node = pathCondsCurrent;
	} else {
		node = pathCondsRoot;
	}

	assert(node);
	if (node->isPlaceholder()) {
		node->value = branch;
		ret = true;
	}

	if (condition) {
		if (!node->true_branch)
			node->true_branch = new Node;
		pathCondsCurrent = node->true_branch;
	} else {
		if (!node->false_branch)
			node->false_branch = new Node;
		pathCondsCurrent = node->false_branch;
	}

	return ret;
}

void
Trace::add(bool condition, std::shared_ptr<BitVector> bv, uint32_t pc)
{
	auto c = (condition) ? bv->eqTrue() : bv->eqFalse();
	cm.addConstraint(c->expr);

	auto br = std::make_shared<Branch>(Branch(bv, false, pc));
	addBranch(br, condition);
}

void
Trace::assume(std::shared_ptr<BitVector> bv)
{
	// Enforce condition as true for this execution path.
	auto c = bv->eqTrue();
	cm.addConstraint(c->expr);

	// Add a negated version of the assume condititon to the
	// execution tree. Assuming that it is the only node in the
	// tree, it will be negated by the solver during execution
	// restart and thus enforced from this point onwards.
	auto negated_assume = bv->eqFalse();
	auto br = std::make_shared<Branch>(Branch(negated_assume, false, 0));
	if (addBranch(br, false)) {
		// If we add a new node to the tree then this is the
		// first time this constraint gets enforced and we
		// need to find new assignments for all concolic values.
		throw AssumeNotification("new assertion added to execution tree");
	}
}

klee::Query
Trace::getQuery(std::shared_ptr<BitVector> bv)
{
	auto expr = cm.simplifyExpr(cs, bv->expr);
	return klee::Query(cs, expr);
}

klee::Query
Trace::newQuery(klee::ConstraintSet &cs, Path &path)
{
	size_t query_idx = path.size() - 1;
	auto cm = klee::ConstraintManager(cs);

	for (size_t i = 0; i < path.size(); i++) {
		auto branch = path.at(i).first;
		auto cond = path.at(i).second;

		auto bv = branch->bv;
		auto bvcond = (cond) ? bv->eqTrue() : bv->eqFalse();

		if (i < query_idx) {
			cm.addConstraint(bvcond->expr);
			continue;
		}

		auto expr = cm.simplifyExpr(cs, bvcond->expr);

		// This is the last expression on the path. By negating
		// it we can potentially discover a new path.
		branch->wasNegated = true;
		return klee::Query(cs, expr).negateExpr();
	}

	throw "unreachable";
}

std::optional<klee::Assignment>
Trace::findNewPath(void)
{
	std::optional<klee::Assignment> assign;

	do {
		klee::ConstraintSet cs;

		Path path;
		if (!pathCondsRoot->randomUnnegated(path))
			return std::nullopt; /* all branches exhausted */

		auto query = newQuery(cs, path);
		/* std::cout << "Attempting to negate new query at: 0x" << std::hex << path.back().first->addr << std::dec << std::endl; */
		assign = solver.getAssignment(query);
	} while (!assign.has_value()); /* loop until we found a sat assignment */

	assert(assign.has_value());
	return assign;
}

ConcreteStore
Trace::getStore(const klee::Assignment &assign)
{
	ConcreteStore store;
	for (auto const &b : assign.bindings) {
		auto array = b.first;
		auto value = b.second;

		std::string name = array->getName();
		store[name] = intFromVector(value);
	}

	return store;
}
