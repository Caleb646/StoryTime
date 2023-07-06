#include <iostream>
#include <stdarg.h>
#include <cassert>
#include <vector>
#include <string>
#include <fstream>
#include <type_traits>
#include <utility>
#include <map>
#include <functional>
#include <unordered_map>

#include "types.h"
#include "utils.h"
#include "core.h"

#ifndef READER_NDB_H
#define READER_NDB_H

namespace reader {
	namespace ndb {

        struct PageTrailer
        {

            /**
             * ptype (1 byte): This value indicates the type of data contained within the page.
            */
            types::PType ptype{types::PType::INVALID};

            /*
            * ptypeRepeat (1 byte): MUST be set to the same value as ptype.
            */
            types::PType ptypeRepeat{};

            /*
            * wSig (2 bytes): Page signature. This value depends on the value of the ptype field.
            *   This value is zero (0x0000) for AMap, PMap, FMap, and FPMap pages. For BBT, NBT,
            *   and DList pages, a page / block signature is computed (see section 5.5).
            */
            std::uint16_t wSig{};

            /*
            * dwCRC (4 bytes): 32-bit CRC of the page data, excluding the page trailer.
            *   See section 5.3 for the CRC algorithm. Note the locations of the dwCRC and
            *   bid are differs between the Unicode and ANSI version of this structure.
            */
            std::uint32_t dwCRC{};

            /*
            * bid (Unicode: 8 bytes; ANSI 4 bytes): The BID of the page's block. AMap, PMap,
            *   FMap, and FPMap pages have a special convention where their BID is assigned the
            *   same value as their IB (that is, the absolute file offset of the page). The bidIndex
            *   for other page types are allocated from the special bidNextP counter in the HEADER structure.
            */
            core::BID bid{};
        };

        struct BlockTrailer
        {
            /// (2 bytes): The amount of data, in bytes, contained within the data section of the block.
            /// This value does not include the block trailer or any unused bytes that can exist after the end
            /// of the data and before the start of the block trailer.
            int64_t cb{};
            /// (2 bytes): Block signature. See section 5.5
            /// *for the algorithm to calculate the block signature.
            int64_t wSig{};
            /// (4 bytes): 32-bit CRC of the cb bytes of raw data, see section 5.3 for the algorithm to calculate the CRC.
            int64_t dwCRC{};
            /// (Unicode: 8 bytes; ANSI 4 bytes): The BID (section 2.2.2.2) of the data block.
            core::BID bid{};

            BlockTrailer() = default;

            BlockTrailer(const std::vector<types::byte_t>& bytes, core::BREF bref)
                : BlockTrailer(bytes)
            {
                auto computedSig = utils::ms::ComputeSig(bref.ib, bref.bid.getBidRaw());
                ASSERT(std::cmp_equal(wSig, computedSig),
                    "[ERROR] Page Sig [%i] != Computed Sig [%i]", wSig, computedSig);
            }

            BlockTrailer(const std::vector<types::byte_t>& bytes)
            {
                ASSERT((bytes.size() == 16), "[ERROR] Block Trailer has to be 16 bytes not %i", bytes.size());

                /*
                * cb (2 bytes):
                *   The amount of data, in bytes, contained within the data section of the block.
                *   This value does not include the block trailer or any unused bytes that can exist after the
                *   end of the data and before the start of the block trailer.
                *
                * wSig (2 bytes): Block signature. See section 5.5 for the algorithm to calculate the block signature.
                *
                * dwCRC (4 bytes): 32-bit CRC of the cb bytes of raw data, see section 5.3 for the algorithm to calculate
                *   the CRC
                *
                * Bid (8 bytes): The BID (section 2.2.2.2) of the data block.
                */
                cb = utils::slice(bytes, 0, 2, 2, utils::toT_l<int64_t>);
                wSig = utils::slice(bytes, 2, 4, 2, utils::toT_l<int64_t>);
                dwCRC = utils::slice(bytes, 4, 8, 4, utils::toT_l<int64_t>);
                bid = core::readBID(utils::slice(bytes, 8, 16, 8));
            }
        };

        class BTEntry
        {
        public:
            /// (Unicode: 8 bytes; ANSI: 4 bytes): The key value associated with this BTENTRY
            /// The btkey is either an NID (zero extended to 8 bytes for Unicode PSTs)
            /// or a BID, depending on the ptype of the page.
            std::vector<types::byte_t> btkey{};

            /// (Unicode: 16 bytes): BREF structure (section 2.2.2.4) that points to the child BTPAGE.
            core::BREF bref{};
        public:
            static constexpr int64_t id() { return 1; }
        };

        class BBTEntry
        {
        public:
            /// BREF (Unicode: 16 bytes; ANSI: 8 bytes): BREF structure (section 2.2.2.4) that contains the BID and
            /// IB of the block that the BBTENTRY references.
            core::BREF bref{};
            /// cb (2 bytes): The count of bytes of the raw data contained
            /// in the block referenced by BREF excluding the block trailer and alignment padding, if any.
            std::uint16_t cb{};
            /// cRef (2 bytes): Reference count indicating the count of references to this block. See section 2.2.2.7.7.3.1 regarding
            /// how reference counts work.
            std::uint16_t cRef{};
            /// dwPadding (Unicode file format only, 4 bytes): Padding; MUST be set to zero.
            std::uint32_t dwPadding{};

        public:
            static constexpr int64_t id() { return 2; }
        };

        class NBTEntry
        {
        public:
            /*
            * nid (Unicode: 8 bytes; ANSI: 4 bytes):
            *   The NID (section 2.2.2.1) of the entry.
            *   Note that the NID is a 4-byte value for both Unicode and ANSI formats.
            *   However, to stay consistent with the size of the btkey member in BTENTRY,
            *   the 4-byte NID is extended to its 8-byte equivalent for Unicode PST files.

            * bidData (Unicode: 8 bytes; ANSI: 4 bytes):
            *   The BID of the data block for this node.
            *
            * bidSub (Unicode: 8 bytes; ANSI: 4 bytes):
            *   The BID of the subnode block for this node. If this value is zero, a
            *   subnode block does not exist for this node.
            *
            * nidParent (4 bytes): If this node
            *   represents a child of a Folder object defined in the Messaging Layer, then this value
            *   is nonzero and contains the NID of the parent Folder object's node. Otherwise,
            *   this value is zero. See section 2.2.2.7.7.4.1 for more information.
            *   This field is not interpreted by any structure defined at the NDB Layer.
            *
            * dwPadding (Unicode file format only, 4 bytes): Padding; MUST be set to zero.
            */

            core::NID nid{};
            core::NID nidParent{};
            core::BID bidData{};
            core::BID bidSub{};
            int64_t dwPadding{};

        public:
            bool hasSubNode() const { return bidSub.getBidIndex() != 0; }
            static constexpr int64_t id() { return 3; }
        };

        class Entry 
        {
        public:
            Entry(const std::vector<types::byte_t>& data) : m_data(data) {}

            BTEntry asBTEntry() const
            {
                ASSERT((m_data.size() == 24), "[ERROR] Incorrect size for BTEntry");
                BTEntry entry{};
                entry.btkey = utils::slice(m_data, 0, 8, 8);
                entry.bref = core::readBREF(utils::slice(m_data, 8, 24, 16));
                return entry;
            }

            BBTEntry asBBTEntry() const
            {
                ASSERT((m_data.size() == 24), "[ERROR] BBTEntry must be 24 bytes not %i", m_data.size());
                BBTEntry entry{};
                entry.bref = core::readBREF(utils::slice(m_data, 0, 16, 16));
                entry.cb = utils::slice(m_data, 16, 18, 2, utils::toT_l<uint16_t>);
                entry.cRef = utils::slice(m_data, 18, 20, 2, utils::toT_l<uint16_t>);
                entry.dwPadding = utils::slice(m_data, 20, 24, 4, utils::toT_l<uint32_t>);

                //ASSERT((entry.dwPadding == 0),
                //    "[ERROR] dwPadding must be 0 for BBTEntry not %i", entry.dwPadding);
                return entry;
            }

