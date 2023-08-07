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



} // storyt::_internal

#endif // STORYT_PDF_TOKENS_H