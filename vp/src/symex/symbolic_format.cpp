/*
 * Copyright (c) 2021 Group of Computer Architecture, University of Bremen
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

#include <vector>
#include <exception>
#include <iostream>

#include <err.h>
#include <limits.h>
#include <assert.h>
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/mman.h>

#include "symbolic_format.h"
#include "symbolic_context.h"

// True if the given bit size is NOT aligned on a byte boundary.
#define HAS_PADDING(BITSIZE) (BITSIZE % CHAR_BIT != 0)

static ssize_t
readfile(const char **dest, const char *fp)
{
	int fd;
	off_t len;
	struct stat st;

	if ((fd = open(fp, O_RDONLY)) == -1)
		return -1;
	if (fstat(fd, &st))
		return -1;
	if ((len = st.st_size) <= 0)
		return 0;

	*dest = (const char*)mmap(NULL, (size_t)len, PROT_READ, MAP_PRIVATE, fd, 0);
	if (*dest == MAP_FAILED)
		return -1;

	close(fd);
	return len;
}

static size_t
to_byte_size(uint64_t bitsize)
{
	uint64_t rem;
	size_t bytesize;

	// Round to next byte boundary.
	if ((rem = bitsize % CHAR_BIT) == 0)
		bytesize = bitsize / CHAR_BIT;
	else
		bytesize = ((bitsize - rem) + CHAR_BIT) / CHAR_BIT;

	return bytesize;
}

SymbolicFormat::SymbolicFormat(SymbolicContext &_ctx, std::string path)
  : ctx(_ctx.ctx), solver(_ctx.solver)
{
	if (path.empty()) {
		input_str = nullptr;
		return;
	}

	if ((input_len = readfile(&input_str, path.c_str())) == -1)
		throw std::system_error(errno, std::generic_category());
	if (input_len == 0) {
		input_str = nullptr;
		return;
	}

	assert(input_len <= INT_MAX);
	bencode_init(&bencode, input_str, (int)input_len);

	input = get_input();
	offset = input->getWidth();

	return;
}

SymbolicFormat::~SymbolicFormat(void)
{
	if (!input_str)
		return;

	if (munmap((void *)input_str, input_len) == -1)
		err(EXIT_FAILURE, "munmap failed");
}

std::shared_ptr<clover::ConcolicValue>
SymbolicFormat::make_symbolic(std::string name, uint64_t bitsize, size_t bytesize)
{
	auto symbolic_value = ctx.getSymbolicBytes(name, bytesize);
	if (HAS_PADDING(bitsize))
		symbolic_value = symbolic_value->extract(0, bitsize);

	env[name] = *(symbolic_value->symbolic);
	return symbolic_value;
}

std::optional<long int>
SymbolicFormat::get_size(bencode_t *list_elem)
{
	long int value;
	bencode_t size_elem;

	if (bencode_list_get_next(list_elem, &size_elem) != 1)
		return std::nullopt;
	if (!bencode_is_int(&size_elem))
		return std::nullopt;

	if (!bencode_int_value(&size_elem, &value))
		return std::nullopt;

	return value;
}

std::optional<std::string>
SymbolicFormat::get_name(bencode_t *list_elem)
{
	bencode_t name_elem;
	const char *dest;
	int dest_len;

	if (bencode_list_get_next(list_elem, &name_elem) != 1)
		return std::nullopt;
	if (!bencode_is_string(&name_elem))
		return std::nullopt;

	if (!bencode_string_value(&name_elem, &dest, &dest_len))
		return std::nullopt;

	std::string ret(dest, dest_len);
	return ret;
}

std::optional<std::shared_ptr<clover::ConcolicValue>>
SymbolicFormat::get_value(bencode_t *list_elem, std::string name, uint64_t bitsize)
{
	size_t bytesize;
	bencode_t value_elem;
	int is_symbolic;
	std::vector<uint8_t> concrete_value;
	std::shared_ptr<clover::ConcolicValue> symbolic_value;

	if (bencode_list_get_next(list_elem, &value_elem) != 1)
		return std::nullopt;
	if (!bencode_is_list(&value_elem))
		return std::nullopt;
	bytesize = to_byte_size(bitsize);

	is_symbolic = -1;
	while (bencode_list_has_next(&value_elem)) {
		bencode_t list_elem;
		long int int_value;
		const char *str_value;
		int str_length;

		if (bencode_list_get_next(&value_elem, &list_elem) != 1)
			return std::nullopt;

		if (is_symbolic == -1) {
			is_symbolic = bencode_is_string(&list_elem);
			if (is_symbolic)
				symbolic_value = make_symbolic(name, bitsize, bytesize);
		}

		if (is_symbolic) {
			if (!bencode_is_string(&list_elem))
				return std::nullopt;
			if (!bencode_string_value(&list_elem, &str_value, &str_length))
				return std::nullopt;

			std::string constraint(str_value, str_length);
			auto bv = solver.fromString(env, constraint);

			// Enforce parsed constraint via symbolic_context.
			// TODO: Build full Env first and constrain after.
			symbolic_context.assume(bv);
		} else { // is_concrete
			if (!bencode_is_int(&list_elem))
				return std::nullopt;
			if (!bencode_int_value(&list_elem, &int_value))
				return std::nullopt;

			if (int_value > UINT8_MAX)
				return std::nullopt;
			concrete_value.push_back((uint8_t)int_value);
		}
	}

	if (is_symbolic == -1) // unconstrained symbolic value
		return make_symbolic(name, bitsize, bytesize);

	if (is_symbolic) {
		return symbolic_value;
	} else {
		if (concrete_value.size() != bytesize)
			return std::nullopt;

		auto bvc = solver.BVC(concrete_value.data(), concrete_value.size(), true);
		if (HAS_PADDING(bitsize))
			bvc = bvc->extract(0, bitsize);
		return bvc;
	}
}

std::shared_ptr<clover::ConcolicValue>
SymbolicFormat::next_field(void)
{
	bencode_t field_value;
	long int bitsize;
	std::string name;
	std::shared_ptr<clover::ConcolicValue> value;

	if (!bencode_list_has_next(&bencode))
		return nullptr;
	if (bencode_list_get_next(&bencode, &field_value) != 1)
		throw std::out_of_range("unexpected end of bencode list");
	if (!bencode_is_list(&field_value))
		throw std::invalid_argument("invalid bencode field type");

	auto n = get_name(&field_value);
	if (!n.has_value())
		throw std::invalid_argument("invalid bencode name field");
	name = std::move(*n);

	auto s = get_size(&field_value);
	if (!s.has_value())
		throw std::invalid_argument("invalid bencode size field");
	bitsize = *s;

	auto v = get_value(&field_value, name, bitsize);
	if (!v.has_value())
		throw std::invalid_argument("invalid bencode value field");
	value = *v;

	return value;
}

std::shared_ptr<clover::ConcolicValue>
SymbolicFormat::get_input(void)
{
	std::shared_ptr<clover::ConcolicValue> field, r = nullptr;

	while ((field = next_field())) {
		if (!r) {
			r = field;
			continue;
		}
		r = r->concat(field);
	}

	assert(r != nullptr);
	assert(r->getWidth() % CHAR_BIT == 0);

	return r;
}

std::shared_ptr<clover::ConcolicValue>
SymbolicFormat::next_byte(void)
{
	if (!input_str || offset == 0)
		return nullptr;

	assert(offset % CHAR_BIT == 0);
	offset -= CHAR_BIT;

	auto byte = input->extract(offset, CHAR_BIT);
	assert(byte->getWidth() == CHAR_BIT);

	return byte;
}

size_t
SymbolicFormat::remaning_bytes(void)
{
	if (!input_str || offset == 0)
		return 0; // empty

	auto width = input->getWidth();
	assert(width % CHAR_BIT == 0);

	return (width - (width - offset)) / CHAR_BIT;
}