            NBTEntry asNBTEntry() const
            {
                ASSERT((m_data.size() == 32), "[ERROR] NBTEntry must be 32 bytes not %i", m_data.size());
                NBTEntry entry{};
                entry.nid = core::readNID(utils::slice(m_data, 0, 4, 4)); // NID is zero padded from 4 to 8
                entry.bidData = core::readBID(utils::slice(m_data, 8, 16, 8));
                entry.bidSub = core::readBID(utils::slice(m_data, 16, 24, 8));
                entry.nidParent = core::readNID(utils::slice(m_data, 24, 28, 4));
                entry.dwPadding = utils::slice(m_data, 28, 32, 4, utils::toT_l<int64_t>);
                //ASSERT((entry.dwPadding == 0),
                //   "[ERROR] dwPadding must be 0 for BBTEntry not %i", entry.dwPadding);
                return entry;
            }

            template<typename E>
            E as() const
			{
                if constexpr (E::id() == BTEntry::id())
				    return asBTEntry();

			    else if constexpr (E::id() == BBTEntry::id())
					return asBBTEntry();

				else if constexpr (E::id() == NBTEntry::id())
					return asNBTEntry();
                else
                {
                    ASSERT(false, "[ERROR] Invalid Type");
                    return E();
                }
			}

            size_t size() const { return m_data.size(); }

        private:
            std::vector<types::byte_t> m_data{};
        };

        class BTPage
        {
        public:
            std::vector<BTPage> subPages{};
            /// rgentries (Unicode: 488 bytes; ANSI: 496 bytes): Entries of the BTree array.
            /// The entries in the array depend on the value of the cLevel field. If cLevel is greater than 0,
            /// then each entry in the array is of type BTENTRY.If cLevel is 0, then each entry is either of type
            /// BBTENTRY or NBTENTRY, depending on the ptype of the page.
            std::vector<Entry> rgentries{};

            /*
            * BTree Type          cLevel             rgentries structure      cbEnt (bytes)
            *  NBT                  0                  NBTENTRY              ANSI: 16, Unicode: 32
            *  NBT            Greater than 0           BTENTRY               ANSI: 12, Unicode: 24
            */
            /// cEnt (1 byte): The number of BTree entries stored in the page data.
            std::int64_t cEnt{};
            /// cEntMax (1 byte): The maximum number of entries that can fit inside the page data.
            std::int64_t cEntMax{};
            /// cbEnt (1 byte): The size of each BTree entry, in bytes. Note that in some cases,
            /// cbEnt can be greater than the corresponding size of the corresponding rgentries structure
            /// because of alignment or other considerations.Implementations MUST use the size specified in cbEnt to advance to the next entry.
            std::int64_t cbEnt{};
            /// cLevel (1 byte): The depth level of this page.
            /// Leaf pages have a level of zero, whereas intermediate pages have a level greater than 0.
            /// This value determines the type of the entries in rgentries, and is interpreted as unsigned.
            std::int64_t cLevel{};

            /// dwPadding (Unicode: 4 bytes): Padding; MUST be set to zero.
            std::int64_t dwPadding{};

            /// pageTrailer (Unicode: 16 bytes):
            /// A PAGETRAILER structure(section 2.2.2.7.1).The ptype subfield of pageTrailer MUST
            /// be set to ptypeBBT for a Block BTree page, or ptypeNBT for a Node BTree page.The other subfields
            /// of pageTrailer MUST be set as specified in section 2.2.2.7.1.
            PageTrailer pageTrailer{};

        public:

            int64_t getEntryType() const
            {
                return BTPage::getEntryType(pageTrailer.ptype, cLevel);
            }

            template<typename T>
            static int64_t getEntryType(types::PType pagePtype, T cLevel)
            {
                ASSERT((pagePtype != types::PType::INVALID), 
                    "[ERROR] Pagetrailer was not setup properly. types::PType is set to INVALID .");
                if (cLevel == 0 && pagePtype == types::PType::NBT) // NBTENTRY 
                {
                    return NBTEntry::id();
                }

                else if (cLevel > 0) // BTENTRY 
                {
                    return BTEntry::id();
                }

                else if (cLevel == 0 && pagePtype == types::PType::BBT) // BBTENTRY (Leaf BBT Entry) 
                {
                    return BBTEntry::id();
                }

                ASSERT(false, "[ERROR] Invalid types::PType for BTPage %i with cLevel %i", (int64_t)pagePtype, cLevel);
                return -1;
            }

            bool hasBTEntries() const
			{
				return getEntryType() == BTEntry::id();
			}

            bool hasBBTEntries() const
            {
                return getEntryType() == BBTEntry::id();
			}

            bool hasNBTEntries() const
            {
                return getEntryType() == NBTEntry::id();
            }

            bool isLeafPage() const
            {
                return getEntryType() == BBTEntry::id() || getEntryType() == NBTEntry::id();
            }

            template<typename EntryType>
            bool has() const
            {
                if constexpr (EntryType::id() == BTEntry::id())
                    return hasBTEntries();
                else if constexpr (EntryType::id() == BBTEntry::id())
                    return hasBBTEntries();
                else if constexpr (EntryType::id() == NBTEntry::id())
                    return hasNBTEntries();
                else
                {
                    ASSERT(false, "[ERROR] Invalid EntryType");
                    return false;
                }
			}

            bool verify() const
            {
                _verify(*this);
                for(size_t i = 0; i < subPages.size(); i++)
                {
                    const BTPage& page = subPages.at(i);
                    _verify(page);
                }
                return true;
            }


