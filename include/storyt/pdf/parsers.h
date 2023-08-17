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
	int hexCharsToHex(std::span<byte_t> bytes)
	{
		return std::stoul(std::string(bytes.begin(), bytes.end()), nullptr, 16);
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

	struct HexString;

	template<typename ReturnValueType = std::span<byte_t>, typename Container>
	std::vector<ReturnValueType> rangesTo(const Container& ranges, std::span<byte_t> bytes)
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
			else if constexpr (std::is_same_v<ReturnValueType, HexString>)
			{
				results.push_back(HexString::create(range.toSpan(bytes)));
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
		std::span<byte_t> bytes, 
		int startIdx, 
		const std::unordered_set<byte_t>& possibleNotBytes
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

	Range strip(
		std::span<byte_t> bytes, 
		const std::unordered_set<byte_t>& charsToStrip
	)
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
		const int newStartIdx = startIdx; //readUntilNot(bytes, startIdx, { ' ', delim });	
		std::vector<Range> values{};
		int nextValidValueByte = newStartIdx;
		while (nextValidValueByte < bytes.size())
		{
			const byte_t byte = bytes[nextValidValueByte];
			const int valueStartIdx = nextValidValueByte;
			//const int delimIdx = std::min(
			//	find(bytes, delim, valueStartIdx + 1), static_cast<int>(bytes.size()) - 1
			//);
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

	std::vector<std::span<byte_t>> split(std::span<byte_t> bytes, byte_t splitBy)
	{
		std::vector<std::span<byte_t>> ranges{};
		std::span<byte_t> bbs = strip(bytes, { ' ' }).toSpan(bytes);
		int idx{ 0 };
		while (idx < bbs.size())
		{
			const int delimIdx = find(bbs, splitBy, idx);
			if (delimIdx == NOTFOUND)
			{
				ranges.push_back(Range{ 
					idx, static_cast<int>(bbs.size()) - 1 
					}.toSpan(bbs));
				return ranges;
			}
			else
			{
				ranges.push_back(Range{
					idx, delimIdx - 1
					}.toSpan(bbs));
				idx = delimIdx + 1;
			}
		}
		return ranges;
	}

	struct HexString
	{
		static HexString create(std::span<byte_t> bytes)
		{
			const int delimSize = 1;
			const int startIdx = 0;
			const Range hrange = find(bytes, '<', '>', startIdx, delimSize);
			if (hrange.isValid())
			{
				std::span<byte_t> newBytes = hrange.toSpan(bytes);
				STORYT_ASSERT((newBytes.size() >= 4 && newBytes.size() % 4 == 0), "Invalid size for HexString");
				std::vector<uint16_t> values{};
				values.reserve(newBytes.size() / 4);
				for (size_t i = 0; i < newBytes.size(); i += 4)
				{
					values.push_back(hexCharsToHex(newBytes.subspan(i, 4)));
				}
				return HexString{ values };
			}
			return HexString{};
		}
		[[nodiscard]] size_t size() const { return values.size(); }
		[[nodiscard]] uint16_t at(size_t idx) { return values.at(idx); }
		auto begin() { return values.begin(); }
		auto end() { return values.end(); }
		std::vector<uint16_t> values{};
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
			//STORYT_ASSERT((!bytes.empty() && bytes[bytes.size() - 1] == 'R'), "Invalid bytes were passed to IndirectReference");
			STORYT_ASSERT(!bytes.empty(), "Invalid bytes were passed to IndirectReference");
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
		static PDFDictionary create(std::span<byte_t> bytes)
		{
			// delimSize = 2 because << and >> for dictionary
			const int delimSize = 2;
			const int startIdx = 0;
			const Range dictRange = find(bytes, '<', '>', startIdx, delimSize);
			if (dictRange.isValid())
			{
				return PDFDictionary(dictRange.toSpan(bytes), dictRange.endIdx);
			}
			return PDFDictionary(std::span<byte_t>{}, bytes.size());
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
		[[nodiscard]] bool isValid() const { return !m_dictionary.empty(); }
		[[nodiscard]] bool hasName(const std::string& name)
		{
			if (isValid())
			{
				return search_(name) != nullptr;
			}
			return false;
		}
		[[nodiscard]] std::span<byte_t> getValue(const std::string& name)
		{
			std::vector<byte_t>* value = search_(name);
			if (value != nullptr)
			{
				return std::span<byte_t>{*value};
			}
			return std::span<byte_t>{};
		}
		template<typename ValueAs>
		[[nodiscard]] ValueAs getValueAs(const std::string& name)
		{
			if constexpr (std::is_same_v<ValueAs, std::string>)
			{
				std::span<byte_t> bytes = getValue(name);
				return std::string(bytes.begin(), bytes.end());
			}
			else if constexpr (std::is_same_v<ValueAs, int>)
			{
				return bytesToInt(getValue(name));
			}
			else if constexpr (std::is_same_v<ValueAs, IndirectReference>)
			{
				return IndirectReference::create(getValue(name));
			}
			else if constexpr (std::is_same_v<ValueAs, PDFDictionary>)
			{
				return PDFDictionary::create(getValue(name));
			}
			else
			{
				STORYT_ASSERT(false, "Passed invalid type");
			}
		}
	private:
		explicit PDFDictionary(std::span<byte_t> dictBytes, int totalBytesParsed)
			: m_totalBytesParsed(totalBytesParsed)
		{
			parseNameValues_(dictBytes);
		}
		void createSubDictionaries_()
		{
			if (isValid() && !m_createdSubDictionaries)
			{
				for (auto& [name, bytes] : m_dictionary)
				{
					PDFDictionary dict = PDFDictionary::create(std::span<byte_t>{bytes});
					if (dict.isValid())
					{
						dict.createSubDictionaries_();
						m_subDictionaries.push_back(std::move(dict));
					}
				}
			}
			m_createdSubDictionaries = true;
		}
		std::vector<byte_t>* search_(const std::string& name)
		{
			if (isValid())
			{
				if (m_dictionary.contains(name))
				{
					return &m_dictionary.at(name);
				}
				// if not in the main dictionary create the subdictionaries and search them
				createSubDictionaries_();
				for (PDFDictionary& dict : m_subDictionaries)
				{
					std::vector<byte_t>* res = dict.search_(name);
					if (res != nullptr)
					{
						return res;
					}
				}
			}
			return nullptr;
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
		std::vector<PDFDictionary> m_subDictionaries{};
		bool m_createdSubDictionaries{ false };
		size_t m_totalBytesParsed{ 0 };
	};

	class PDFObject
	{
	public:
		static PDFObject create(std::span<byte_t> bytes, int objId, int objGen = 0)
		{
			return PDFObject(
				objId, 
				objGen, 
				std::vector<byte_t>(bytes.begin(), bytes.end()), 
				PDFDictionary::create(bytes),
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
				PDFDictionary dict = PDFDictionary::create(objBytes);
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
			return PDFObject(
				NOTFOUND, 
				NOTFOUND, 
				std::vector<byte_t>{}, 
				PDFDictionary(std::span<byte_t>{}, bytes.size()), 
				bytes.size()
			);
		}

		[[nodiscard]] size_t getTotalBytesParsed() const { return m_totalBytesParsed; }
		[[nodiscard]] int getID() const { return m_id; }
		[[nodiscard]] bool isValid() const { return !m_objectBytes.empty() && m_dict.isValid(); }
		[[nodiscard]] bool hasStream() { return !decompressStream().empty(); }
		[[nodiscard]] bool dictHasName(const std::string& name)
		{
			return m_dict.hasName(name);
		}
		[[nodiscard]] std::span<byte_t> getDictValue(const std::string& name)
		{
			return m_dict.getValue(name);
		}
		template<typename ValueAs>
		[[nodiscard]] ValueAs getDictValueAs(const std::string& name)
		{
			return m_dict.getValueAs<ValueAs>(name);
		}
		[[nodiscard]] std::string decompressedStreamToString()
		{
			decompressStream();
			return std::string(m_decompressedStream.begin(), m_decompressedStream.end());
		}
		[[nodiscard]] std::span<byte_t> decompressStream()
		{
			if (m_decompressedStream.empty())
			{
				m_decompressedStream = decompress(PDFStream::create(m_objectBytes));
			}
			return std::span<byte_t>{m_decompressedStream};
		}
		std::vector<PDFObject> getSubObjects()
		{
			std::vector<PDFObject> subObjects{};
			if (getDictValueAs<std::string>("/Type") == "ObjStm")
			{
				std::span<byte_t> decompressedBytes = decompressStream();
				STORYT_ASSERT((!decompressedBytes.empty()), "Failed to decompress stream");
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
			PDFDictionary&& dict, 
			size_t totalBytesParsed
		)
			: 
			m_id(id),
			m_gen(gen),
			m_objectBytes(objectBytes),
			m_dict(std::move(dict)),
			m_totalBytesParsed(totalBytesParsed)
		{}
	private:
		int m_id{ NOTFOUND };
		int m_gen{ NOTFOUND };
		std::vector<byte_t> m_objectBytes{};
		std::vector<byte_t> m_decompressedStream{};
		PDFDictionary m_dict;
		size_t m_totalBytesParsed{ 0 };
	};

	std::vector<Range> readLines(std::span<byte_t> bytes)
	{
		int i = 0;
		std::vector<Range> lines{};
		const std::unordered_set<byte_t> endbytes = { '\r', '\n' };
		while (i < bytes.size())
		{
			int endIdx = readUntil(bytes, i, endbytes);
			const Range r{ i, endIdx };
			if (r.isValid())
			{
				lines.push_back(r);
			}
			i += std::max(static_cast<int>(r.size()), 1);
		}
		return lines;
	}

	class Cmap
	{
	public:
		static Cmap create(std::span<byte_t> bytes)
		{
			std::unordered_map<uint16_t, uint16_t> mapping{};
			std::span<byte_t> bfBlock = readKwBlock(
				bytes, "beginbfrange\n", "endbfrange"
			);
			std::vector<Range> bflines = readLines(bfBlock);
			for (const Range& line : bflines)
			{
				std::span<byte_t> lineBytes = line.toSpan(bfBlock);
				std::vector<std::span<byte_t>> lineObjects = readLineObjects(lineBytes);
				std::vector<HexString> srcCodes = readSrcCodes(lineObjects);
				std::vector<HexString> dstCodes = readDstCodes(lineObjects);
				createMapping(mapping, srcCodes, dstCodes);
			}

			std::span<byte_t> charBlock = readKwBlock(
				bytes, "beginbfchar\n", "endbfchar"
			);
			std::vector<Range> charLines = readLines(charBlock);
			for (const Range& line : charLines)
			{
				std::span<byte_t> lineBytes = line.toSpan(charBlock);
				std::vector<std::span<byte_t>> lineObjects = readLineObjects(lineBytes);
				std::vector<HexString> srcCodes = readSrcCodes(lineObjects);
				std::vector<HexString> dstCodes = readDstCodes(lineObjects);
				createMapping(mapping, srcCodes, dstCodes);
			}
			return Cmap{ std::move(mapping) };
		}
		static std::span<byte_t> readKwBlock(
			std::span<byte_t> bytes, 
			const std::string& startKw, 
			const std::string& endKw
		)
		{
			const bool removeKws{ true };
			const Range crange = findKeywordBlock(
				bytes, startKw, endKw, removeKws
			);
			if (crange.isValid())
			{
				return crange.toSpan(bytes);
			}
			return std::span<byte_t>{};
		}

		static void createMapping(
			std::unordered_map<uint16_t, uint16_t>& map,
			std::span<HexString> src,
			std::span<HexString> dst
			)
		{
			// Dst should be a single uint16_t
			/*
			* Example: Src -> <0000> <005E>  Dst -> <0020>
			*	<0000> maps to <0020>
			*	<005E> maps to <007E> ie <0020> + <005E>
			*/
			HexString hex = dst[0];
			STORYT_ASSERT((hex.size() == 1), "Invalid HexString");
			uint16_t dstCode = hex.at(0);
			for (size_t i = 0; i < src.size(); ++i)
			{
				STORYT_ASSERT((src[i].size() == 1), "Invalid HexString");
				uint16_t srcCode = src[i].at(0) + dstCode * (i != 0);
				STORYT_ASSERT(!map.contains(srcCode), "Invalid HexString");
				map[srcCode] = dstCode;
			}
		}

		static void createCharacterSequenceMapping(
			std::unordered_map<uint16_t, HexString>& map,
			std::span<HexString> src,
			std::span<HexString> dst
		)
		{
			/*
			* Example: Src -> <005F> <0061>  Dst -> [<00660066> <00660069> <00660066006C>]
			*	<005F> maps to <00660066> which is the character sequence ff
			*	<0060> maps to <00660069> which is the character sequence fi
			*	<0061> maps to <00660066006C> which is the character sequence ffl
			*/

			//HexString hex = dst[0];
			//STORYT_ASSERT((hex.size() == 1), "Invalid HexString");
			//uint16_t dstCode = hex.at(0);
			//for (size_t i = 0; i < src.size(); ++i)
			//{
			//	STORYT_ASSERT((src[i].size() == 1), "Invalid HexString");
			//	uint16_t srcCode = src[i].at(0) + dstCode * (i != 0);
			//	map[srcCode] = dstCode;
			//}
		}

		static std::vector<std::span<byte_t>> readLineObjects(std::span<byte_t> lineBytes)
		{
			return split(lineBytes, ' ');
		}

		static std::vector<HexString> readSrcCodes(
			std::span<std::span<byte_t>> lineObjects
		)
		{
			std::vector<HexString> strings{};
			strings.reserve(lineObjects.size());
			// Don't include the last line object as it is the dst code.
			for (std::span<byte_t> srcBytes : lineObjects.subspan(0, lineObjects.size() - 1))
			{
				strings.push_back(HexString::create(srcBytes));
			}
			return strings;
		}

		static std::vector<HexString> readDstCodes(
			std::vector<std::span<byte_t>> lineObjects
		)
		{
			std::span<byte_t> dstBytes = lineObjects.back();
			if (dstBytes[0] == '[')
			{
				std::span<byte_t> strippedBytes = strip(dstBytes, { '[', ']' }).toSpan(dstBytes);
				std::vector<Range> ranges = readValuesDelimitedByUntil(
					strippedBytes, 0, ' ', {}
				);
				return rangesTo<HexString>(
					ranges, strippedBytes
				);
			}
			else if (dstBytes[0] == '<')
			{
				return std::vector<HexString>{HexString::create(dstBytes)};
			}
			else
			{
				STORYT_ASSERT(false, "Dst bytes were invalid");
			}
			return std::vector<HexString>{};
		}

		static void readRange(std::span<byte_t> bytes, const std::vector<Range>& lines)
		{
			for (const Range& line : lines)
			{
				std::span<byte_t> lineBytes = line.toSpan(bytes);
				std::vector<Range> lineObjs = readValuesDelimitedByUntil(
					lineBytes,
					0,
					' ',
					{}
				);
				// the last object in lineObjs is the dest obj so don't read it
				std::vector<HexString> srcObjects = rangesTo<HexString>(
					std::span<Range>{lineObjs}.subspan(0, lineObjs.size() - 1), 
					lineBytes
				);
				std::span<byte_t> dstBytes = strip(lineObjs.back().toSpan(lineBytes), {' '}).toSpan(lineBytes);
				if (dstBytes[0] == '[')
				{
					std::span<byte_t> strippedBytes = strip(dstBytes, {'[', ']'}).toSpan(dstBytes);
					std::vector<HexString> values = rangesTo<HexString>(readValuesDelimitedByUntil(
							strippedBytes,
							0,
							' ',
							{}
						),
						strippedBytes
					);
				}
				else if (dstBytes[0] == '<')
				{
					uint16_t dst = HexString::create(dstBytes).at(0);
					std::unordered_map<uint16_t, uint16_t> mapping;
					for (HexString& srcHex : srcObjects)
					{
						for (size_t i = 0; i < srcHex.size(); ++i)
						{
							mapping[srcHex.at(i)] = srcHex.at(i) + (dst * (i != 0));
						}
					}
				}
				else
				{
					STORYT_ASSERT(false, "Dst bytes were invalid");
				}
			}
		}
		private:
			explicit Cmap(std::unordered_map<uint16_t, uint16_t>&& cmap)
				: m_cmap(std::move(cmap)) {}
		private:
			std::unordered_map<uint16_t, uint16_t> m_cmap{};
	};

} // storyt::_internal

#endif // STORYT_PDF_PARSERS_H