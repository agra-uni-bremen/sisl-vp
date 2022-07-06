#ifndef CLOVER_H
#define CLOVER_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include <klee/Expr/ArrayCache.h>
#include <klee/Expr/Assignment.h>
#include <klee/Expr/Expr.h>
#include <klee/Expr/ExprBuilder.h>
#include <klee/Solver/Solver.h>

#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <variant>

namespace clover {

typedef std::variant<uint8_t, uint32_t> IntValue;

/* Raised when a new assertion was added to the execution tree
 * and new values need to be determined for all concolic values
 * via ExecutionContext::setupNewValues(). */
class AssumeNotification {
	std::string what;

public:
	AssumeNotification(const std::string &reason)
	    : what(reason)
	{
		return;
	}
};

class BitVector {
public:
	klee::ref<klee::Expr> expr;

	BitVector(const klee::ref<klee::Expr> &expr);
	BitVector(void);
	BitVector(IntValue value);
	BitVector(const klee::Array *array);

	std::shared_ptr<BitVector> eqTrue(void);
	std::shared_ptr<BitVector> eqFalse(void);

	friend class ConcolicValue;
	friend class Solver;
	friend class Trace;
};

class ConcolicValue {
public:
	std::shared_ptr<BitVector> concrete;
	std::optional<std::shared_ptr<BitVector>> symbolic;

	unsigned getWidth(void);

	std::shared_ptr<ConcolicValue> eq(std::shared_ptr<ConcolicValue> other);
	std::shared_ptr<ConcolicValue> ne(std::shared_ptr<ConcolicValue> other);
	std::shared_ptr<ConcolicValue> lshl(std::shared_ptr<ConcolicValue> other);
	std::shared_ptr<ConcolicValue> lshr(std::shared_ptr<ConcolicValue> other);
	std::shared_ptr<ConcolicValue> ashr(std::shared_ptr<ConcolicValue> other);
	std::shared_ptr<ConcolicValue> add(std::shared_ptr<ConcolicValue> other);
	std::shared_ptr<ConcolicValue> mul(std::shared_ptr<ConcolicValue> other);
	std::shared_ptr<ConcolicValue> udiv(std::shared_ptr<ConcolicValue> other);
	std::shared_ptr<ConcolicValue> sdiv(std::shared_ptr<ConcolicValue> other);
	std::shared_ptr<ConcolicValue> urem(std::shared_ptr<ConcolicValue> other);
	std::shared_ptr<ConcolicValue> srem(std::shared_ptr<ConcolicValue> other);
	std::shared_ptr<ConcolicValue> sub(std::shared_ptr<ConcolicValue> other);
	std::shared_ptr<ConcolicValue> slt(std::shared_ptr<ConcolicValue> other);
	std::shared_ptr<ConcolicValue> sge(std::shared_ptr<ConcolicValue> other);
	std::shared_ptr<ConcolicValue> ult(std::shared_ptr<ConcolicValue> other);
	std::shared_ptr<ConcolicValue> ule(std::shared_ptr<ConcolicValue> other);
	std::shared_ptr<ConcolicValue> uge(std::shared_ptr<ConcolicValue> other);
	std::shared_ptr<ConcolicValue> band(std::shared_ptr<ConcolicValue> other);
	std::shared_ptr<ConcolicValue> bor(std::shared_ptr<ConcolicValue> other);
	std::shared_ptr<ConcolicValue> bxor(std::shared_ptr<ConcolicValue> other);
	std::shared_ptr<ConcolicValue> concat(std::shared_ptr<ConcolicValue> other);
	std::shared_ptr<ConcolicValue> bnot(void);
	std::shared_ptr<ConcolicValue> extract(unsigned offset, klee::Expr::Width width);
	std::shared_ptr<ConcolicValue> sext(klee::Expr::Width width);
	std::shared_ptr<ConcolicValue> zext(klee::Expr::Width width);
	std::shared_ptr<ConcolicValue> select(std::shared_ptr<ConcolicValue> texpr, std::shared_ptr<ConcolicValue> fexpr);

private:
	klee::ExprBuilder *builder = NULL;

	ConcolicValue(klee::ExprBuilder *_builder,
	              std::shared_ptr<BitVector> _concrete,
	              std::optional<std::shared_ptr<BitVector>> _symbolic = std::nullopt);

	/* The solver acts as a factory for ConcolicValue */
	friend class Solver;
};

class Solver {
private:
	klee::Solver *solver;
	klee::ArrayCache array_cache;
	klee::ExprBuilder *builder = NULL;

public:
	Solver(klee::Solver *_solver = NULL);
	~Solver(void);

	void setTimeout(klee::time::Span timeout);
	std::optional<klee::Assignment> getAssignment(const klee::Query &query);

	bool eval(const klee::Query &query);
	std::shared_ptr<ConcolicValue> BVC(std::optional<std::string> name, IntValue value);

	/* Methods for converting between concolic values and uint8_t buffers */
	std::shared_ptr<ConcolicValue> BVC(uint8_t *buf, size_t buflen, bool lsb = false);
	void BVCToBytes(std::shared_ptr<ConcolicValue> value, uint8_t *buf, size_t buflen);

	typedef std::map<std::string, std::shared_ptr<BitVector>> Env;
	std::shared_ptr<BitVector> fromString(Env env, std::string kquery);

	template <typename T>
	T evalValue(const klee::Query &query)
	{
		klee::ref<klee::ConstantExpr> r;

		if (!solver->getValue(query, r))
			throw std::runtime_error("getValue() failed for solver");

		return (T)r->getZExtValue(sizeof(T) * 8);
	}