            static BTPage readBTPage(
                const std::vector<types::byte_t>& btBytes,
                types::PType treeType,
                int64_t parentCLevel = -1,
                core::BREF* btPageBref = nullptr)
            {
                /*
                * Entries of the BTree array. The entries in the array depend on the value of the cLevel field.
                *   If cLevel is greater than 0, then each entry in the array is of type BTENTRY. If cLevel is 0,
                *   then each entry is either of type BBTENTRY or NBTENTRY, depending on the ptype of the page.
                */
                std::vector<types::byte_t> rgentriesBytes = utils::slice(btBytes, 0, 488, 488);
                /*
                * cEnt (1 byte): The number of BTree entries stored in the page data.
                *
                * cEntMax (1 byte): The maximum number of entries that can fit inside the page data.
                *
                * cbEnt (1 byte): The size of each BTree entry, in bytes. Note that in some cases, cbEnt can be
                *   greater than the corresponding size of the corresponding rgentries structure because of alignment or other
                *   considerations. Implementations MUST use the size specified in cbEnt to advance to the next entry.
                *
                *   BTree Type        cLevel      rgentries structure         cbEnt (bytes)
                *    NBT                0           NBTENTRY                ANSI: 16, Unicode: 32
                *    NBT           Greater than 0   BTENTRY                 ANSI: 12, Unicode: 24
                *    BBT                0           BBTENTRY                ANSI: 12, Unicode: 24
                *    BBT           Greater than 0   BTENTRY                 ANSI: 12, Unicode: 24
                *
                * cLevel (1 byte): The depth level of this page. Leaf pages have a level of zero,
                *   whereas intermediate pages have a level greater than 0. This value determines the
                *   type of the entries in rgentries, and is interpreted as unsigned.
                */
                std::vector<types::byte_t> buffer = utils::slice(btBytes, 488, 496, 8);

                ASSERT((btBytes.size() == 512), "[ERROR] BTPage must be 512 bytes in size");

                auto cEnt = utils::slice(buffer, 0, 1, 1, utils::toT_l<decltype(BTPage::cbEnt)>);
                auto cEntMax = utils::slice(buffer, 1, 2, 1, utils::toT_l<decltype(BTPage::cEntMax)>);
                auto cbEnt = utils::slice(buffer, 2, 3, 1, utils::toT_l<decltype(BTPage::cbEnt)>);
                auto cLevel = utils::slice(buffer, 3, 4, 1, utils::toT_l<decltype(BTPage::cLevel)>);

                /*
                * dwPadding (Unicode: 4 bytes): Padding; MUST be set to zero. Note there is no padding in the ANSI version of this structure.
                */
                auto dwPadding = utils::slice(buffer, 4, 8, 4, utils::toT_l<decltype(BTPage::dwPadding)>);

                if (parentCLevel != -1)
                {
                    ASSERT((parentCLevel - 1 == cLevel),
                        "[ERROR] SubBTPage cLevel [%i] must be smaller than ParentBTPage cLevel [%i]", cLevel, parentCLevel);
                }

                PageTrailer pageTrailer = BTPage::readPageTrailer(
                    utils::slice(btBytes, 496, 512, 16),
                    btPageBref
                );

                /**
                    * pageTrailer (Unicode: 16 bytes; ANSI: 12 bytes): A PAGETRAILER structure (section 2.2.2.7.1). The ptype
                    *  subfield of pageTrailer MUST be set to ptypeBBT for a Block BTree page, or ptypeNBT for a Node BTree page.
                    *  The other subfields of pageTrailer MUST be set as specified in section 2.2.2.7.1.
                */

                ASSERT((pageTrailer.ptype == types::PType::BBT || pageTrailer.ptype == types::PType::NBT), "[ERROR] Invalid ptype for pagetrailer");
                ASSERT((pageTrailer.ptype == treeType), "[ERROR] Pagetrailer types::PType and given Tree types::PType do not match");
                ASSERT((cEnt <= cEntMax), "[ERROR] Invalid cEnt %i", cEnt);
                ASSERT((dwPadding == 0), "[ERROR] dwPadding should be 0 not %i", dwPadding);

                BTPage page{};
                page.cEnt = cEnt;
                page.cbEnt = cbEnt;
                page.cEntMax = cEntMax;
                page.cLevel = cLevel;
                page.dwPadding = dwPadding;
                page.pageTrailer = pageTrailer;
                page.rgentries = BTPage::readEntries(rgentriesBytes, cEnt, cbEnt, cEntMax);
                return page;
            }

            static std::vector<Entry> readEntries(
                const std::vector<types::byte_t>& entriesInBytes,
                int64_t numEntries,
                int64_t singleEntrySize,
                int64_t maxNumberofEntries
            )
            {

                ASSERT((numEntries * singleEntrySize <= maxNumberofEntries * singleEntrySize),
                    "[ERROR] Exceeding maximum entries");

                std::vector<Entry> entries{};
                entries.reserve(numEntries * 2);
                for (int64_t i = 0; i < numEntries; i++)
                {
                    int64_t start = i * singleEntrySize;
                    int64_t end = (i + 1) * singleEntrySize;
                    int64_t size = end - start;
                    ASSERT((size == singleEntrySize),
                        "[ERROR] Size should be %i NOT %i", singleEntrySize, size);
                    std::vector<types::byte_t> entryBytes = utils::slice(entriesInBytes, start, end, size);

                    /**
                    * Entries of the BTree array. The entries in the array depend on the value of the cLevel field.
                    *   If cLevel is greater than 0, then each entry in the array is of type BTENTRY. If cLevel is 0,
                    *   then each entry is either of type BBTENTRY or NBTENTRY, depending on the ptype of the page.
                    */
                    entries.push_back(Entry(entryBytes));
                }
                ASSERT((std::cmp_equal(entries.size(), numEntries)), "[ERROR] Invalid number of entries %i", entries.size());
                return entries;
            }

            static PageTrailer readPageTrailer(const std::vector<types::byte_t>& pageTrailerBytes, const core::BREF* const bref = nullptr)
            {
                /**
                 * ptype (1 byte): This value indicates the type of data contained within the page. This field MUST contain one of the following values.
                */
                uint8_t ptypeByte = utils::slice(pageTrailerBytes, 0, 1, 1, utils::toT_l<uint8_t>);
                types::PType ptype = utils::getPType(ptypeByte);

                /*
                * ptypeRepeat (1 byte): MUST be set to the same value as ptype.
                */
                uint8_t ptypeRepeatByte = utils::slice(pageTrailerBytes, 1, 2, 1, utils::toT_l<uint8_t>);
                types::PType ptypeRepeat = utils::getPType(ptypeRepeatByte);

                ASSERT((ptypeByte == ptypeRepeatByte), "Ptype [%i] and PtypeRepeat [%i]", ptypeByte, ptypeRepeatByte);

                /*
                * wSig (2 bytes): Page signature. This value depends on the value of the ptype field.
                *   This value is zero (0x0000) for AMap, PMap, FMap, and FPMap pages. For BBT, NBT,
                *   and DList pages, a page / block signature is computed (see section 5.5).
                */
                uint16_t wSig = utils::slice(pageTrailerBytes, 2, 4, 2, utils::toT_l<uint16_t>);
                if (bref != nullptr)
                {
                    auto computedSig = utils::ms::ComputeSig(bref->ib, bref->bid.getBidRaw());
                    if (ptype == types::PType::NBT || ptype == types::PType::BBT)
                        ASSERT((wSig == computedSig), "[ERROR] Page Sig [%i] != Computed Sig [%i]", wSig, computedSig);
                }
                

                /*
                * dwCRC (4 bytes): 32-bit CRC of the page data, excluding the page trailer.
                *   See section 5.3 for the CRC algorithm. Note the locations of the dwCRC and
                *   bid are differs between the Unicode and ANSI version of this structure.
                */
                uint32_t dwCRC = utils::slice(pageTrailerBytes, 4, 8, 4, utils::toT_l<uint32_t>);

                /*
                * bid (Unicode: 8 bytes; ANSI 4 bytes): The BID of the page's block. AMap, PMap,
                *   FMap, and FPMap pages have a special convention where their BID is assigned the
                *   same value as their IB (that is, the absolute file offset of the page). The bidIndex
                *   for other page types are allocated from the special bidNextP counter in the HEADER structure.
                */
                core::BID bid = core::readBID(utils::slice(pageTrailerBytes, 8, 16, 8));

                PageTrailer pt{};
                pt.ptype = ptype;
                pt.ptypeRepeat = ptypeRepeat;
                pt.wSig = wSig;
                pt.dwCRC = dwCRC;
                pt.bid = bid;
                return pt;
            }

            private:
                bool _verify(const BTPage& page) const
                {
                    types::PType ptype = pageTrailer.ptype;
                    ASSERT((page.pageTrailer.ptype == ptype), "[ERROR] Subpage has different ptype than parent page.");
                    ASSERT(std::cmp_equal(page.cEnt, page.rgentries.size()), "[ERROR] Subpage has different number of entries than cEnt.");
                    ASSERT(std::cmp_not_equal(page.getEntryType(), -1), "[ERROR] Subpage has invalid entry type.");
                    int entryIdx{ 0 };
                    for (size_t i = 0; i < page.rgentries.size(); i++)
                    {
                        const Entry& entry = page.rgentries.at(i);
                        ASSERT(std::cmp_equal(entry.size(), page.cbEnt), "[ERROR] Entry Size mismatch");
                        entryIdx++;
                    }
                    return true;
                }
        };

        struct DataBlock
        {
            /// data (Variable): Raw data.
            std::vector<types::byte_t> data{};
            /// padding (Variable, Optional): Reserved.
            int32_t padding{0};
            BlockTrailer trailer{};
        };


