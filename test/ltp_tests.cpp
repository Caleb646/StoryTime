#include <iostream>
#include <fstream>
#include <stdarg.h>
#include <cassert>
#include <vector>

#include <gtest/gtest.h>

#include "types.h"
#include "utils.h"
#include "core.h"
#include "ndb.h"
#include "ltp.h"

namespace ltp_tests
{
	using namespace reader::types;
	using namespace reader::utils;
	using namespace reader::core;
	using namespace reader::ndb;
	using namespace reader::ltp;

	const std::vector<byte_t> sample_HN = {
	0xEC, 0x00, 0xEC, 0xBC, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB5, 0x02, 0x06, 0x00,
	0x40, 0x00, 0x00, 0x00, 0x34, 0x0E, 0x02, 0x01, 0xA0, 0x00, 0x00, 0x00, 0x38, 0x0E, 0x03, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xF9, 0x0F, 0x02, 0x01, 0x60, 0x00, 0x00, 0x00, 0x01, 0x30, 0x1F, 0x00,
	0x80, 0x00, 0x00, 0x00, 0xDF, 0x35, 0x03, 0x00, 0x89, 0x00, 0x00, 0x00, 0xE0, 0x35, 0x02, 0x01,
	0xC0, 0x00, 0x00, 0x00, 0xE3, 0x35, 0x02, 0x01, 0x00, 0x01, 0x00, 0x00, 0xE7, 0x35, 0x02, 0x01,
	0xE0, 0x00, 0x00, 0x00, 0x33, 0x66, 0x0B, 0x00, 0x01, 0x00, 0x00, 0x00, 0xFA, 0x66, 0x03, 0x00,
	0x0D, 0x00, 0x0E, 0x00, 0xFF, 0x67, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x9D, 0xB5, 0x0A,
	0xDC, 0xD9, 0x94, 0x43, 0x85, 0xDE, 0x90, 0xAE, 0xB0, 0x7D, 0x12, 0x70, 0x55, 0x00, 0x4E, 0x00,
	0x49, 0x00, 0x43, 0x00, 0x4F, 0x00, 0x44, 0x00, 0x45, 0x00, 0x31, 0x00, 0x01, 0x00, 0x00, 0x00,
	0xF5, 0x5E, 0xF6, 0x66, 0x95, 0x69, 0xCC, 0x4C, 0x83, 0xD1, 0xD8, 0x73, 0x98, 0x99, 0x02, 0x85,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x9D, 0xB5, 0x0A, 0xDC, 0xD9, 0x94, 0x43,
	0x85, 0xDE, 0x90, 0xAE, 0xB0, 0x7D, 0x12, 0x70, 0x22, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x22, 0x9D, 0xB5, 0x0A, 0xDC, 0xD9, 0x94, 0x43, 0x85, 0xDE, 0x90, 0xAE, 0xB0, 0x7D, 0x12, 0x70,
	0x42, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x9D, 0xB5, 0x0A, 0xDC, 0xD9, 0x94, 0x43,
	0x85, 0xDE, 0x90, 0xAE, 0xB0, 0x7D, 0x12, 0x70, 0x62, 0x80, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
	0x0C, 0x00, 0x14, 0x00, 0x6C, 0x00, 0x7C, 0x00, 0x8C, 0x00, 0xA4, 0x00, 0xBC, 0x00, 0xD4, 0x00,
	0xEC, 0x00
	};




	TEST(HNTests, HNHDRTest)
	{
		const HNHDR hnhdr = HN::readHNHDR(sample_HN, 0, 1);
		ASSERT_EQ(hnhdr.bSig, 0xEC);
		ASSERT_EQ(hnhdr.bClientSig, 0xBC);
		ASSERT_EQ(hnhdr.hidUserRoot.getHIDRaw(), 0x00000020);
		ASSERT_EQ(hnhdr.ibHnpm, 0x00EC);
	}

}; // end namespace ltp_tests