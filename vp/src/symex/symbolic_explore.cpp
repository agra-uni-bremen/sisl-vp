/*
 * Copyright (c) 2020,2021 Group of Computer Architecture, University of Bremen
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef RISCV_ISA_EXPLORATION_H
#define RISCV_ISA_EXPLORATION_H

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

/* Debug leaks with valgrind --leak-check=full --undef-value-errors=no
 * Also: Define valgrind here to prevent spurious Z3 memory leaks. */
#ifdef VALGRIND
#include <z3.h>
#endif

#include <iostream>
#include <systemc>
#include <filesystem>
#include <systemc>

#include <clover/clover.h>
#include "symbolic_explore.h"
#include "symbolic_context.h"

#define TESTCASE_ENV "SYMEX_TESTCASE"
#define TIMEBUDGET_ENV "SYMEX_TIMEBUDGET"
#define ERR_EXIT_ENV "SYMEX_ERREXIT"

static std::filesystem::path *testcase_path = nullptr;
static size_t errors_found = 0;

static const char* assume_mtype = "/AGRA/riscv-vp/assume-notification";
static bool stopped = false;

void
symbolic_exploration::stop_assume(void)
{
	SC_REPORT_ERROR(assume_mtype, "AssumeNotification");
}

static std::optional<std::string>
dump_input(std::string fn)
{
	clover::ExecutionContext &ctx = symbolic_context.ctx;
	clover::ConcreteStore store = ctx.getPrevStore();
	if (store.empty())
		return std::nullopt; // Execution does not depend on symbolic values

	assert(testcase_path);
	auto path = *testcase_path / fn;

	std::ofstream file(path);
	if (!file.is_open())
		throw std::runtime_error("failed to open " + path.string());

	clover::TestCase::toFile(store, file);
	return path;
}

static void
report_handler(const sc_core::sc_report& report, const sc_core::sc_actions& actions)
{
	auto mtype = report.get_msg_type();
	if (!strcmp(mtype, "/AGRA/riscv-vp/host-error") && testcase_path) {
		auto path = dump_input("error" + std::to_string(++errors_found));
		if (!path.has_value())
			return;

		std::cerr << "Found error, use " << *path << " to reproduce." << std::endl;
		if (getenv(ERR_EXIT_ENV)) {
			std::cerr << "Exit on first error set, terminating..." << std::endl;
			exit(EXIT_FAILURE);
		}

		sc_core::sc_stop();
	} else if (!strcmp(mtype, assume_mtype)) {
		stopped = true;

		// Never display message for assume notifications.
		sc_core::sc_actions new_actions = actions;
		if (new_actions & sc_core::SC_DISPLAY)
			new_actions &= ~sc_core::SC_DISPLAY;
		sc_core::sc_report_handler::default_handler(report, new_actions);
	} else {
		sc_core::sc_report_handler::default_handler(report, actions);
	}
}

static void
remove_testdir(void)
{
	assert(testcase_path != nullptr);
	if (errors_found > 0)
		return;

	// Remove test directory if no errors were found
	if (rmdir(testcase_path->c_str()) == -1)
		throw std::system_error(errno, std::generic_category());

	delete testcase_path;
	testcase_path = nullptr;
}

static void
create_testdir(void)
{
	char *dirpath;
	char tmpl[] = "/tmp/clover_testsXXXXXX";

	if (!(dirpath = mkdtemp(tmpl)))
		throw std::system_error(errno, std::generic_category());
	testcase_path = new std::filesystem::path(dirpath);

	if (std::atexit(remove_testdir))
		throw std::runtime_error("std::atexit failed");
}

static std::chrono::duration<double, std::milli> solver_time;

static bool
setupNewValues(clover::ExecutionContext &ctx, clover::Trace &tracer)
{
	auto start = std::chrono::steady_clock::now();
	auto r = ctx.setupNewValues(tracer);
	auto end = std::chrono::steady_clock::now();

	solver_time += end - start;
	return r;
}

static int
run_test(const char *path, int argc, char **argv)
{
	std::string fp(path);
	std::ifstream file(fp);
	if (!file.is_open())
		throw std::runtime_error("failed to open " + fp);

	clover::ExecutionContext &ctx = symbolic_context.ctx;
	clover::ConcreteStore store = clover::TestCase::fromFile(fp, file);

	ctx.setupNewValues(store);
	return sc_core::sc_elab_and_sim(argc, argv);
}

static size_t
explore_paths(int argc, char **argv)
{
	clover::ExecutionContext &ctx = symbolic_context.ctx;
	clover::Trace &tracer = symbolic_context.trace;

	typedef std::chrono::high_resolution_clock::time_point time_point;
	std::optional<time_point> budget;

	char *timebudget = getenv(TIMEBUDGET_ENV);
	if (timebudget) {
		budget = std::chrono::high_resolution_clock::now() +
			std::chrono::seconds(std::atoi(timebudget));
	}

	// Set stop mode for symbolic_exploration::stop.
	sc_core::sc_set_stop_mode(sc_core::SC_STOP_IMMEDIATE);

	size_t paths_found = 0;
	do {
		if (!stopped) {
			if (budget.has_value()) {
				time_point now = std::chrono::high_resolution_clock::now();
				if (now >= budget) {
					std::cout << "Time budget exceeded, terminating..." << std::endl;
					break;
				}
			}

			std::cout << std::endl << "##" << std::endl << "# "
				<< ++paths_found << "th concolic execution" << std::endl
				<< "##" << std::endl;
		}

		tracer.reset();

		// Reset SystemC simulation context
		// See also: https://github.com/accellera-official/systemc/issues/8
		if (sc_core::sc_curr_simcontext) {
			sc_core::sc_report_handler::release();
			delete sc_core::sc_curr_simcontext;
		}
		sc_core::sc_curr_simcontext = NULL;

		int ret;
		stopped = false;
		if ((ret = sc_core::sc_elab_and_sim(argc, argv)) && !stopped) {
			std::cerr << "sc_main() exited with non-zero exit status" << std::endl;
			exit(ret);
		}
	} while (setupNewValues(ctx, tracer));

	sc_core::sc_report_handler::release();
	delete sc_core::sc_curr_simcontext;

	return paths_found;
}

int
symbolic_explore(int argc, char **argv)
{
	// Hide SystemC copyright message
	setenv("SYSTEMC_DISABLE_COPYRIGHT_MESSAGE", "1", 0);

	// Mempool does not seem to free all memory, disable it.
	setenv("SYSTEMC_MEMPOOL_DONT_USE", "1", 0);

	// Use current time as seed for random generator
	std::srand(std::time(nullptr));

	char *testcase = getenv(TESTCASE_ENV);
	if (testcase)
		return run_test(testcase, argc, argv);
	create_testdir();

	// Set report handler for detecting errors
	sc_core::sc_report_handler::set_handler(report_handler);

	size_t paths_found = explore_paths(argc, argv);
	auto stime = std::chrono::duration_cast<std::chrono::seconds>(solver_time);

	std::cout << std::endl << "---" << std::endl;
	std::cout << "Unique paths found: " << paths_found << std::endl;
	std::cout << "Solver Time: " << stime.count() << " seconds" << std::endl;
	if (errors_found > 0) {
		std::cout << "Errors found: " << errors_found << std::endl;
		std::cout << "Testcase directory: " << *testcase_path << std::endl;
	}

#ifdef VALGRIND
	Z3_finalize_memory();
#endif

	return 0;
}

#endif
