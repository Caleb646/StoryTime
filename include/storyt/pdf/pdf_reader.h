

#ifndef STORYT_PDF_TEXT_READER_H
#define STORYT_PDF_TEXT_READER_H

#include <cstddef>
#include <vector>
#include <array>
#include <span>

#include <storyt/common.h>
#include <storyt/pdf/tokens.h>

namespace storyt::_internal
{
	class PDFTextReader
	{
	public:
		explicit PDFTextReader(std::vector<byte_t>& bytes)
			: m_pdf(std::span(bytes)) 
		{
			
			for (size_t i = 0; i < m_pdf.size(); ++i)
			{
				byte_t byte = m_pdf[i];
				switch (m_pdf[i])
				{
				//case ObjToken::firstCharacter:
				//	{
				//		ObjToken obj(m_pdf.subspan(i));
				//		i += obj.nParsedCharacters();
				//		break;
				//	}
				case DictToken::firstCharacter:
				{
					DictToken dict(m_pdf.subspan(i));
					i += dict.idx;
					break;
				}
				}
			}
		}
		
	private:
		std::span<byte_t> m_pdf;
	};

} // End of storyt::_internal

#endif // End of STORYT_PDF_TEXT_READER_H