        /*
        * @brief: XBLOCKs are used when the data associated with a node data that exceeds 8,176 bytes in size.
            The XBLOCK expands the data that is associated with a node by using an array of BIDs that reference data
            blocks that contain the data stream associated with the node. A BLOCKTRAILER is present at the end of an XBLOCK,
            and the end of the BLOCKTRAILER MUST be aligned on a 64-byte boundary.
        */
        class XBlock
        {
        public:
            /// btype (1 byte): Block type; MUST be set to 0x01 to indicate an XBLOCK or XXBLOCK.
            int32_t btype{};
            /// cLevel (1 byte): MUST be set to 0x01 to indicate an XBLOCK.
            int32_t cLevel{};
            /// cEnt (2 bytes): The count of BID entries in the XBLOCK.
            int32_t cEnt{};
            /// lcbTotal (4 bytes): Total count of bytes of all the external data stored in the data blocks referenced by XBLOCK.
            int64_t lcbTotal{};
            /// rgbid (variable): Array of BIDs that reference data blocks. The size is equal to the number of
            /// entries indicated by cEnt multiplied by the size of a BID(8 bytes for Unicode PST files).
            std::vector<core::BID> rgbid{};
            /// rgbPadding (variable, optional): This field is present if the total size of all of the other fields is not a multiple of 64.
            /// The size of this field is the smallest number of bytes required to make the size of the XBLOCK a multiple of 64.
            /// Implementations MUST ignore this field.
            int64_t rgbPadding{};
            /// blockTrailer (ANSI: 12 bytes; Unicode: 16 bytes): A BLOCKTRAILER structure (section 2.2.2.8.1).
            BlockTrailer trailer{};
        };

        /**
        * @brief The XXBLOCK further expands the data that is associated with a node by using an array of BIDs that reference XBLOCKs.
            A BLOCKTRAILER is present at the end of an XXBLOCK, and the end of the BLOCKTRAILER MUST be aligned on a 64-byte boundary.
        */
        class XXBlock
        {
        public:
            /// btype (1 byte): Block type; MUST be set to 0x01 to indicate an XBLOCK or XXBLOCK.
            int32_t btype{};
            /// cLevel (1 byte): MUST be set to 0x02 to indicate an XXBLOCK.
            int32_t cLevel{};
            /// cEnt (2 bytes): The count of BID entries in the XXBLOCK.
            int32_t cEnt{};
            /// lcbTotal (4 bytes): Total count of bytes of all the external data stored in XBLOCKs under this XXBLOCK.
            int64_t lcbTotal{};
            /// rgbid (variable): Array of BIDs that reference XBLOCKs. The size is equal to the number of entries indicated by cEnt
            /// multiplied by the size of a BID(8 bytes for Unicode PST files).
            std::vector<core::BID> rgbid{};
            /// rgbPadding (variable, optional): This field is present if the total size of all of the other fields is not a multiple of 64.
            /// The size of this field is the smallest number of bytes required to make the size of the XXBLOCK a multiple of 64.
            /// Implementations MUST ignore this field.
            int64_t rgbPadding{};
            /// blockTrailer (Unicode: 16 bytes): A BLOCKTRAILER structure (section 2.2.2.8.1).
            BlockTrailer trailer{};
        };

        struct Block 
        {
            types::BlockType blockType{types::BlockType::INVALID};
            std::vector<types::byte_t> data;
            BlockTrailer trailer{};
        };

        static BlockTrailer _readBlockTrailer(
            const std::vector<types::byte_t>& bytes, const core::BREF* const bref = nullptr)
        {
            ASSERT((bytes.size() == 16), "[ERROR] Block Trailer has to be 16 bytes not %i", bytes.size());

            /*
            * cb (2 bytes):
            *   The amount of data, in bytes, contained within the data section of the block.
            *   This value does not include the block trailer or any unused bytes that can exist after the
            *   end of the data and before the start of the block trailer.
            *
            * wSig (2 bytes): Block signature. See section 5.5 for the algorithm to calculate the block signature.
            *
            * dwCRC (4 bytes): 32-bit CRC of the cb bytes of raw data, see section 5.3 for the algorithm to calculate
            *   the CRC
            *
            * Bid (8 bytes): The BID (section 2.2.2.2) of the data block.
            */

            BlockTrailer trailer{};
            trailer.cb = utils::slice(bytes, 0, 2, 2, utils::toT_l<int64_t>);
            trailer.wSig = utils::slice(bytes, 2, 4, 2, utils::toT_l<int64_t>);
            trailer.dwCRC = utils::slice(bytes, 4, 8, 4, utils::toT_l<int64_t>);
            trailer.bid = core::readBID(utils::slice(bytes, 8, 16, 8));

            if (bref != nullptr)
            {
                auto computedSig = utils::ms::ComputeSig(bref->ib, bref->bid.getBidRaw());
                ASSERT(std::cmp_equal(trailer.wSig, computedSig),
                    "[ERROR] Page Sig [%i] != Computed Sig [%i]", trailer.wSig, computedSig);
            }
            return trailer;
        }

        static BlockTrailer sliceReadBlockTrailer(
            const std::vector<types::byte_t>& bytes,
            uint64_t blockTotalSize,
            const core::BREF* const bref = nullptr
        )
        {
            ASSERT((bytes.size() == blockTotalSize), "[ERROR]");
            const uint64_t blockTrailerSize = 16;
            const uint64_t blockTrailerStart = bytes.size() - blockTrailerSize;
            const uint64_t blockTrailerEnd = bytes.size();
            return _readBlockTrailer(utils::slice(bytes, blockTrailerStart, blockTrailerEnd, blockTrailerSize), bref);
        }

        static DataBlock _readDataBlock(
            const std::vector<types::byte_t>& blockBytes, BlockTrailer trailer, int64_t blockSize)
        {
            ASSERT(std::cmp_equal(blockBytes.size(), blockSize), "[ERROR] blockBytes.size() != blockSize");
            std::vector<types::byte_t> data = utils::slice(blockBytes, 0LL, trailer.cb, trailer.cb);
            ASSERT(std::cmp_equal(data.size(), trailer.cb), "[ERROR] blockBytes.size() != trailer.cb");

            size_t dwCRC = utils::ms::ComputeCRC(0, data.data(), static_cast<uint32_t>(trailer.cb));
            ASSERT(std::cmp_equal(trailer.dwCRC, dwCRC), "[ERROR] trailer.dwCRC != dwCRC");

            // TODO: The data block is not always encrypted or could be encrypted with a different algorithm
            utils::ms::CryptPermute(
                data.data(),
                static_cast<int>(data.size()),
                utils::ms::DECODE_DATA
            );

            //utils::ms::CryptCyclic(
            //    data.data(), 
            //    static_cast<int>(data.size()), 
            //    static_cast<utils::ms::DWORD>(trailer.bid.getBidRaw())
            //);

            DataBlock block{};
            block.data = std::move(data);
            block.trailer = trailer;
            return block;
        }

        static DataBlock readDataBlock(const std::vector<types::byte_t>& blockBytes, core::BREF bref)
        {
            ASSERT(!bref.bid.isInternal(), "[ERROR] A Data Block can NOT be marked as Internal");
            utils::ByteView view(blockBytes);
            BlockTrailer trailer(view.takeLast(16), bref);
            return _readDataBlock(blockBytes, trailer, blockBytes.size());
        }

