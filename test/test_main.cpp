
#include <iostream>
#include <fstream>
#include <stdarg.h>
#include <cassert>
#include <vector>

#include <gtest/gtest.h>

#include "types_tests.cpp"
#include "util_tests.cpp"
#include "ndb_tests.cpp"
#include "ltp_tests.cpp"

int main(int argc, char** argv)
{   
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
