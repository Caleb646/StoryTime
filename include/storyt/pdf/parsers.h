#ifndef STORYT_PDF_PARSERS_H
#define STORYT_PDF_PARSERS_H
#include <cstddef>
#include <cstring>
#include <span>
#include <functional>
#include <optional>
#include <regex>
#include <unordered_map>
#include <algorithm>
#include <numeric>
#include <unordered_set>

#include <storyt/common.h>
#include <storyt/pdf/tokens.h>


namespace storyt::_internal
{
	static constexpr int NOTFOUND = std::numeric_limits<int>::max();
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
	std::string bytesToString(std::span<byte_t> bytes)
	{
		return std::string(bytes.begin(), bytes.end());
	}
	int bytesToInt(std::span<byte_t> bytes)
	{
		std::string i = bytesToString(bytes);
		// search for anything that is not a number
		if (std::regex_search(i, std::regex("[^0-9]")))
		{
			STORYT_ERROR("Failed to convert string [{}] to integer", i);
			return NOTFOUND;
		}
		return std::stoi(i);
	}

	struct Range
	{
		int startIdx{ NOTFOUND };
		int endIdx{ NOTFOUND };
		[[nodiscard]] bool isValid() const 
		{ 
			return 
				startIdx != NOTFOUND && 
				endIdx != NOTFOUND && 
				startIdx >= 0 && endIdx >= 0 &&
				endIdx >= startIdx;
		}

		[[nodiscard]] size_t size() const
		{
			if (isValid())
			{
				/// It is possible for start and end to equal one another and the Range is
				/// still valid such as in the case when the Range represents only 1 byte.
				/// To account for this 1 is added to end.
				return (1 + endIdx) - startIdx;
			}
			STORYT_ASSERT(false, "Range was not valid");
			return 0;
		}
		[[nodiscard]] std::span<byte_t> toSpan(std::span<byte_t> bytes) const
		{
			if (isValid())
			{
				return bytes.subspan(startIdx, size());
			}
			STORYT_ASSERT(false, "Range was not valid");
			return {};
		}
		[[nodiscard]] std::string toString(std::span<byte_t> bytes) const
		{
			return bytesToString(toSpan(bytes));
		}
		[[nodiscard]] int toInt(std::span<byte_t> bytes) const
		{
			return bytesToInt(toSpan(bytes));
		}
	};

	std::span<byte_t> rangeToSpan(const Range& range, std::span<byte_t> bytes)
	{
		if (range.isValid())
		{
			return bytes.subspan(range.startIdx, range.size());
		}
		STORYT_ASSERT(false, "Range was not valid");
		return {};
	}

	template<typename ReturnValueType = std::span<byte_t>>
	std::vector<ReturnValueType> rangesTo(const std::vector<Range>& ranges, std::span<byte_t> bytes)
	{
		std::vector<ReturnValueType> results{};
		results.reserve(ranges.size());
		for (const auto& range : ranges)
		{
			if constexpr (std::is_same_v<ReturnValueType, int>)
			{
				results.push_back(range.toInt(bytes));
			}
			else if constexpr (std::is_same_v<ReturnValueType, std::string>)
			{
				results.push_back(range.toString(bytes));
			}
			else
			{
				results.push_back(range.toSpan(bytes));
			}
		}
		STORYT_ASSERT((results.size() == ranges.size()), "Failed to convert ranges to values");
		return results;
	}