        static XBlock _readXBlock(const std::vector<types::byte_t>& blockBytes, BlockTrailer trailer)
        {
            XBlock block{};
            block.btype = utils::slice(blockBytes, 0, 1, 1, utils::toT_l<int32_t>);
            block.cLevel = utils::slice(blockBytes, 1, 2, 1, utils::toT_l<int32_t>);
            block.cEnt = utils::slice(blockBytes, 2, 4, 2, utils::toT_l<int32_t>);
            block.lcbTotal = utils::slice(blockBytes, 4, 8, 4, utils::toT_l<int64_t>);
            block.trailer = trailer;

            ASSERT((block.btype == 0x01), "[ERROR] btype for XBlock should be 0x01 not %i", block.btype);
            ASSERT((block.cLevel == 0x01), "[ERROR] cLevel for XBlock should be 0x01 not %i", block.cLevel);

            uint64_t bidSize = 8;
            // from the end of lcbTotal to the end of the data field
            std::vector<types::byte_t> bidBytes = utils::slice(
                blockBytes,
                bidSize,
                static_cast<uint64_t>(trailer.cb + bidSize),
                static_cast<uint64_t>(trailer.cb)
            );
            for (uint64_t i = 0; i < block.cEnt; i++)
            {
                uint64_t start = i * bidSize;
                uint64_t end = (i + 1) * bidSize;
                ASSERT((end > start), "[ERROR]");
                ASSERT((end - start == bidSize), "[ERROR] BID must be of size 8 not %i", (end - start));
                block.rgbid.push_back(core::readBID(utils::slice(bidBytes, start, end, bidSize)));
            }
            return block;
        }

        static XXBlock _readXXBlock(const std::vector<types::byte_t>& blockBytes, BlockTrailer trailer)
        {
            XXBlock block{};
            block.btype = utils::slice(blockBytes, 0, 1, 1, utils::toT_l<int32_t>);
            block.cLevel = utils::slice(blockBytes, 1, 2, 1, utils::toT_l<int32_t>);
            block.cEnt = utils::slice(blockBytes, 2, 4, 2, utils::toT_l<int32_t>);
            block.lcbTotal = utils::slice(blockBytes, 4, 8, 4, utils::toT_l<int64_t>);
            block.trailer = trailer;

            ASSERT((block.btype == 0x01), "[ERROR] btype for XXBlock should be 0x01 not %i", block.btype);
            ASSERT((block.cLevel == 0x02), "[ERROR] cLevel for XXBlock should be 0x02 not %i", block.cLevel);

            const int32_t bidSize = 8;
            // from the end of lcbTotal to the end of the data field
            std::vector<types::byte_t> bidBytes = utils::slice(blockBytes, (int64_t)8, trailer.cb + (int64_t)8, trailer.cb);
            for (int32_t i = 0; i < block.cEnt; i++)
            {
                int32_t start = i * bidSize;
                int32_t end = ((int64_t)i + 1) * bidSize;
                ASSERT((end - start == bidSize), "[ERROR] BID must be of size 8 not %i", (end - start));
                block.rgbid.push_back(core::readBID(utils::slice(bidBytes, start, end, bidSize)));
            }
            return block;
        }

        class DataTree
        {
        public:
            using GetBBT_t = std::function<BBTEntry(const core::BID& bid)>;

        public:
            DataTree(core::Ref<std::ifstream> file, const GetBBT_t& getBBT, core::BREF bref, uint64_t sizeOfBlockData)
                : m_file(file), m_bref(bref), m_getBBT(getBBT), m_sizeofFirstBlockData(sizeOfBlockData)
            {
                static_assert(std::is_move_constructible_v<DataTree>, "DataTree must be move constructible");
                static_assert(std::is_move_assignable_v<DataTree>, "DataTree must be move assignable");
                static_assert(std::is_copy_constructible_v<DataTree>, "DataTree must be copy constructible");
                static_assert(std::is_copy_assignable_v<DataTree>, "DataTree must be copy assignable");
            }

            [[nodiscard]] const DataBlock& first() const
            {
                return m_dataBlocks.front();
            }

            [[nodiscard]] size_t size() const
            {
                return m_dataBlocks.size();
            }

            [[nodiscard]] size_t size(size_t dataBlockIdx) const
            {
                return m_dataBlocks.at(dataBlockIdx).data.size();
            }

            [[nodiscard]] bool isValid() const
			{
                return !m_dataBlocks.empty(); //|| !m_xBlocks.empty() || !m_xxBlocks.empty();
			}

            const std::vector<DataBlock>& blocks()
            {
                if (m_dataBlocks.size() > 0) // Data Blocks have already been read and cached in m_dataBlocks
                {
                    return m_dataBlocks;
                }
                    
                _init();
                return m_dataBlocks;
            }

            void init()
            {
                if (m_dataBlocks.empty())
                {
                    _init();
                }
            }

            const DataBlock& at(size_t idx) const
            {
                return m_dataBlocks.at(idx);
            }

            /**
             * @return pair<totatBlockSize, offset or padding>
            */
            static std::pair<uint64_t, uint64_t> getBlockTotalSize(uint64_t sizeofBlockData)
            {
                const uint64_t blockTrailerSize = 16;
                const uint64_t multiple = 64;
                const uint64_t remainder = (sizeofBlockData + blockTrailerSize) % multiple;
                const uint64_t offset = (multiple * (remainder != 0)) - remainder;
                // The block size is the smallest multiple of 64 that can hold both the data and the block trailer.
                const uint64_t blockSize = (sizeofBlockData + blockTrailerSize) + offset;

                ASSERT((blockSize % 64 == 0),
                    "[ERROR] Block Size must be a mutiple of 64");
                ASSERT((blockSize <= 8192),
                    "[ERROR] Block Size must less than or equal to the max blocksize of 8192");
                return { blockSize, offset };
            }

        private:

            std::vector<types::byte_t> readBlockBytes(core::BREF bref, uint64_t blockTotalSize)
            {
                m_file->seekg(bref.ib, std::ios::beg);
                return utils::readBytes(m_file.get(), blockTotalSize);
            }

            void _init()
            {
                /*
                * data (variable): Raw data.
                *
                * padding (variable, optional): Reserved.
                *
                * cb (2 bytes):
                *   The amount of data, in bytes, contained within the data section of the block.
                *   This value does not include the block trailer or any unused bytes that can exist after the
                *   end of the data and before the start of the block trailer.
                *
                * wSig (2 bytes): Block signature. See section 5.5 for the algorithm to calculate the block signature.
                *
                * dwCRC (4 bytes): 32-bit CRC of the cb bytes of raw data, see section 5.3 for the algorithm to calculate
                *   the CRC
                *
                * Bid (8 bytes): The BID (section 2.2.2.2) of the data block.
                */

                /*
                *   Block type            Data structure    Internal BID?    Header level    Array content
                *
                *   Data Tree               Data block            No               N/A             Bytes
                *                           XBLOCK                Yes              1               Data block reference
                *                           XXBLOCK                                2               XBLOCK reference
                *   Subnode BTree data      SLBLOCK                                0               SLENTRY
                *                           SIBLOCK                                1               SIENTRY
                */

                const auto [blockSize, offset] = getBlockTotalSize(m_sizeofFirstBlockData);
                const uint64_t blockTrailerSize = 16;

                std::vector<types::byte_t> blockBytes = readBlockBytes(m_bref, blockSize);
                BlockTrailer trailer = sliceReadBlockTrailer(blockBytes, blockSize, &m_bref);

                ASSERT((trailer.bid == m_bref.bid), "[ERROR] Bids should match");

                ASSERT((blockSize - (blockTrailerSize + offset) == trailer.cb),
                    "[ERROR] Given BlockSize [%i] != Trailer BlockSize [%i]", blockSize, trailer.cb);

                ASSERT((m_sizeofFirstBlockData == trailer.cb),
                    "[ERROR] Given sizeofBlockData [%i] != Trailer BlockSize [%i]", m_sizeofFirstBlockData, trailer.cb);

                if (!trailer.bid.isInternal()) // Data Block
                {
                    m_dataBlocks.push_back(_readDataBlock(blockBytes, trailer, blockSize));
                    return; // If the first block is a data block then we are done.
                }
                else // XBlock or XXBlock
                {
                    const auto btype = utils::slice(blockBytes, 0, 1, 1, utils::toT_l<uint32_t>);
                    if (btype == 0x01) // XBlock
                    {
                        XBlock xblock = _readXBlock(blockBytes, trailer);
                        xBlocktoDataBlocks(xblock);
                        return;
                    }

                    else if (btype == 0x02) // XXBlock
                    {
                        XXBlock xxblock = _readXXBlock(blockBytes, trailer);
                        xxBlocktoDataBlocks(xxblock);
                        return;
                    }
                    else
                    {
                        ASSERT(false, "[ERROR] Invalid btype must 0x01 or 0x02 not [%X]", btype);
                    }
                }
                ASSERT(false, "[ERROR] Unknown block type");
            }