	/* Convert the concrete part of a ConcolicValue to a C type. */
	template <typename T>
	T getValue(std::shared_ptr<BitVector> bv)
	{
		// Since we don't have constraints here, these function
		// only works on ConstantExpr as provided by ->concrete.
		klee::ConstantExpr *ce = dyn_cast<klee::ConstantExpr>(bv->expr);
		assert(ce && "getValue only works on constants");

		return ce->getZExtValue(sizeof(T) * 8);
	}
};

class ConcolicMemory {
private:
	typedef uint32_t Addr;

	Solver &solver;
	std::unordered_map<Addr, std::shared_ptr<ConcolicValue>> data;

public:
	ConcolicMemory(Solver &_solver);
	void reset(void);

	std::shared_ptr<ConcolicValue> load(Addr addr, unsigned bytesize);
	std::shared_ptr<ConcolicValue> load(std::shared_ptr<ConcolicValue> addr, unsigned bytesize);

	void store(Addr addr, std::shared_ptr<ConcolicValue> value, unsigned bytesize);
	void store(std::shared_ptr<ConcolicValue> addr, std::shared_ptr<ConcolicValue> value, unsigned bytesize);
};

typedef std::map<std::string, IntValue> ConcreteStore;

/**
 * The Tracer fullfills two tasks:
 *
 *   1. It iteratively creates an execution tree where each node
 *      constitutes a branch condition. This tree is then used
 *      to find new assignments for symbolic input variables
 *      based on a Dynamic Symbolic Execution (DSE) algorithm.
 *
 *   2. It tracks the current constraints for the currently
 *      executed path for the program. As such, allowing the
 *      creation of properly constrained queries using getQuery().
 *      These queries can then be solved using the Solver class.
 */
class Trace {
private:
	class Branch {
	public:
		std::shared_ptr<BitVector> bv;

		// Track if this negation of this branch condition was
		// already attempted. Negating the same branch condition
		// twice (even if a true/false branch) was not discovered
		// yet must be avoided as the negated branch condition
		// could be unsat.
		bool wasNegated;

		// Address of branch instruction for the associated
		// branch condition represented by the BitVector.
		uint32_t addr;

		Branch(std::shared_ptr<BitVector> _bv, bool _wasNegated, uint32_t _addr)
		    : bv(_bv), wasNegated(_wasNegated), addr(_addr)
		{
			return;
		}
	};

	typedef std::pair<std::shared_ptr<Branch>, bool> PathElement;
	typedef std::vector<PathElement> Path;

	class Node {
	public:
		std::shared_ptr<Branch> value;

		Node *true_branch;
		Node *false_branch;

		Node(void);
		bool isPlaceholder(void);

		/* Returns a seemingly random unnegated path to a branch
		 * condition in the Tree but prefers nodes in the upper
		 * Tree. The caller is responsible for updating the wasNegated
		 * member of the last element of the path, if the caller actually
		 * decides to negate the branch condition the path leads to.
		 *
		 * Returns false if no unnegated branch condition exists. */
		bool randomUnnegated(Path &path);
	};

	Solver &solver;
	klee::ConstraintSet cs;
	klee::ConstraintManager cm;

	Node *pathCondsRoot;
	Node *pathCondsCurrent;

	/* Create new query for path in execution tree. */
	klee::Query newQuery(klee::ConstraintSet &cs, Path &path);

	/* Add a new node to the execution tree and the constraint set.*/
	bool addBranch(std::shared_ptr<Branch> branch, bool condition);

public:
	Trace(Solver &_solver);
	~Trace(void);
	void reset(void);

	/* Add branch node to tree which can (potentially) be either true or false. */
	void add(bool condition, std::shared_ptr<BitVector> bv, uint32_t pc);

	/* Enforce the given symbolic constraint to be always true.
	 * Using this method requires handling thrown ::AssumeNotification
	 * in the main execution loop. */
	void assume(std::shared_ptr<BitVector> bv);

	/* Create query from BitVector with currently tracked constraints. */
	klee::Query getQuery(std::shared_ptr<BitVector> bv);

	std::optional<klee::Assignment> findNewPath(void);
	ConcreteStore getStore(const klee::Assignment &assign);
};

class ExecutionContext {
private:
	// Variable assignment for next invocation of getSymbolic().
	ConcreteStore next_run;

	// Variable assignment for the last invocation of getSymbolic().
	ConcreteStore last_run;

	Solver &solver;

	template <typename T>
	IntValue findRemoveOrRandom(std::string name)
	{
		IntValue concrete;

		auto iter = next_run.find(name);
		if (iter != next_run.end()) {
			concrete = (*iter).second;
			assert(std::get_if<T>(&concrete) != nullptr);
			next_run.erase(iter);
		} else {
			concrete = (T)rand();
		}

		last_run[name] = concrete;
		return concrete;
	}

public:
	ExecutionContext(Solver &_solver);
	ConcreteStore getPrevStore(void);

	bool setupNewValues(ConcreteStore store);
	bool setupNewValues(Trace &trace);

	std::shared_ptr<ConcolicValue> getSymbolicWord(std::string name);
	std::shared_ptr<ConcolicValue> getSymbolicBytes(std::string name, size_t size);
	std::shared_ptr<ConcolicValue> getSymbolicByte(std::string name);
};

class TestCase {
	class ParserError : public std::exception {
		std::string fileName, msg, whatstr;
		size_t line;

	public:
		ParserError(std::string _fileName, size_t _line, std::string _msg)
		    : fileName(_fileName), msg(_msg), line(_line)
		{
			this->whatstr = fileName + ":" + std::to_string(line) + ": " + msg;
		}

		const char *what(void) const throw()
		{
			return whatstr.c_str();
		}
	};

public:
	static ConcreteStore fromFile(std::string name, std::ifstream &stream);
	static void toFile(ConcreteStore store, std::ofstream &stream);
};

}; // namespace clover

#endif
