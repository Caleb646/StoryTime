

#ifndef STORYT_PDF_TEXT_READER_H
#define STORYT_PDF_TEXT_READER_H

#include <cstddef>
#include <vector>
#include <array>
#include <string>
#include <span>
#include <unordered_map>

#include <storyt/common.h>
#include <storyt/pdf/parsers.h>


namespace storyt::_internal
{
	class PDFTextReader
	{
	public:
		explicit PDFTextReader(std::vector<byte_t>& bytes)
			: m_pdf(std::span(bytes)) 
		{	
			int trailerObjectID{ NOTFOUND };
			for (size_t i = 0; i < m_pdf.size(); ++i)
			{
				PDFObject obj = PDFObject::create(m_pdf.subspan(i));
				i += obj.getTotalBytesParsed();
				if (obj.isValid())
				{
					addObject(obj);
					for (const auto& subObj : obj.getSubObjects())
					{
						addObject(subObj);
					}
					// store the latest valid PDF Object id
					trailerObjectID = obj.getID();
				}
			}
			// The final pdf object should be the pdf trailer
			if (trailerObjectID != NOTFOUND)
			{
				m_trailer = getObject(trailerObjectID);
				STORYT_ASSERT(m_trailer->dictHasName("/Root"), "Invalid PDF");
			}
			const IndirectReference rootRef = m_trailer->getDictValueAs<IndirectReference>("/Root");
			m_root = getObject(rootRef.id);

			const IndirectReference pageRef = m_root->getDictValueAs<IndirectReference>("/Pages");
			m_pages = getObject(pageRef.id);
			std::vector<IndirectReference> pageRefs = IndirectReference::create(m_pages->getDictValue("/Kids"), getPageCount());

			PDFObject* firstPage = getObject(pageRefs.at(0));
			readPageText(firstPage);
		}

		void readPageText(PDFObject* page)
		{
			if (page != nullptr)
			{
				PDFObject* pageContent = getObject(
					page->getDictValueAs<IndirectReference>("/Contents")
				);
				PageText text = PageText::create(pageContent->decompressStream());
				for (auto& block : text)
				{
					if (block.fontName.size() > 0)
					{
						const IndirectReference c0_0 = page->getDictValueAs<IndirectReference>(block.fontName);
						PDFObject* font = getObject(c0_0);
						const IndirectReference cmapRef = font->getDictValueAs<IndirectReference>("/ToUnicode");
						PDFObject* cmap = getObject(cmapRef);
						Cmap map = Cmap::create(cmap->decompressStream());
						std::string decodedText = PageText::decode(block, map);
					}
				}
			}
		}

		int getPageCount()
		{
			if (m_pages != nullptr)
			{
				return m_pages->getDictValueAs<int>("/Count");
			}
			return 0;
		}

		void addObject(const PDFObject& obj)
		{
			if (obj.isValid())
			{
				STORYT_ASSERT(!m_objects.contains(obj.getID()), "Duplicate PDF Object found");
				m_objects.emplace(
					std::piecewise_construct,
					std::forward_as_tuple(obj.getID()),
					std::forward_as_tuple(obj)
				);
			}
		}
		PDFObject* getObject(IndirectReference ref)
		{
			return getObject(ref.id);
		}
		PDFObject* getObject(int objId)
		{
			if (m_objects.contains(objId))
			{
				return &m_objects.at(objId);
			}
			return nullptr;
		}
		
	private:
		PDFObject* m_trailer{ nullptr };
		PDFObject* m_root{ nullptr };
		PDFObject* m_pages{ nullptr };
		std::unordered_map<int, PDFObject> m_objects;
		std::span<byte_t> m_pdf;
	};

} // End of storyt::_internal

#endif // End of STORYT_PDF_TEXT_READER_H