            void xBlocktoDataBlocks(const XBlock& xblock)
            {
                for (const core::BID& bid : xblock.rgbid)
                {
                    BBTEntry bbt = m_getBBT(bid);
                    const auto [blockSize, offset] = getBlockTotalSize(bbt.cb);
                    std::vector <types::byte_t> bytes = readBlockBytes(bbt.bref, blockSize);
                    BlockTrailer trailer = sliceReadBlockTrailer(bytes, blockSize, &bbt.bref);
                    m_dataBlocks.push_back(_readDataBlock(bytes, trailer, blockSize));
                }
            }

            void xxBlocktoDataBlocks(const XXBlock& xxblock)
            {
                std::vector<XBlock> xblocks{};
                for (const core::BID& bid : xxblock.rgbid)
                {
                    BBTEntry bbt = m_getBBT(bid);
                    const auto [blockSize, offset] = getBlockTotalSize(bbt.cb);
                    std::vector <types::byte_t> bytes = readBlockBytes(bbt.bref, blockSize);
                    BlockTrailer trailer = sliceReadBlockTrailer(bytes, blockSize, &bbt.bref);
                    xblocks.push_back(_readXBlock(bytes, trailer));
                }
                for (const XBlock& xblock : xblocks)
                {
                    for (const core::BID& bid : xblock.rgbid)
                    {
                        BBTEntry bbt = m_getBBT(bid);
                        const auto [blockSize, offset] = getBlockTotalSize(bbt.cb);
                        std::vector <types::byte_t> bytes = readBlockBytes(bbt.bref, blockSize);
                        BlockTrailer trailer = sliceReadBlockTrailer(bytes, blockSize, &bbt.bref);
                        m_dataBlocks.push_back(_readDataBlock(bytes, trailer, blockSize));
                    }
                }
            }

        private:
            core::Ref<std::ifstream> m_file;
            core::BREF m_bref;
            GetBBT_t m_getBBT;
            uint64_t m_sizeofFirstBlockData{ 0 };
            std::vector<DataBlock> m_dataBlocks{};
        };
        /**
         * @brief SLENTRY are records that refer to internal subnodes of a node.
        */
        struct SLEntry
        {
            /// (Unicode: 8 bytes; ANSI: 4 bytes): Local NID of the subnode. This NID is guaranteed to be
            /// unique only within the parent node.
            core::NID nid;
            /// (Unicode: 8 bytes; ANSI: 4 bytes): The BID of the data block associated with the
            /// subnode.
            core::BID bidData;
            /// (Unicode: 8 bytes; ANSI: 4 bytes): If nonzero, the BID of the subnode of this subnode.
            core::BID bidSub;

            SLEntry(const std::vector<types::byte_t>& bytes)
            {
                ASSERT((bytes.size() == 24), "[ERROR]");
                utils::ByteView view(bytes);
                nid = view.entry<core::NID>(8);//core::NID(utils::slice(bytes, 0, 8, 8));
                bidData = view.entry<core::BID>(8);//core::BID(utils::slice(bytes, 8, 16, 8));
                bidSub = view.entry<core::BID>(8);//core::BID(utils::slice(bytes, 16, 24, 8));
            }
        };

        /*
        * @brief SIENTRY are intermediate records that point to SLBLOCKs.
        */
        struct SIEntry
        {
            /// (Unicode: 8 bytes; ANSI: 4 bytes): The key NID value to the next-level child block. This NID is
            /// only unique within the parent node.The NID is extended to 8 bytes in order for Unicode PST files
            /// to follow the general convention of 8 - byte indices
            core::NID nid;
            /// (Unicode: 8 bytes; ANSI: 4 bytes): The BID of the SLBLOCK
            core::BID bid;

            SIEntry(const std::vector<types::byte_t>& bytes)
            {
                ASSERT((bytes.size() == 16), "[ERROR]");
                utils::ByteView view(bytes);
                nid = view.entry<core::NID>(8);
                bid = view.entry<core::BID>(8);
            }
        };

        struct SLBlock
        {
            /// (1 byte): Block type; MUST be set to 0x02.
            uint8_t btype;
            /// (1 byte): MUST be set to 0x00.
            uint8_t cLevel;
            /// (2 bytes): The number of SLENTRYs in the SLBLOCK. This value and the number of elements in
            /// the rgentries array MUST be non - zero.When this value transitions to zero, it is required for the
            /// block to be deleted.
            uint16_t cEnt;
            /// (4 bytes, Unicode only): Padding; MUST be set to zero.
            uint32_t dwPadding;
            /// (variable size) Array of SLENTRY structures.The size is equal to the number of entries
            /// indicated by cEnt multiplied by the size of an SLENTRY(24 bytes for Unicode PST files)
            std::vector<SLEntry> entries;
            /// (Unicode: 16 bytes): A BLOCKTRAILER structure
            BlockTrailer trailer;

            SLBlock(const std::vector<types::byte_t>& bytes, core::BREF bref)
            {
                ASSERT(bref.bid.isInternal(), "[ERROR]");
                utils::ByteView view(bytes);
                btype = view.read<uint8_t>(1); //utils::slice(bytes, 0, 1, 1, utils::toT_l<uint8_t>);
                cLevel = view.read<uint8_t>(1); //utils::slice(bytes, 1, 2, 1, utils::toT_l<uint8_t>);
                cEnt = view.read<uint16_t>(2); //utils::slice(bytes, 2, 4, 2, utils::toT_l<uint16_t>);
                dwPadding = view.read<uint32_t>(4); //utils::slice(bytes, 4, 8, 2, utils::toT_l<uint32_t>);
                entries = view.entries<SLEntry>(cEnt, 24);

                /// rgbPadding (optional, variable): This field is present if the total size of all of the other fields is not
                /// a multiple of 64. The size of this field is the smallest number of bytes required to make the size of
                ///    the SLBLOCK a multiple of 64. 
                trailer = BlockTrailer(view.takeLast(16), bref);

                ASSERT((btype == 0x02), "[ERROR]");
                ASSERT((cLevel == 0x00), "[ERROR]");
                ASSERT((cEnt != 0), "[ERROR]");
            }
        };

        struct SIBlock
        {
            /// (1 byte): Block type; MUST be set to 0x02.
            uint8_t btype;
            /// (1 byte): MUST be set to 0x01.
            uint8_t cLevel;
            /// (2 bytes): The number of SIENTRYs in the SIBLOCK.
            uint16_t cEnt;
            /// (4 bytes, Unicode only): Padding; MUST be set to zero.
            uint32_t dwPadding;
            /// (variable size): Array of SIENTRY structures. The size is equal to the number of entries
            /// indicated by cEnt multiplied by the size of an SIENTRY(16 bytes for Unicode PST files)
            std::vector<SIEntry> entries;
            /// (16 bytes)
            BlockTrailer trailer;

