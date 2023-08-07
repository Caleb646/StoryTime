#ifndef STORYT_PDF_PARSERS_H
#define STORYT_PDF_PARSERS_H
#include <cstddef>
#include <cstring>
#include <span>
#include <functional>
#include <optional>
#include <regex>

#include <storyt/common.h>
#include <storyt/pdf/tokens.h>


namespace storyt::_internal
{
	enum State : state_t
	{
		MATCHANY = 0xFFFF,
		END = 0xFFFE,
		FAILED = 0xFFFD,
		CONTINUE = 0xFFFC,
		START = 0xFFFA
	};

	struct ParseResult
	{
		int totalBytesParsed{ 0 };
		std::span<byte_t> bytesWithNoKeyWords{};
	};

	ParseResult keywordParseRegex(const std::string& startKw, const std::string& endKw, std::span<byte_t> span)
	{
		std::string_view view(reinterpret_cast<char*>(span.data()), span.size());
		std::cmatch firstMatch;
		std::cmatch endMatch;
		std::regex start(startKw);
		std::regex end(endKw);
			
		const bool firstResult = std::regex_search(view.data(), firstMatch, start);
		const bool endResult = std::regex_search(view.data(), endMatch, end);

		if (firstResult && endResult)
		{
			const int positionOfFirstChar = firstMatch.position(0);
			const int positionOfLastChar = endMatch.position(0) + endKw.size() - 1;
			const int totalBytesParsed = positionOfLastChar;
			const int totalValidBytes = 1 + (positionOfLastChar - positionOfFirstChar);
			return { totalBytesParsed, span.subspan(firstMatch.position(0), totalValidBytes) };
		}
		return { 0, span.subspan(0, 0) };
	}

	ParseResult keywordParse(const std::string& startKw, const std::string& endKw, std::span<byte_t> span)
	{
		auto match = [&span](const int idx, const std::string& kw)
		{
			const int nbytes = span.size();
			const int kwsize = kw.size();
			for (int i = idx; i < (nbytes - kwsize) + 1; ++i)
			{
				int nmatchs{ 0 };
				for (int j = i; j < i + kwsize; ++j)
				{
					nmatchs = nmatchs + static_cast<int>((span[j] == kw[j - i]));
				}
				if (nmatchs == kwsize)
				{
					return i;
				}
			}
			return static_cast<int>(-1);
		};
		const int startIdx = match(0, startKw);
		const int endIdx = match(std::max(startIdx, 0), endKw);	
		if (startIdx != -1 && endIdx != -1)
		{
			const int startWithoutKw = startIdx + startKw.size();
			const int endWithoutKw = endIdx - 1;
			const int totalValidBytes = endWithoutKw - startWithoutKw;
			return { static_cast<int>(endWithoutKw + endKw.size()), span.subspan(startWithoutKw, totalValidBytes) };
		}
		return { static_cast<int>(span.size()),  {} };
	}

	ParseResult delimiterParse(byte_t left, byte_t right, std::span<byte_t> span, int delimiterSize = 1)
	{
		for (int i = 0; i < span.size(); ++i)
		{
			byte_t byte = span[i];
			if (byte == left)
			{
				int delimCount{ 1 };
				int idx{ i+1 };
				while (byte != right || delimCount != 0)
				{
					byte = span[idx];
					if (byte == left)
					{
						++delimCount;
					}
					else if (byte == right)
					{
						--delimCount;
					}
					++idx;
				}
				const int startWithoutDel = i + delimiterSize;
				const int endWithoutDel = idx - delimiterSize;
				const int totalValidBytes = endWithoutDel - startWithoutDel;
				return { idx, span.subspan(startWithoutDel, totalValidBytes) };
			}
		}
		return { static_cast<int>(span.size()), {} };
	}


	struct PDFObject
	{
		std::span<byte_t> object{};
		std::span<byte_t> stream{};
		std::span<byte_t> dictionary{};
		int totalBytesParsed{ 0 };

		explicit PDFObject(std::span<byte_t> span)
		{
			const ParseResult objRes = keywordParse("obj", "endobj", span);
			STORYT_ASSERT((objRes.bytesWithNoKeyWords.size() > 0), "Failed to find pdf object");
			const ParseResult streamRes = keywordParse("stream", "endstream", objRes.bytesWithNoKeyWords);
			const ParseResult dictionaryRes = delimiterParse('<', '>', objRes.bytesWithNoKeyWords, 2 /* Number of Delimiters for Dict << and >> */);
			STORYT_ASSERT((dictionaryRes.bytesWithNoKeyWords.size() > 0), "Failed to find pdf dictionary");
			object = objRes.bytesWithNoKeyWords;
			stream = streamRes.bytesWithNoKeyWords;
			dictionary = dictionaryRes.bytesWithNoKeyWords;
			totalBytesParsed = objRes.totalBytesParsed;
		}
	};

} // storyt::_internal

#endif // STORYT_PDF_PARSERS_H