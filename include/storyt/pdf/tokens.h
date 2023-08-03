#ifndef STORYT_PDF_TOKENS_H
#define STORYT_PDF_TOKENS_H
#include <cstddef>
#include <cstring>
#include <span>

#include <storyt/common.h>

namespace storyt::_internal
{

	/*
	* 
	* 
	* 
	* 
	* Regular Characters:
	*  A sequence of consecutive
	*  regular characters comprises a single token.
	* 
	* 
	* 
	*/
	enum WSpaceChars : byte_t /// White Space Characters
	{
		PNULL = 0x00, /// NULL (NUL)
		HTAB = 0x09, /// HORIZONTAL TAB (HT)
		LFEED = 0x0A, /// LINE FEED (LF) - treated as end-of-line (EOL) marker
		FFEED = 0x0C, /// FORM FEED (FF)
		CRETURN = 0x0D, /// CARRIAGE RETURN (CR) - treated as end-of-line (EOL) marker
		SPACE = 0x20 /// SPACE (SP)
	};

	enum Delimiter : byte_t
	{
		LEFTPAREN = 0x28, /// LEFT PARENTHESIS
		RIGHTPAREN = 0x29, /// RIGHT PARENTHESIS
		LESSTHAN = 0x3C,
		GREATERTHAN = 0x3E,
		LEFTSQUBRACKET = 0x5B,
		RIGHTSQUBRACKET = 0x5D,
		LEFTCURLYBRACKET = 0x7B,
		RIGHTCURLYBRACKET = 0x7D,
		SOLIDUS = 0x2F, /// '/'
		PERCENT = 0x25
	};
	/*
	* 
	* 
	* 
	* 
	* END Regular Characters:
	* 
	* 
	* 
	*/

	enum WildChar : byte_t
	{
		MATCHANY = 0xFF,
		DONE = 0xFE
	};

	enum class Result
	{
		FAILED,
		NOTFOUND,
		FOUND
	};

	struct Transition
	{
		byte_t current;
		byte_t incoming;
		byte_t next;
		byte_t on(byte_t incoming_)
		{
			if (incoming == incoming_)
			{
				return next;
			}
			return current;
		}
	};

	class ObjToken
	{
	public:
		static constexpr byte_t firstCharacter{'o'};
	public:
		explicit ObjToken(std::span<byte_t> span)
		{
			m_storedChars.reserve(2048);
			for (byte_t byte : span)
			{
				if (match(byte))
				{
					return;
				}
			}
		}
		bool match(byte_t byte)
		{
			m_storedChars.push_back(byte);
			if (getCurrentState() == MATCHANY)
			{
				/// If true objend has been found but its possible 
				/// that this is just a random e
				if (byte == 'e')
				{
					++m_currentStateIdx;
				}
			}
			else if (getCurrentState() == byte)
			{
				return ++m_currentStateIdx == sizeof(m_states);
			}
			else
			{
				/// If past the MATCHANY token then its possible that
				/// a random e was hit that triggered the state transition to e .
				/// So reset the state back to the MATCHANY Token and try again.
				if (m_currentStateIdx > 4)
				{
					m_currentStateIdx = 4;
				}
				else
				{
					reset();
				}
			}
			return false;
		}
		void reset()
		{
			m_storedChars.clear();
			m_currentStateIdx = 0;
		}
		[[nodiscard]] byte_t getCurrentState() const
		{
			return m_states[m_currentStateIdx];
		}
		[[nodiscard]] size_t nParsedCharacters() const
		{
			return m_storedChars.size();
		}
	private:
		uint8_t m_currentStateIdx{ 0 };
		std::vector<byte_t> m_storedChars{};
		static constexpr byte_t m_states[] = {'o', 'b', 'j', CRETURN, MATCHANY, 'n', 'd', 'o', 'b', 'j', CRETURN};
	};

	struct DictToken
	{
		explicit DictToken(std::span<byte_t> span)
		{
			storedChars.reserve(256);
			for (byte_t byte : span)
			{
				if (match(byte))
				{
					return;
				}
			}
		}
		static constexpr byte_t firstCharacter{ LESSTHAN };
		bool match(byte_t byte)
		{
			storedChars.push_back(byte);
			if (byte == LESSTHAN)
			{
				++nLessThan;
			}
			else if (byte == GREATERTHAN)
			{
				++nGreaterThan;
			}
			return nLessThan == nGreaterThan && (nLessThan > 0 && nGreaterThan > 0);
		}	
		uint32_t nLessThan{ 0 };
		uint32_t nGreaterThan{ 0 };
		std::vector<byte_t> storedChars{};
	};

} // storyt::_internal

#endif // STORYT_PDF_TOKENS_H