#ifndef STORYT_PDF_TOKENS_H
#define STORYT_PDF_TOKENS_H
#include <cstddef>
#include <cstring>
#include <span>
#include <functional>
#include <optional>

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

	enum State : state_t
	{
		MATCHANY = 0xFFFF,
		FOUND = 0xFFFE,
		FAILED = 0xFFFD,
		CONTINUE = 0xFFFC,
		START = 0xFFFA
	};

	template<typename T>
	struct ParseResult
	{
		State state;
		T transition;

		[[nodiscard]] bool didFail() const
		{
			return state == FAILED;
		}
	};

	struct Transition
	{
		using NextType = Transition*;
		using ResultType = ParseResult<NextType>;
		using onFuncBaseType = std::function<ResultType(state_t)>;
		using onFuncType = std::optional<std::function<ResultType(state_t)>>;
		state_t current;
		state_t incoming;
		NextType next;
		onFuncType onFunc;
		Transition(state_t curr, state_t inc, NextType n)
			: current(curr), incoming(inc), next(n), onFunc(std::nullopt) {}
		Transition(state_t curr, state_t inc, onFuncType of)
			: current(curr), incoming(inc), next(nullptr), onFunc(std::move(of)) {}
		Transition(state_t curr, state_t inc)
			: current(curr), incoming(inc), next(nullptr), onFunc(std::nullopt) {}
		ResultType on(state_t transitionByte)
		{
			STORYT_ASSERT((onFunc.has_value() || nextIsValid()),
				"Transition with current [{}] incoming [{}] does NOT have an onFunc or next value", current, incoming);
			if (onFunc.has_value())
			{
				return onFunc.value()(transitionByte);
			}
			else if (incoming == transitionByte && nextIsValid())
			{
				return { CONTINUE, next };
			}
			return { FAILED, nullptr };
		}
		[[nodiscard]] bool nextIsValid() const
		{
			return next != nullptr;
		}
		void setOnFunc(onFuncBaseType&& func)
		{
			onFunc = std::move(func);
		}
		static Transition createDoneTransition(state_t current, state_t incoming)
		{
			return Transition(current, incoming, [incoming](state_t inc) {
				if (incoming == inc)
				{
					return ResultType{ FOUND, nullptr };
				}
				else
				{
					return ResultType{ FAILED, nullptr };
				}
			});
		}
		static Transition createTransitionToMatchAny(state_t current, Transition* matchAny)
		{
			STORYT_ASSERT((matchAny != nullptr), "Cannot create Transtion to MatchAny with a nullptr");
			return Transition(current, MATCHANY, [matchAny](state_t) { return ResultType{ CONTINUE, matchAny }; });
		}

		static Transition createFailBackToTransition(
			state_t current, state_t incoming, Transition* successfulTo, Transition* failBackTo, State successfulMatch = CONTINUE)
		{
			STORYT_ASSERT((failBackTo != nullptr), "Cannot create Transtion to MatchAny with a nullptr");
			return Transition(current, incoming, 
				[successfulMatch, incoming, successfulTo, failBackTo](state_t inc)
				{
					if (incoming == inc)
					{
						return ResultType{ successfulMatch, successfulTo };
					}
					return ResultType{ CONTINUE, failBackTo };
				});
		}
	};

	template<typename PDFObjectType>
	int parse(std::span<const byte_t> span, Transition* starting)
	{
		STORYT_ASSERT((span.size() > 0 && span[0] == PDFObjectType::firstCharacter),
			"match was passed an invalid span");
		int bytesParsed{ 0 };
		for (byte_t byte : span)
		{
			++bytesParsed;
			auto result = starting->on(byte);
			if (result.didFail())
			{
				STORYT_ASSERT(false, "Parsing failed");
				return -1;
			}
			else if (result.state == FOUND)
			{
				return bytesParsed;
			}
			starting = result.transition;
		}
		STORYT_ASSERT(false, "Parsing failed");
		return -1;
	}

	struct ObjToken
	{
		static constexpr byte_t firstCharacter{'o'};
		std::span<const byte_t> storedBytes;
		int bytesParsed{ 0 };

		explicit ObjToken(std::span<const byte_t> span)
		{
			Transition matchAny = { MATCHANY, 'e' };
			Transition JJ = Transition::createFailBackToTransition('j', CRETURN, nullptr, &matchAny, FOUND);
			Transition BB = Transition::createFailBackToTransition('b', 'j', &JJ, &matchAny);
			Transition OO = Transition::createFailBackToTransition('o', 'b', &BB, &matchAny);
			Transition DD = Transition::createFailBackToTransition('d', 'o', &OO, &matchAny);
			Transition NN = Transition::createFailBackToTransition('n', 'd', &DD, &matchAny);
			Transition EE = Transition::createFailBackToTransition('e', 'n', &NN, &matchAny);
			matchAny.setOnFunc([&matchAny, &EE](state_t incoming) {
					if (incoming == 'e')
					{
						return Transition::ResultType{ CONTINUE, &EE};
					}
					else
					{
						return Transition::ResultType{ CONTINUE, &matchAny};
					}
				});
			Transition carriage = Transition::createTransitionToMatchAny(CRETURN, &matchAny);
			Transition J{ 'j', CRETURN, &carriage };
			Transition B{ 'b', 'j', &J };
			Transition O{ 'o', 'b', &B };
			Transition start{ START, 'o', &O };
			bytesParsed = parse<ObjToken>(span, &start);
			if (bytesParsed != -1)
			{
				storedBytes = span.first(bytesParsed);
			}
		}
	};

	struct DictToken
	{
		static constexpr byte_t firstCharacter{LESSTHAN};
		std::span<const byte_t> storedBytes;
		int nLessThan{ 0 };
		int nGreaterThan{ 0 };
		int bytesParsed{ 0 };

		explicit DictToken(std::span<const byte_t> span)
		{
			Transition secondGreaterThan = Transition::createDoneTransition(GREATERTHAN, GREATERTHAN);
			Transition matchAny{ MATCHANY, GREATERTHAN, [this, &matchAny, &secondGreaterThan](state_t incoming) {
				if (incoming == LESSTHAN)
				{
					this->nLessThan++;
				}
				else if (incoming == GREATERTHAN)
				{
					if (this->nLessThan - 2 == this->nGreaterThan++)
					{
						return Transition::ResultType{CONTINUE, &secondGreaterThan};
					}
				}
				return Transition::ResultType{CONTINUE, &matchAny};
			} };
			Transition secondLessThan = Transition::createTransitionToMatchAny(LESSTHAN, &matchAny);
			Transition start{ START, LESSTHAN, &secondLessThan };
			bytesParsed = parse<DictToken>(span, &start);
			if (bytesParsed != -1)
			{
				storedBytes = span.first(bytesParsed);
			}
		}	
	};

} // storyt::_internal

#endif // STORYT_PDF_TOKENS_H