            SIBlock(const std::vector<types::byte_t>& bytes, core::BREF bref)
            {
                ASSERT(bref.bid.isInternal(), "[ERROR]");
                utils::ByteView view(bytes);
                btype = view.read<uint8_t>(1);
                cLevel = view.read<uint8_t>(1);
                cEnt = view.read<uint16_t>(2);
                dwPadding = view.read<uint32_t>(4);
                entries = view.entries<SIEntry>(cEnt, 16);
                /*
                * rgbPadding (optional, variable): This field is present if the total size of all of the other fields is not
                * a multiple of 64. The size of this field is the smallest number of bytes required to make the size of
                * the SIBLOCK a multiple of 64. Implementations MUST ignore this field.
                */
                trailer = BlockTrailer(view.takeLast(16), bref);

                ASSERT((btype == 0x02), "[ERROR]");
                ASSERT((cLevel == 0x01), "[ERROR]");
            }
        };

        class SubNodeBTree
        {
        public:
            using GetBBT_t = std::function<BBTEntry(const core::BID& bid)>;
            using RawNID_Type = uint32_t;
            
        public:

            SubNodeBTree(core::BID bid, core::Ref<std::ifstream> file, const GetBBT_t& getBBT)
                : m_bid(bid), m_file(file), m_getBBT(getBBT)
            {
                if (m_bid.getBidRaw() != 0) // When BID == 0 there is no subnode tree
                {
                    const auto [bytes, bbt] = readBlock(m_bid);
                    const uint8_t clevel = bytes.at(1);
                    if (clevel == 0x00) // SL Block
                    {
                        m_slblocks.push_back(SLBlock{ bytes, bbt.bref });
                    }
                    else if (clevel == 0x01) // SI Block
                    {
                        SIBlock siblock{ bytes, bbt.bref };
                        for (const auto& sientry : siblock.entries)
                        {
                            const auto [slBlockBytes, slBlockBBT] = readBlock(sientry.bid);
                            m_slblocks.emplace_back(SLBlock(slBlockBytes, slBlockBBT.bref));
                        }
                    }
                    else
                    {
                        ASSERT(false, "[ERROR] Unknown SubNode Type");
                    }

                    for (const auto& slblock : m_slblocks)
                    {
                        for (const SLEntry& slentry : slblock.entries)
                        {
                            if (slentry.bidSub.getBidRaw() != 0) // theres a nested subnode btree
                            {
                                LOG("[WARN] A nested SubNodeBTree was created");
                                m_subtrees.emplace_back(SubNodeBTree(slentry.bidSub, m_file, m_getBBT));
                            }
                            const uint32_t nidID = slentry.nid.getNIDRaw();
                            ASSERT(!m_datatrees.contains(nidID), "[ERROR] Duplicate entry");
                            const auto [dataTreeBytes, dataTreeBBT] = readBlock(slentry.bidData);
                            m_datatrees.emplace(
                                std::piecewise_construct,
                                std::forward_as_tuple(nidID),
                                std::forward_as_tuple(m_file, m_getBBT, dataTreeBBT.bref, dataTreeBBT.cb)
                            );
                            m_datatrees.at(nidID).init();
                        }
                    }
                }
            }

            [[nodiscard]] DataTree* at(core::NID nid)
            {
                if (m_datatrees.contains(nid.getNIDRaw()))
                {
                    return &m_datatrees.at(nid.getNIDRaw());
                }
                for (SubNodeBTree& subtree : m_subtrees)
                {
                    DataTree* data = subtree.at(nid);
                    if (data != nullptr)
                    {
                        return data;
                    }
                }
                LOG("[WARN] Failed to find DataBlock in SubNode Tree");
                return nullptr;
            }

            [[nodiscard]] const DataTree* const at(core::NID nid) const
            {
                if (m_datatrees.contains(nid.getNIDRaw()))
                {
                    return &m_datatrees.at(nid.getNIDRaw());
                }
                for (const auto& subtree : m_subtrees)
                {
                    const auto data = subtree.at(nid);
                    if (data != nullptr)
                    {
                        return data;
                    }
                }
                LOG("[WARN] Failed to find DataBlock in SubNode Tree");
                return nullptr;
            }

            std::pair<std::vector<types::byte_t>, BBTEntry> readBlock(core::BID bid)
            {
                const BBTEntry bbt = m_getBBT(bid);
                const size_t totalBlockSize = calcBlockSize(bbt.cb);
                m_file->seekg(bbt.bref.ib, std::ios::beg);
                return { utils::readBytes(m_file.get(), totalBlockSize), bbt };
            }

            size_t calcBlockSize(size_t dataSize)
            {
                const size_t sBType = 1;
                const size_t sCLevel = 1;
                const size_t sCEnt = 2;
                const size_t sDwPadding = 4;
                const size_t blockTrailerSize = 16;
                const size_t totalBlockSizeWithoutPadding = dataSize + blockTrailerSize + sBType + sCLevel + sCEnt + sDwPadding;

                const size_t multiple = 64;
                const size_t remainder = totalBlockSizeWithoutPadding % multiple;
                const size_t offset = (multiple * (remainder != 0)) - remainder;
                // The block size is the smallest multiple of 64 that can hold both the data and the block trailer.
                return totalBlockSizeWithoutPadding + offset;
            }

        private:
            core::BID m_bid;
            core::Ref<std::ifstream> m_file;
            GetBBT_t m_getBBT;
            std::vector<SLBlock> m_slblocks;
            std::vector<SubNodeBTree> m_subtrees;
            // uint32_t is a Raw NID
            std::unordered_map<uint32_t, DataTree> m_datatrees;
        };

        template<typename LeafEntryType>
        class BTree
        {
        public:
            BTree(BTPage rootPage)
            {
                addSubPage(rootPage);
                _init();
            }
            const BTPage& rootPage() const
            {
			    return m_pages.at(0);
		    }

            types::PType ptype() const
            {
				return rootPage().pageTrailer.ptype;
			}

            int64_t cLevel() const
            {
                return rootPage().cLevel;
            }
            size_t size() const
            {
                return m_pages.size();
            }

            const BTPage& at(size_t index) const
            {
				return m_pages.at(index);
			}

            auto begin() const
            {
                return m_pages.begin();
            }

            auto end() const
            {
                return m_pages.end();
            }

            bool addSubPage(BTPage page)
            {
				m_pages.push_back(page);
				return true;
			}

            std::map<types::NIDType, NBTEntry> all(core::NID nid) const
            {
                std::map<types::NIDType, NBTEntry> entries{};
                for (size_t i = 0; i < m_pages.size(); i++)
                {
                    const BTPage& page = m_pages.at(i);
                    if (page.has<NBTEntry>())
                    {
                        for (size_t j = 0; j < page.rgentries.size(); j++)
                        {
                            NBTEntry entry = page.rgentries.at(j).as<NBTEntry>();
                            // NID Index is used because a single PST Folder has 4 parts and those 4 parts
                            // have the same NID Index but different NID Types. The == operator for NID class compares
                            // the NID Type and NID Index.
                            if (entry.nid.getNIDIndex() == nid.getNIDIndex())
                            {
                                    ASSERT((entries.count(entry.nid.getNIDType()) == 0), "Duplicate NID Type found in NBT");
                                    entries[entry.nid.getNIDType()] = entry;
                            }
                        }
                    }
                }
                return entries;
            }

            template<typename NIDorBID>
            LeafEntryType get(NIDorBID id) const
            {
                //if constexpr (std::is_same_v<NIDorBID, core::NID>)
                //    static_assert(std::is_same_v<LeafEntryType, NBTEntry>, "NID can only be used with Node BTree");
                //else
                //    static_assert(std::is_same_v<LeafEntryType, BBTEntry>, "BID can only be used with Block BTree");

                for (size_t i = 0; i < m_pages.size(); i++)
                {
                    const BTPage& page = m_pages.at(i);
                    if (page.has<LeafEntryType>())
                    {
                        for (size_t j = 0; j < page.rgentries.size(); j++)
                        {
                            LeafEntryType entry = page.rgentries.at(j).as<LeafEntryType>();
                            if constexpr (LeafEntryType::id() == BBTEntry::id())
                            {
                                if (entry.bref.bid == id)
                                {
                                    return entry;
                                }
							}
							else
							{
                                if (entry.nid == id)
                                {
								    return entry;
                                }
                            }
                        }
                    }
                }
                return LeafEntryType{};
            }

