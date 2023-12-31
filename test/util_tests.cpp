#include <iostream>
#include <fstream>
#include <stdarg.h>
#include <cassert>
#include <vector>

#include <gtest/gtest.h>

#include "types.h"
#include "utils.h"


namespace util_tests
{
	using namespace storyt::types;
	using namespace storyt::utils;

	const std::vector<byte_t> testData = {
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


	TEST(UtilTests, PadTest)
	{
		{
			const std::vector<byte_t> A = { 1, 2, 3, 4, 5 };
			const std::vector<byte_t> B = { 1, 2, 3, 4, 5, 0, 0, 0 };
			const std::vector<byte_t> C = pad(A, 3);
			ASSERT_EQ(B, C);
		}
	}

	TEST(UtilTests, ToT_LTest)
	{
		{
			const std::vector<byte_t> A = { 0x01 };
			const uint8_t B = 1;
			const uint8_t C = toT_l<uint8_t>(A);
			ASSERT_EQ(B, C);
		}
		{
			const std::vector<byte_t> A = { 0x01, 0x01 };
			const uint16_t B = 257;
			const uint16_t C = toT_l<uint16_t>(A);
			ASSERT_EQ(B, C);
		}
		{
			const std::vector<byte_t> A = { 0x01, 0x01, 0x01 };
			const uint32_t B = 65793;
			const uint32_t C = toT_l<uint32_t>(A);
			ASSERT_EQ(B, C);
		}
		{
			const std::vector<byte_t> A = { 0x01, 0x01, 0x01, 0x01 };
			const uint32_t B = 16843009;
			const uint32_t C = toT_l<uint32_t>(A);
			ASSERT_EQ(B, C);
		}
		{
			const std::vector<byte_t> A = { 0x01, 0x01, 0x01, 0x01, 0x01 };
			const uint64_t B = 4311810305;
			const uint64_t C = toT_l<uint64_t>(A);
			ASSERT_EQ(B, C);
		}
		{
			const std::vector<byte_t> A = { 0x01, 0x01, 0x01, 0x01, 0x01, 0x01 };
			const uint64_t B = 1103823438081;
			const uint64_t C = toT_l<uint64_t>(A);
			ASSERT_EQ(B, C);
		}
		{
			const std::vector<byte_t> A = { 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01 };
			const uint64_t B = 282578800148737;
			const uint64_t C = toT_l<uint64_t>(A);
			ASSERT_EQ(B, C);
		}
		{
			const std::vector<byte_t> A = { 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01 };
			const uint64_t B = 72340172838076673;
			const uint64_t C = toT_l<uint64_t>(A);
			ASSERT_EQ(B, C);
		}
	}

	TEST(UtilTests, SliceTest)
	{
		{
			std::vector<byte_t> A = { 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01 };
			std::vector<uint64_t> B = {1, 257, 65793, 16843009, 4311810305, 1103823438081, 282578800148737, 72340172838076673 };
			for (int i = 0; i < A.size(); ++i)
			{
				const uint64_t C = slice(A, 0, i+1, i+1, toT_l<uint64_t>);
				ASSERT_EQ(B[i], C);
			}
		}
	}

	TEST(UtilTests, EncodingDecodingTest)
	{
		std::vector<byte_t> A(testData);
		const std::vector<byte_t> B(testData);
		ASSERT_EQ(A, B);
		ms::CryptPermute(A.data(), static_cast<int>(A.size()), ms::ENCODE_DATA);
		ASSERT_NE(A, B);
		ms::CryptPermute(A.data(), static_cast<int>(A.size()), ms::DECODE_DATA);
		ASSERT_EQ(A, B);
	}
}; // end namespace util_tests