	Range findKeywordBlock(std::span<byte_t> span, const std::string& startKw, const std::string& endKw, bool removeKeyWords = true)
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
			return NOTFOUND;
		};
		const int startIdx = match(0, startKw);
		const int endIdx = match(std::max(startIdx, 0), endKw);
		// If the start keyword is obj then the startIdx is pointing at 'o'
		// on a successful match
		// If the end keyword is endobj then the endIdx is pointing at 'e'
		// on a successful match
		if (startIdx != NOTFOUND && endIdx != NOTFOUND)
		{
			if (removeKeyWords)
			{
				return Range{ static_cast<int>(startIdx + startKw.size()), endIdx - 1 };
			}
			else
			{
				return Range{ startIdx, static_cast<int>(endIdx + endKw.size() - 1) };
			}
			
		}
		return { NOTFOUND,  NOTFOUND };
	}

	int find(
		std::span<byte_t> bytes, byte_t byte, int startIdx
	)
	{
		for (int i = startIdx; i < bytes.size(); ++i)
		{
			if (bytes[i] == byte)
			{
				return i;
			}
		}
		return NOTFOUND;
	}

	Range find(
		std::span<byte_t> bytes, 
		byte_t startDelim, 
		byte_t endDelim, 
		int startIdx, 
		int delimSize = 0 // If 0 will not remove delimiters from returned range
	)
	{
		int ndelims{ 0 };
		const int firstStartDelimIdx = find(bytes, startDelim, startIdx);
		if (firstStartDelimIdx == NOTFOUND)
		{
			return { NOTFOUND, NOTFOUND };
		}
		for (int i = firstStartDelimIdx; i < bytes.size(); ++i)
		{
			const byte_t byte = bytes[i];
			ndelims = ndelims + static_cast<int>((byte == startDelim));
			ndelims = ndelims - static_cast<int>((byte == endDelim));
			if (ndelims == 0)
			{
				return { firstStartDelimIdx + delimSize, i - delimSize };
			}
		}
		return { NOTFOUND, NOTFOUND };
	}

	int readUntil(
		std::span<byte_t> bytes, 
		int startIdx, 
		std::unordered_set<byte_t> possibleEndBytes, 
		int defaultReturn = NOTFOUND
	)
	{
		for (int i = startIdx; i < bytes.size(); ++i)
		{
			if (possibleEndBytes.contains(bytes[i]))
			{
				return i;
			}
		}
		return defaultReturn;
	}

	int readBackwardsUntil(
		std::span<byte_t> bytes, int startIdx, std::unordered_set<byte_t> possibleEndBytes
	)
	{
		for (int i = startIdx; i >= 0; --i)
		{
			if (possibleEndBytes.contains(bytes[i]))
			{
				return i;
			}
		}
		return NOTFOUND;
	}

	int readUntilNot(
		std::span<byte_t> bytes, int startIdx, std::unordered_set<byte_t> possibleNotBytes
	)
	{
		for (int i = startIdx; i < bytes.size(); ++i)
		{
			if (!possibleNotBytes.contains(bytes[i]))
			{
				return i;
			}
		}
		return NOTFOUND;
	}

	Range strip(std::span<byte_t> bytes, std::unordered_set<byte_t> charsToStrip)
	{
		int left{ 0 };
		while (charsToStrip.contains(bytes[left]))
		{
			++left;
		}
		int right{ static_cast<int>(bytes.size()) - 1 };
		while (charsToStrip.contains(bytes[right]))
		{
			--right;
		}
		return Range{ left, right };
	}

	std::vector<Range> readValuesDelimitedByUntil(
		std::span<byte_t> bytes, 
		int startIdx, 
		byte_t delim, 
		const std::unordered_set<byte_t>& possibleEndBytes
	)
	{
		// remove white space and any delims up until the first value byte
		const int newStartIdx = readUntilNot(bytes, startIdx, { ' ', delim });	
		if (newStartIdx == NOTFOUND)
		{
			return {};
		}
		std::vector<Range> values{};
		int nextValidValueByte = newStartIdx;
		while (nextValidValueByte < bytes.size())
		{
			const byte_t byte = bytes[nextValidValueByte];
			const int valueStartIdx = nextValidValueByte;
			const int delimIdx = find(bytes, delim, valueStartIdx + 1);
			if (possibleEndBytes.contains(byte) || delimIdx == NOTFOUND || byte == delim)
			{
				return values;
			}
			values.push_back({ valueStartIdx, delimIdx - 1 });
			nextValidValueByte = delimIdx + 1;
		}
		return values;
	}

	class BaseObject
	{
	public:
		int totalBytesRead{0};
	};

	class IndirectReference
	{
	public:
		static std::vector<IndirectReference> create(std::span<byte_t> bytes, int count)
		{
			STORYT_ASSERT((bytes.size() > 0 && bytes[0] == LEFTSQUBRACKET && bytes[bytes.size() - 1] == RIGHTSQUBRACKET), 
				"Invalid bytes");
			const Range cleaned = strip(bytes, { ' ', LEFTSQUBRACKET, RIGHTSQUBRACKET });
			std::span<byte_t> newBytes(bytes.subspan(cleaned.startIdx, cleaned.size()));
			const byte_t delim = 'R';
			const std::unordered_set<byte_t> endBytes = { RIGHTSQUBRACKET };
			std::vector<Range> ranges = readValuesDelimitedByUntil(newBytes, 0, delim, endBytes);
			STORYT_ASSERT((count == ranges.size()), "Failed to find enough values");
			std::vector<IndirectReference> refs{};
			refs.reserve(count);
			for (const auto& range : ranges)
			{
				refs.push_back(create(range.toSpan(newBytes)));
			}
			return refs;
		}
		static IndirectReference create(std::span<byte_t> bytes)
		{
			/// Should be in the format: Number Number R -> 23 0 R or 1 0 R
			STORYT_ASSERT((!bytes.empty() && bytes[bytes.size() - 1] == 'R'), "Invalid bytes were passed to IndirectReference");
			STORYT_ASSERT((bytes[0] != ' '), "Invalid bytes were passed to IndirectReference");
			const byte_t delim = ' ';
			const std::unordered_set<byte_t> endBytes = {'R'};
			std::vector<Range> ranges = readValuesDelimitedByUntil(
				bytes, 0, delim, endBytes
			);
			/// Should be something like: 9 0 with R removed
			STORYT_ASSERT((ranges.size() == 2), "Invalid indirect reference");
			std::vector<int> ids = rangesTo<int>(ranges, bytes);
			return IndirectReference{ ids.at(0), ids.at(1) };
		}
		int id{ NOTFOUND };
		int gen{ NOTFOUND };
	};

	struct PDFStream
	{
		static std::span<byte_t> create(std::span<byte_t> bytes)
		{
			const Range streamRange = findKeywordBlock(bytes, "stream\r\n", "endstream");
			if (streamRange.isValid())
			{
				return streamRange.toSpan(bytes);
			}
			return std::span<byte_t>{};
		}
	};

	// Forward Declare for friend status
	class PDFObject;

	class PDFDictionary
	{
		friend PDFObject;
	public:
		static std::optional<PDFDictionary> create(std::span<byte_t> bytes)
		{
			const int delimSize = 2;
			const Range dictRange = find(bytes, '<', '>', 0, delimSize);
			if (dictRange.isValid())
			{
				return PDFDictionary(dictRange.toSpan(bytes), dictRange.endIdx);
			}
			return std::nullopt;
		}
		static Range findName(std::span<byte_t> bytes)
		{
			const int startIdx = find(bytes, '/', 0);
			const int endIdx = readUntil(
				bytes, 
				startIdx + 1, 
				std::unordered_set<byte_t>{ ' ', LESSTHAN, '/', LEFTPAREN, LEFTSQUBRACKET }
			);
			if (startIdx == NOTFOUND || endIdx == NOTFOUND)
			{
				return { NOTFOUND, NOTFOUND };
			}
			/// remove ' ', <, (, or [ from Name Range
			return { startIdx, endIdx - 1 };
		}

		static Range findValue(std::span<byte_t> bytes, const Range nameRange)
		{
			auto [nameStartIdx, nameEndIdx] = nameRange;
			if (nameRange.isValid() && bytes.size() > nameEndIdx + 1 && bytes[nameStartIdx] == '/')
			{
				const int delimiterIdx = nameEndIdx + 1;
				const byte_t delimiter = bytes[delimiterIdx];
				const int valueStartIdx = delimiterIdx + 1;
				/// -> /Name Value or -> /Name/Value
				if (delimiter == ' ' || delimiter == '/')
				{
					/// For delimiter = whitespace, could be -> /Name Value/Name2, /Name Value>>, or /Name Value where 'e' in Value
					/// is the last byte in the stream.
					/// 
					/// For delimiter = '/', could be -> /Name/Value/Name2, /Name/Value>>, or /Name/Value where 'e' in Value
					/// is the last byte in the stream.
					const int defaultReturnIdx = static_cast<int>(bytes.size());
					const int valueEndIdx = readUntil(
						bytes, valueStartIdx, std::unordered_set<byte_t>{ '/', GREATERTHAN }, defaultReturnIdx
					);
					// Remove '/', '>' or valueEndIdx is at bytes.size() so subtract 1
					return Range{ valueStartIdx, valueEndIdx - 1 };

				}
				/// -> /Name<<.....>>
				else if (delimiter == LESSTHAN)
				{
					// Use delimiterIdx instead of valueStartIdx so ndelims will be correct
					const Range valueRange = find(bytes, '<', '>', delimiterIdx);
					if (valueRange.isValid())
					{
						return valueRange;
					}
					STORYT_ASSERT(false, "Failed to parse subdictionary");
					return { NOTFOUND, NOTFOUND };
				}
				/// -> /Name(Value)
				else if(delimiter == LEFTPAREN)
				{
					// Use delimiterIdx instead of valueStartIdx so ndelims will be correct
					const Range valueRange = find(bytes, '(', ')', delimiterIdx);
					if (valueRange.isValid())
					{
						return valueRange;
					}
					STORYT_ASSERT(false, "Failed to parse string literal");
					return { NOTFOUND, NOTFOUND };
				}
				/// -> /Name[.....]
				else if (delimiter == LEFTSQUBRACKET)
				{
					// Use delimiterIdx instead of valueStartIdx so ndelims will be correct
					const Range valueRange = find(bytes, '[', ']', delimiterIdx);
					if (valueRange.isValid())
					{
						return valueRange;
					}
					STORYT_ASSERT(false, "Failed to parse array");
					return { NOTFOUND, NOTFOUND };
				}
				else
				{
					STORYT_ASSERT(false, "Invalid Delimiter [{}] for Dictionary Value", delimiter);
				}
			}
			STORYT_ASSERT(false, "Failed to find Dictionary value");
			return { NOTFOUND, NOTFOUND };
		}
		[[nodiscard]] size_t getTotalBytesParsed() const { return m_totalBytesParsed; }
	private:
		explicit PDFDictionary(std::span<byte_t> dictBytes, int totalBytesParsed)
			: m_totalBytesParsed(totalBytesParsed)
		{
			parseNameValues_(dictBytes);
		}
		void parseNameValues_(std::span<byte_t> bytes)
		{
			for (int i = 0; i < bytes.size(); ++i)
			{
				const std::span<byte_t> unparsedBytes = bytes.subspan(i);
				const Range nameRange = findName(unparsedBytes);
				const Range valueRange = findValue(unparsedBytes, nameRange);
				if (nameRange.isValid() && valueRange.isValid())
				{
					std::span<byte_t> nameBytes = nameRange.toSpan(unparsedBytes);
					std::string name(nameBytes.begin(), nameBytes.end());
					std::span<byte_t> value = valueRange.toSpan(unparsedBytes);
					STORYT_VERIFY((!m_dictionary.contains(name)),
						"Found duplicate name [{}] in dictionary", name);
					m_dictionary[name] = std::vector<byte_t>(value.begin(), value.end());
					i += valueRange.endIdx;
				}
			}
		}
	private:
		std::unordered_map<std::string, std::vector<byte_t>> m_dictionary;
		size_t m_totalBytesParsed{ 0 };
	};

	class PDFObject
	{
	public:
		static PDFObject create(std::span<byte_t> bytes, int objId, int objGen = 0)
		{
			std::optional<PDFDictionary> dict = PDFDictionary::create(bytes);
			return PDFObject(
				objId, 
				objGen, 
				std::vector<byte_t>(bytes.begin(), bytes.end()), 
				std::move(dict), 
				bytes.size()
			);
		}
		static PDFObject create(std::span<byte_t> bytes)
		{
			const std::string objStartKw("obj");
			const std::string objEndKw("endobj");
			const bool shouldRemoveKws{ false };
			const Range objRange = findKeywordBlock(bytes, objStartKw, objEndKw, shouldRemoveKws);
			if (objRange.isValid())
			{
				STORYT_ASSERT((bytes[objRange.startIdx] == 'o' && bytes[objRange.startIdx - 1] == ' '), 
					"Invalid bytes");
				const int genEndIdx = objRange.startIdx - 2;
				const int genStartIdx = readBackwardsUntil(bytes, genEndIdx, { ' ' });
				const Range genRange{ genStartIdx + 1, genEndIdx };

				const int idEndIdx = genRange.startIdx - 2;
				const int idStartIdx = readBackwardsUntil(bytes, idEndIdx, { ' ', '\n', '\r'});
				const Range idRange{ idStartIdx + 1, idEndIdx };

				int id{ NOTFOUND };
				int gen{ NOTFOUND };
				if (genRange.isValid() && idRange.isValid())
				{
					id = idRange.toInt(bytes);
					gen = genRange.toInt(bytes);
				}

				static_assert(CRETURN == '\r');
				static_assert(LFEED == '\n');
				std::span<byte_t> objBytes = objRange.toSpan(bytes);
				std::optional<PDFDictionary> dict = PDFDictionary::create(objBytes);
				/// TODO: The stream keyword doesn't have to be followed by a \r and a \n. It is possible 
				/// that it is only followed by a \n .
				const Range streamRange = findKeywordBlock(objBytes, "stream\r\n", "endstream");
				std::span<byte_t> stream{};
				if (streamRange.isValid())
				{
					stream = streamRange.toSpan(objBytes);
				}
				return PDFObject(
					id, 
					gen,
					std::vector<byte_t>(objBytes.begin(), objBytes.end()),
					std::move(dict), 
					objRange.endIdx + (objEndKw.size() * static_cast<int>(shouldRemoveKws))
				);
			}
			return PDFObject(NOTFOUND, NOTFOUND, std::vector<byte_t>{}, std::nullopt, bytes.size());
		}

		[[nodiscard]] size_t getTotalBytesParsed() const { return m_totalBytesParsed; }
		[[nodiscard]] int getID() const { return m_id; }
		[[nodiscard]] bool isValid() const { return !m_objectBytes.empty() && m_dict.has_value(); }
		[[nodiscard]] bool dictHasName(const std::string& name)  const
		{
			if (m_dict.has_value())
			{
				return m_dict->m_dictionary.contains(name);
			}
			return false;
		}
		[[nodiscard]] std::span<byte_t> getDictValue(const std::string& name)
		{
			if (dictHasName(name))
			{
				return m_dict->m_dictionary.at(name);
			}
			return {};
		}
		template<typename ValueAs>
		[[nodiscard]] ValueAs getDictValueAs(const std::string& name)
		{
			if constexpr(std::is_same_v<ValueAs, std::string>)
			{
				std::span<byte_t> bytes = getDictValue(name);
				return std::string(bytes.begin(), bytes.end());
			}
			else if constexpr(std::is_same_v<ValueAs, int>)
			{
				return bytesToInt(getDictValue(name));
			}
			else if constexpr (std::is_same_v<ValueAs, IndirectReference>)
			{
				return IndirectReference::create(getDictValue(name));
			}
			else
			{
				STORYT_ASSERT(false, "Passed invalid type");
			}
		}
		[[nodiscard]] std::string decompressedStreamToString()
		{
			decompressStream();
			return std::string(m_decompressedStream.begin(), m_decompressedStream.end());
		}
		void decompressStream()
		{
			if (m_decompressedStream.empty())
			{
				m_decompressedStream = decompress(PDFStream::create(m_objectBytes));
			}
		}
		std::vector<PDFObject> getSubObjects()
		{
			std::vector<PDFObject> subObjects{};
			if (getDictValueAs<std::string>("/Type") == "ObjStm")
			{
				decompressStream();
				STORYT_ASSERT((!m_decompressedStream.empty()), "Failed to decompress stream");
				std::span<byte_t> decompressedBytes(m_decompressedStream);
				const int first = getDictValueAs<int>("/First");
				const int length = getDictValueAs<int>("/Length");
				const int N = getDictValueAs<int>("/N");
				STORYT_ASSERT((decompressedBytes[0] != ' '), "Invalid decompressed stream");
				const byte_t delim = ' ';
				std::vector<Range> values = readValuesDelimitedByUntil(
					decompressedBytes, 0, delim, { LESSTHAN }
				);
				STORYT_ASSERT((N * 2 == values.size()), "Incorrect number of obj ids and offsets");
				std::vector<int> idsAndOffsets = rangesTo<int>(values, decompressedBytes);
				std::span<byte_t> subObjectBytes = decompressedBytes.subspan(values.at(values.size() - 1).endIdx + 2);				
				for (int i = 0; i < idsAndOffsets.size(); i += 2)
				{
					const int id = idsAndOffsets.at(i);
					const int offset = idsAndOffsets.at(i + 1);
					int nextId = 0;
					int nextOffset = subObjectBytes.size();
					if (i != idsAndOffsets.size() - 2)
					{
						nextId = idsAndOffsets.at(i + 2);
						nextOffset = idsAndOffsets.at(i + 3);
					}
					STORYT_ASSERT((offset < nextOffset), "Invalid offsets");
					subObjects.push_back(PDFObject::create(
						subObjectBytes.subspan(offset, nextOffset - offset),
						id
					));
				}
			}
			return subObjects;
		}
		
	private:
		explicit PDFObject(
			int id,
			int gen,
			const std::vector<byte_t>& objectBytes,
			std::optional<PDFDictionary> dict, 
			size_t totalBytesParsed
		)
			: 
			m_id(id),
			m_gen(gen),
			m_objectBytes(objectBytes),
			m_dict(dict),
			m_totalBytesParsed(totalBytesParsed)
		{}
	private:
		int m_id{ NOTFOUND };
		int m_gen{ NOTFOUND };
		std::vector<byte_t> m_objectBytes{};
		std::vector<byte_t> m_decompressedStream{};
		std::optional<PDFDictionary> m_dict;
		size_t m_totalBytesParsed{ 0 };
	};

} // storyt::_internal

#endif // STORYT_PDF_PARSERS_H