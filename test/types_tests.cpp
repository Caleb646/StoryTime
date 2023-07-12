#include <iostream>
#include <fstream>
#include <stdarg.h>
#include <cassert>
#include <vector>

#include <gtest/gtest.h>

#include "types.h"

namespace types_tests
{
	using namespace reader::types;
	TEST(TypesTest, TypesSizeTest)
	{
		static_assert(sizeof(byte_t) == 1);
	}

}; // end namespace types_tests