            template<typename NIDorBID>
            uint32_t count(NIDorBID id)
            {
                //bool t = std::is_same_v<NIDorBID, core::NID>;
                //if constexpr (std::is_same_v<NIDorBID, core::NID> == true)
                //    static_assert(std::is_same_v<LeafEntryType, NBTEntry> == true, "NID can only be used with Node BTree");
                //else if constexpr (std::is_same_v<NIDorBID, core::BID> == true)
                //    static_assert(std::is_same_v<LeafEntryType, BBTEntry> == true, "BID can only be used with Block BTree");
                //else
                //    static_assert(false, "Invalid ID type");

                uint32_t count{ 0 };
                for (size_t i = 0; i < m_pages.size(); i++)
                {
                    const BTPage& page = m_pages.at(i);
                    if (page.has<LeafEntryType>())
                    {
                        for (size_t j = 0; j < page.rgentries.size(); j++)
                        {
                            LeafEntryType entry = page.rgentries.at(j).as<LeafEntryType>();
                            if constexpr (LeafEntryType::id() == BBTEntry::id())
                            {
                                if (entry.bref.bid == id)
                                {
                                    count++;
                                }
                            }
                            else
                            {
                                if (entry.nid == id)
                                {
                                    count++;
                                }
                            }
                        }
                    }
                }
                return count;
            }

            bool verify()
            {
                for (size_t i = 0; i < m_pages.size(); i++)
                {
                    const BTPage& page = m_pages.at(i);
                    page.verify();
                }
                return true;
            }


        private:
            void _init()
            {
                static_assert((std::is_same_v<LeafEntryType, BBTEntry>
                        || std::is_same_v<LeafEntryType, NBTEntry>, "Invalid LeafEntryType. Must be BBTEntry or NBTEntry."));

                m_pages.reserve(20000);
                //ASSERT((rootPage().hasBTEntries() == true), "[ERROR] Root BTPage must have BTEntries");
                if constexpr (std::is_same_v<LeafEntryType, NBTEntry>)
                {
                    ASSERT((rootPage().pageTrailer.ptype == types::PType::NBT), "[ERROR] Root BTPage with NBTEntries must be NBT");
                }   
                else
                {
                    ASSERT((rootPage().pageTrailer.ptype == types::PType::BBT), "[ERROR] Root BTPage with BBTEntries must be BBT");
                }
                   
            }
        private:
            std::vector<BTPage> m_pages{};
        };

        class NDB
        {
        public:
            NDB(
                std::ifstream& file,
                core::Header header)
                :
                m_file(file),
                m_header(header),
                m_rootNBT(BTree<NBTEntry>(_readBTPage(m_header.root.nodeBTreeRootPage, types::PType::NBT))),
                m_rootBBT(BTree<BBTEntry>(_readBTPage(m_header.root.blockBTreeRootPage, types::PType::BBT)))
            {
                _init();
            }

            std::map<types::NIDType, NBTEntry> all(core::NID nid) const
			{
				return m_rootNBT.all(nid);
			}

            template<typename NIDorBID>
            auto get(NIDorBID id) const
            {
                if constexpr(NIDorBID::id() == core::NID::id())
					return m_rootNBT.get(id);
                else
                {
                    static_assert( (NIDorBID::id() == core::BID::id()), "[ERROR] Invalid ID Type");
                    return m_rootBBT.get(id);
                }
			}

            bool verify()
            {
                m_rootBBT.verify();
                m_rootNBT.verify();

                for (size_t i = 0; i < m_rootNBT.size(); i++)
                {
                    const BTPage& page = m_rootNBT.at(i);
                    page.verify();

                    //if (page.hasNBTEntries())
                    //{
                    //    for (size_t j = 0; j < page.rgentries.size(); j++)
                    //    {
                    //        const NBTEntry& b = page.rgentries.at(j).asNBTEntry();
                    //        int matches = m_rootBBT.count(b.bidData);
                    //        //ASSERT((matches == 1), "[ERROR] BID %i != 1", matches);
                    //        LOGIF((matches == 0), "page: %i entry: %i matches: %i", i, j, matches);
                    //    }
                    //}
                }
                    
                ASSERT((m_rootNBT.count(core::NID_MESSAGE_STORE) == 1),
                    "[ERROR] Cannot be more than 1 Message Store");
                ASSERT((m_rootNBT.count(core::NID_NAME_TO_ID_MAP) == 1),
                    "[ERROR]");
                ASSERT((m_rootNBT.count(core::NID_NORMAL_FOLDER_TEMPLATE) <= 1),
                    "[ERROR]");
                ASSERT((m_rootNBT.count(core::NID_SEARCH_FOLDER_TEMPLATE) <= 1),
                    "[ERROR]");
                ASSERT((m_rootNBT.count(core::NID_ROOT_FOLDER) == 1),
                    "[ERROR]");
                ASSERT((m_rootNBT.count(core::NID_SEARCH_MANAGEMENT_QUEUE) <= 1),
                    "[ERROR]");
                LOG("Found only 1 Message Store and Root Folder");

                return true;
            }

            DataTree InitDataTree(core::BREF blockBref, int64_t sizeofBlockData) const
            {
                return DataTree(
                    core::Ref<std::ifstream>(m_file), 
                    [this](const core::BID& bid) { return this->get(bid); },
                    blockBref, 
                    sizeofBlockData
                );
            }

            SubNodeBTree InitSubNodeBTree(core::BID bid) const
            {



                return SubNodeBTree(
                    bid,
                    core::Ref<std::ifstream>(m_file),
                    [this](const core::BID& bid) { return this->get(bid); }
                );
            }


            /**
             * @brief This function only deals with Data Blocks (i.e Data, XBlock, or XXBlock). Not with subnodes
             * @param blockBref
             * @param blockSize
             * @return
            */

        private:

            void _init()
            {
                _buildBTree(m_rootNBT);
                _buildBTree(m_rootBBT);
                verify();
            }

            template<typename LeafEntryType>
            void _buildBTree(BTree<LeafEntryType>& btree)
            {
                types::PType rootPagePType = btree.ptype();

                size_t idxProcessing = 0;
                int maxIter = 1000;
                while (idxProcessing < btree.size() && --maxIter > 0)
                {
                    const BTPage& currentPage = btree.at(idxProcessing);
                    if (currentPage.hasBTEntries())
                    {
                        for(size_t i = 0; i < currentPage.rgentries.size(); i++)
                        {
                            const BTEntry& btentry = currentPage.rgentries.at(i).asBTEntry();
                            // TODO: Should NOT be modifying the vector while iterating over it.
                            // Because when the vector has to be resized the iterator will be invalidated.
                            // I used reserve() to fix this for now.
                            btree.addSubPage(_readBTPage(btentry.bref, rootPagePType, currentPage.cLevel));
                        }
                    }
                    idxProcessing++;
                }
                LOG("[INFO] %s has %i subpages", 
                    utils::PTypeToString(rootPagePType).c_str(), btree.size());
            }

            BTPage _readBTPage(core::BREF btpageBref, types::PType treeType, int64_t parentCLevel = -1)
            {
                // from the begning of the file move to .ib
                m_file.seekg(btpageBref.ib, std::ios::beg);
                return BTPage::readBTPage(utils::readBytes(m_file, 512), treeType, parentCLevel, &btpageBref);
            }

        private:
            std::ifstream& m_file;
            core::Header m_header{};
            BTree<NBTEntry> m_rootNBT;
            BTree<BBTEntry> m_rootBBT;
        };

	}
}


#endif // READER_NDB_H