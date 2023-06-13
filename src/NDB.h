#include <iostream>
#include <stdarg.h>
#include <cassert>
#include <vector>
#include <string>
#include <fstream>

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
            /*
            * cb (2 bytes): The amount of data, in bytes, contained within the data section of the block.
            *   This value does not include the block trailer or any unused bytes that can exist after the end
            *   of the data and before the start of the block trailer.
            *
            * wSig (2 bytes): Block signature. See section 5.5
            *   for the algorithm to calculate the block signature.
            *
            * dwCRC (4 bytes): 32-bit CRC of the cb bytes of raw data, see section 5.3 for the algorithm to calculate the CRC.
            *   Note the locations of the dwCRC and bid are differs between the Unicode and ANSI version of this structure.
            *
            * bid (Unicode: 8 bytes; ANSI 4 bytes): The BID (section 2.2.2.2) of the data block.
            */

            int64_t cb{};
            int64_t wSig{};
            int64_t dwCRC{};
            core::BID bid{};
        };

        class Entry {};


        class BTEntry : public Entry
        {
        public:
            enum class IDType {
                INVALID,
                BID,
                NID
            };
            /*
            * btkey (Unicode: 8 bytes; ANSI: 4 bytes): The key value associated with this BTENTRY.
            *   All the entries in the child BTPAGE referenced by BREF have key values greater than
            *   or equal to this key value. The btkey is either an NID (zero extended to 8 bytes for Unicode PSTs)
            *   or a BID, depending on the ptype of the page.
            */
            //std::unique_ptr<ID> btKey{ nullptr }; // NID or BID

            core::BID bid{}; // Only NID or BID is Valid
            core::NID nid{};
            IDType idType{ IDType::INVALID };

            /*
            * BREF (Unicode: 16 bytes; ANSI: 8 bytes): BREF structure (section 2.2.2.4) that points to the child BTPAGE.
            */
            core::BREF bref{};
        };

        class BBTEntry : public Entry
        {
        public:
            /*
            * BREF (Unicode: 16 bytes; ANSI: 8 bytes): BREF structure (section 2.2.2.4) that contains the BID and
            *   IB of the block that the BBTENTRY references.
            *
            * cb (2 bytes): The count of bytes of the raw data contained
            *   in the block referenced by BREF excluding the block trailer and alignment padding, if any.
            *
            * cRef (2 bytes): Reference count indicating the count of references to this block. See section 2.2.2.7.7.3.1 regarding
            *   how reference counts work.
            *
            * dwPadding (Unicode file format only, 4 bytes): Padding; MUST be set to zero.
            */

            core::BREF bref{};
            std::uint16_t cb{};
            std::uint16_t cRef{};
            std::uint32_t dwPadding{};
        };

        class NBTEntry : public Entry
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
        };

        class BTPage
        {
        public:
            /**
             * rgentries (Unicode: 488 bytes; ANSI: 496 bytes): Entries of the BTree array.
             *  The entries in the array depend on the value of the cLevel field. If cLevel is greater than 0,
             *  then each entry in the array is of type BTENTRY. If cLevel is 0, then each entry is either of type
             *  BBTENTRY or NBTENTRY, depending on the ptype of the page.
            */
            enum class EntryType
            {
                INVALID,
                _BTENTRY,
                B_BTENTRY,
                N_BTENTRY
            };

            std::vector<BTPage> subPages{};
            std::vector<Entry*> rgentries{};

            /*
            * cEnt (1 byte): The number of BTree entries stored in the page data.
            *
            * cEntMax (1 byte): The maximum number of entries that can fit inside the page data.
            *
            * cbEnt (1 byte): The size of each BTree entry, in bytes. Note that in some cases,
            *   cbEnt can be greater than the corresponding size of the corresponding rgentries structure
            *   because of alignment or other considerations. Implementations MUST use the size specified in cbEnt to advance to the next entry.
            *
            * cLevel (1 byte): The depth level of this page.
            *   Leaf pages have a level of zero, whereas intermediate pages have a level greater than 0.
            *   This value determines the type of the entries in rgentries, and is interpreted as unsigned.
            *
            * BTree Type          cLevel             rgentries structure      cbEnt (bytes)
            *  NBT                  0                  NBTENTRY              ANSI: 16, Unicode: 32
            *  NBT            Greater than 0           BTENTRY               ANSI: 12, Unicode: 24
            */
            std::int64_t cEnt{};
            std::int64_t cEntMax{};
            std::int64_t cbEnt{};
            std::int64_t cLevel{};

            /*
            * dwPadding (Unicode: 4 bytes): Padding; MUST be set to zero.
            *   Note there is no padding in the ANSI version of this structure.
            *
            * pageTrailer (Unicode: 16 bytes; ANSI: 12 bytes):
            *   A PAGETRAILER structure (section 2.2.2.7.1). The ptype subfield of pageTrailer MUST
            *   be set to ptypeBBT for a Block BTree page, or ptypeNBT for a Node BTree page. The other subfields
            *   of pageTrailer MUST be set as specified in section 2.2.2.7.1.
            */
            std::int64_t dwPadding{};
            PageTrailer pageTrailer{};

        public:
            const EntryType getEntryType() const
            {
                ASSERT((pageTrailer.ptype != types::PType::INVALID), "[ERROR] Pagetrailer was not setup properly. types::PType is set to INVALID .");
                auto ptype = pageTrailer.ptype;
                if (cLevel == 0 && ptype == types::PType::NBT) // entries are NBTENTRY 
                {
                    return EntryType::N_BTENTRY;
                }

                else if ((cLevel > 0 && ptype == types::PType::BBT) || (ptype == types::PType::NBT && cLevel > 0)) // entries are BTENTRY 
                {
                    return EntryType::_BTENTRY;
                }

                else if (cLevel == 0 && ptype == types::PType::BBT) // BBTENTRY (Leaf BBT Entry) 
                {
                    return EntryType::B_BTENTRY;
                }

                ASSERT(false, "[ERROR] Invalid types::PType for BTPage %i with cLevel %i", (int64_t)ptype, cLevel);
                return EntryType::INVALID;
            }

            bool isLeafPage() const
            {
                return getEntryType() == EntryType::B_BTENTRY || getEntryType() == EntryType::N_BTENTRY;
            }

            bool verify()
            {
                types::PType ptype = pageTrailer.ptype;
                for (const BTPage& page : subPages)
                {
                    ASSERT((page.pageTrailer.ptype == ptype), "[ERROR] Subpage has different ptype than parent page.");
                    ASSERT((page.cEnt == page.rgentries.size()), "[ERROR] Subpage has different number of entries than cEnt.");
                    ASSERT((page.getEntryType() != EntryType::INVALID), "[ERROR] Subpage has invalid entry type.");
                }
                return true;
            }
        };

        /*
        * @param = data (Variable): Raw data.
        *
        * @param = padding (Variable, Optional): Reserved.
        *
        * @param = cb (2 bytes):
        *   The amount of data, in bytes, contained within the data section of the block.
        *   This value does not include the block trailer or any unused bytes that can exist after the
        *   end of the data and before the start of the block trailer.
        *
        * @param = wSig (2 bytes): Block signature. See section 5.5 for the algorithm to calculate the block signature.
        *
        * @param = dwCRC (4 bytes): 32-bit CRC of the cb bytes of raw data, see section 5.3 for the algorithm to calculate
        *   the CRC
        *
        * @param = Bid (8 bytes): The BID (section 2.2.2.2) of the data block.
        */
        class Block {};

        class DataBlock : public Block
        {
        public:
            std::vector<types::byte_t> data{};
            int32_t padding{};
            BlockTrailer trailer{};
        };


        /*
        * @brief: XBLOCKs are used when the data associated with a node data that exceeds 8,176 bytes in size.
            The XBLOCK expands the data that is associated with a node by using an array of BIDs that reference data
            blocks that contain the data stream associated with the node. A BLOCKTRAILER is present at the end of an XBLOCK,
            and the end of the BLOCKTRAILER MUST be aligned on a 64-byte boundary.

        * @param = btype (1 byte): Block type; MUST be set to 0x01 to indicate an XBLOCK or XXBLOCK.
        *
        * @param = cLevel (1 byte): MUST be set to 0x01 to indicate an XBLOCK.
        *
        * @param = cEnt (2 bytes): The count of BID entries in the XBLOCK.
        *
        * @param = lcbTotal (4 bytes): Total count of bytes of all the external data stored in the data blocks referenced by XBLOCK.
        *
        * @param = rgbid (variable): Array of BIDs that reference data blocks. The size is equal to the number of
        *   entries indicated by cEnt multiplied by the size of a BID (8 bytes for Unicode PST files, 4 bytes for ANSI PST files).
        *
        * @param = rgbPadding (variable, optional): This field is present if the total size of all of the other fields is not a multiple of 64.
        *   The size of this field is the smallest number of bytes required to make the size of the XBLOCK a multiple of 64.
        *   Implementations MUST ignore this field.
        *
        * @param = blockTrailer (ANSI: 12 bytes; Unicode: 16 bytes): A BLOCKTRAILER structure (section 2.2.2.8.1).
        *
        */
        class XBlock : public Block
        {
        public:
            int32_t btype{};
            int32_t cLevel{};
            int32_t cEnt{};
            int64_t lcbTotal{};
            std::vector<core::BID> rgbid{};
            int64_t rgbPadding{};
            BlockTrailer trailer{};
        };

        /**
        * @brief The XXBLOCK further expands the data that is associated with a node by using an array of BIDs that reference XBLOCKs.
            A BLOCKTRAILER is present at the end of an XXBLOCK, and the end of the BLOCKTRAILER MUST be aligned on a 64-byte boundary.

        * @param = btype (1 byte): Block type; MUST be set to 0x01 to indicate an XBLOCK or XXBLOCK.
        *
        * @param = cLevel (1 byte): MUST be set to 0x02 to indicate an XXBLOCK.
        *
        * @param = cEnt (2 bytes): The count of BID entries in the XXBLOCK.
        *
        * @param = lcbTotal (4 bytes): Total count of bytes of all the external data stored in XBLOCKs under this XXBLOCK.
        *
        * @param = rgbid (variable): Array of BIDs that reference XBLOCKs. The size is equal to the number of entries indicated by cEnt
            multiplied by the size of a BID (8 bytes for Unicode PST files, 4 bytes for ANSI PST Files).
        *
        * @param = rgbPadding (variable, optional): This field is present if the total size of all of the other fields is not a multiple of 64.
        *   The size of this field is the smallest number of bytes required to make the size of the XXBLOCK a multiple of 64.
        *   Implementations MUST ignore this field.
        *
        * @param = blockTrailer (ANSI: 12 bytes; Unicode: 16 bytes): A BLOCKTRAILER structure (section 2.2.2.8.1).
        */
        class XXBlock : public Block
        {
        public:
            int32_t btype{};
            int32_t cLevel{};
            int32_t cEnt{};
            int64_t lcbTotal{};
            std::vector<core::BID> rgbid{};
            int64_t rgbPadding{};
            BlockTrailer trailer{};
        };

        class NDB
        {
        public:
            NDB(
                std::ifstream& file,
                core::BREF rootNBTFilePos,
                core::BREF rootBBTFilePos)
                :
                m_file(file),
                m_rootNBTFilePos(rootNBTFilePos),
                m_rootBBTFilePos(rootBBTFilePos)
            {
                init();
            }

        private:

            void init()
            {
			    m_rootNBT = _readBTPage(m_rootNBTFilePos, types::PType::NBT);
				m_rootBBT = _readBTPage(m_rootBBTFilePos, types::PType::BBT);

                _buildBTPage(m_rootNBT);
                _buildBTPage(m_rootBBT);

                m_rootNBT.verify();
                m_rootBBT.verify();
            }

            void _buildBTPage(BTPage& rootPage)
            {
                types::PType pagePtype = rootPage.pageTrailer.ptype;
                for (const Entry* entry : rootPage.rgentries)
                {
                    auto etype = rootPage.getEntryType();
                    if (etype == BTPage::EntryType::_BTENTRY)
                    {
                        const BTEntry* btentry = static_cast<const BTEntry*>(entry);
                        rootPage.subPages.push_back(_readBTPage(btentry->bref, pagePtype));
                    }
                }
                LOG("[INFO] BTPage has %i subpages", rootPage.subPages.size());

                int idxProcessing = 0;
                int maxIter = 1000;
                while (idxProcessing < rootPage.subPages.size() && maxIter > 0)
                {
                    const BTPage& currentPage = rootPage.subPages.at(idxProcessing);
                    auto etype = currentPage.getEntryType();
                    for (const Entry* entry : currentPage.rgentries)
                    {
                        if (etype == BTPage::EntryType::_BTENTRY)
                        {
                            const BTEntry* btentry = static_cast<const BTEntry*>(entry);
                            rootPage.subPages.push_back(_readBTPage(btentry->bref, pagePtype));
                        }
                        maxIter--;
                        idxProcessing++;
                    }
                }
            }


            NBTEntry _readNBTEntry(const std::vector<types::byte_t>& bytes)
            {
                /*
                * nid (Unicode: 8 bytes; ANSI: 4 bytes):
                *     The NID (section 2.2.2.1) of the entry. Note that the NID is a 4-byte value for both Unicode and ANSI formats.
                *     However, to stay consistent with the size of the btkey member in BTENTRY, the 4-byte NID is extended to its 8-byte
                *     equivalent for Unicode PST files.
                *
                * bidData (Unicode: 8 bytes; ANSI: 4 bytes): The BID of the data block for this node.
                *
                * bidSub (Unicode: 8 bytes; ANSI: 4 bytes): The BID of the subnode block for this node.
                *     If this value is zero, a subnode block does not exist for this node.
                *
                * nidParent (4 bytes):
                *     If this node represents a child of a Folder object defined in the Messaging Layer, then this value is
                *     nonzero and contains the NID of the parent Folder object's node. Otherwise, this value is zero.
                *     See section 2.2.2.7.7.4.1 for more information. This field is not interpreted by any structure defined at the NDB Layer.
                *
                * dwPadding (Unicode file format only, 4 bytes): Padding; MUST be set to zero.
                */
                NBTEntry entry{};
                entry.nid = core::readNID(utils::slice(bytes, 0, 4, 4)); // NID is zero padded from 4 to 8
                entry.bidData = core::readBID(utils::slice(bytes, 8, 16, 8), false);
                entry.bidSub = core::readBID(utils::slice(bytes, 16, 24, 8), false);
                entry.nidParent = core::readNID(utils::slice(bytes, 24, 28, 4));
                entry.dwPadding = utils::slice(bytes, 28, 32, 4, utils::toT_l<int64_t>);

                LOG("[INFO] Created NBTEntry with [nid] of %s", utils::NIDTypeToString(entry.nid.nidType).c_str());
                return entry;
            }

            BTEntry _readBTEntry(const std::vector<types::byte_t>& bytes, types::PType pagePType)
            {
                /*
                *
                * btkey (Unicode: 8 bytes; ANSI: 4 bytes): The key value associated with this BTENTRY.
                *   All the entries in the child BTPAGE referenced by BREF have key values greater than or equal to this key value.
                *   The btkey is either an NID (zero extended to 8 bytes for Unicode PSTs) or a BID, depending on the ptype of the page.
                *
                * BREF (Unicode: 16 bytes; ANSI: 8 bytes): BREF structure (section 2.2.2.4) that points to the child BTPAGE.
                */

                ASSERT((bytes.size() == 24), "[ERROR] BTEntry must be 24 bytes not %i", bytes.size());

                BTEntry entry{};
                if (pagePType == types::PType::NBT)
                {
                    entry.nid = core::readNID(utils::slice(bytes, 0, 4, 4)); // NID is zero padded from 4 to 8
                    entry.idType = BTEntry::IDType::NID;
                    entry.bref = core::readBREF(utils::slice(bytes, 8, 24, 16), false);
                }

                else if (pagePType == types::PType::BBT)
                {
                    entry.bid = core::readBID(utils::slice(bytes, 0, 8, 8), true);
                    entry.idType = BTEntry::IDType::BID;
                    entry.bref = core::readBREF(utils::slice(bytes, 8, 24, 16), true);
                }

                else
                {
                    ASSERT(false, "[ERROR] Unknown Page [types::PType] %s for BTEntry", utils::PTypeToString(pagePType));
                }
                LOG("[INFO] Created BTEntry with bid %i and BREF bid %i", entry.bid.bidIndex, entry.bref.bid.bidIndex);
                return entry;
            }

            BBTEntry _readBBTEntry(const std::vector<types::byte_t>& bytes)
            {
                /**
                 * BREF (Unicode: 16 bytes; ANSI: 8 bytes):
                    BREF structure (section 2.2.2.4) that contains the BID and IB of the block that the
                    BBTENTRY references.

                  cb (2 bytes): The count of bytes of the raw data contained in the block referenced by
                    BREF excluding the block trailer and alignment padding, if any.

                  cRef (2 bytes): Reference count indicating the count of references to this block. See section 2.2.2.7.7.3.1
                    regarding how reference counts work.

                  dwPadding (Unicode file format only, 4 bytes): Padding; MUST be set to zero.
                */
                ASSERT((bytes.size() == 24), "[ERROR] BBTEntry must be 24 bytes not %i", bytes.size());
                BBTEntry entry{};
                entry.bref = core::readBREF(utils::slice(bytes, 0, 16, 16), false);
                entry.cb = utils::slice(bytes, 16, 18, 2, utils::toT_l<uint16_t>);
                entry.cRef = utils::slice(bytes, 18, 20, 2, utils::toT_l<uint16_t>);
                entry.dwPadding = utils::slice(bytes, 20, 24, 4, utils::toT_l<uint32_t>);

                ASSERT((entry.dwPadding == 0), "[ERROR] dwPadding must be 0 for BBTEntry not %i", entry.dwPadding);
                return entry;
            }

            PageTrailer _readPageTrailer(const std::vector<types::byte_t>& pageTrailerBytes)
            {
                /**
                 * ptype (1 byte): This value indicates the type of data contained within the page. This field MUST contain one of the following values.
                 *
                 * Value        Friendly name       Meaning                     wSig value
                 * 0x80         ptypeBBT            Block BTree page.           Block or page signature (section 5.5).
                 * 0x81         ptypeNBT            Node BTree page.            Block or page signature (section 5.5).
                 * 0x82         ptypeFMap           Free Map page.              0x0000
                 * 0x83         ptypePMap           Allocation Page Map page.   0x0000
                 * 0x84         ptypeAMap           Allocation Map page.        0x0000
                 * 0x85         ptypeFPMap          Free Page Map page.         0x0000
                 * 0x86         ptypeDL             Density List page.          Block or page signature (section 5.5).
                */
                std::uint8_t ptypeByte = utils::slice(pageTrailerBytes, 0, 1, 1, utils::toT_l<std::uint8_t>);
                types::PType ptype = utils::getPType(ptypeByte);

                /*
                * ptypeRepeat (1 byte): MUST be set to the same value as ptype.
                */
                std::uint8_t ptypeRepeatByte = utils::slice(pageTrailerBytes, 1, 2, 1, utils::toT_l<std::uint8_t>);
                types::PType ptypeRepeat = utils::getPType(ptypeRepeatByte);

                ASSERT((ptypeByte == ptypeRepeatByte), "Ptype [%i] and PtypeRepeat [%i]", ptypeByte, ptypeRepeatByte);

                /*
                * wSig (2 bytes): Page signature. This value depends on the value of the ptype field.
                *   This value is zero (0x0000) for AMap, PMap, FMap, and FPMap pages. For BBT, NBT,
                *   and DList pages, a page / block signature is computed (see section 5.5).
                */
                std::uint16_t wSig = utils::slice(pageTrailerBytes, 2, 4, 2, utils::toT_l<std::uint16_t>);

                /*
                * dwCRC (4 bytes): 32-bit CRC of the page data, excluding the page trailer.
                *   See section 5.3 for the CRC algorithm. Note the locations of the dwCRC and
                *   bid are differs between the Unicode and ANSI version of this structure.
                */
                std::uint32_t dwCRC = utils::slice(pageTrailerBytes, 4, 8, 4, utils::toT_l<std::uint32_t>);

                /*
                * bid (Unicode: 8 bytes; ANSI 4 bytes): The BID of the page's block. AMap, PMap,
                *   FMap, and FPMap pages have a special convention where their BID is assigned the
                *   same value as their IB (that is, the absolute file offset of the page). The bidIndex
                *   for other page types are allocated from the special bidNextP counter in the HEADER structure.
                */
                core::BID bid = core::readBID(utils::slice(pageTrailerBytes, 8, 16, 8), false);

                PageTrailer pt{};
                pt.ptype = ptype;
                pt.ptypeRepeat = ptypeRepeat;
                pt.wSig = wSig;
                pt.dwCRC = dwCRC;
                pt.bid = bid;

                LOG("[INFO] Create Page Trailer w [ptype] %s", utils::PTypeToString(ptype).c_str());

                return pt;
            }

                BTPage _readBTPage(core::BREF bTreeBref, types::PType treeType)
                {
                    // from the begging of the file move to .ib
                    m_file.seekg(bTreeBref.ib, std::ios::beg);
                    /*
                    * Entries of the BTree array. The entries in the array depend on the value of the cLevel field.
                    *   If cLevel is greater than 0, then each entry in the array is of type BTENTRY. If cLevel is 0,
                    *   then each entry is either of type BBTENTRY or NBTENTRY, depending on the ptype of the page.
                    */
                    std::vector<types::byte_t> rgentries = utils::readBytes(m_file, 488);
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
                    auto cEnt = utils::readBytes(m_file, 1, utils::toT_l<decltype(BTPage::cbEnt)>);
                    auto cEntMax = utils::readBytes(m_file, 1, utils::toT_l<decltype(BTPage::cEntMax)>);
                    auto cbEnt = utils::readBytes(m_file, 1, utils::toT_l<decltype(BTPage::cbEnt)>);
                    auto cLevel = utils::readBytes(m_file, 1, utils::toT_l<decltype(BTPage::cLevel)>);

                    /*
                    * dwPadding (Unicode: 4 bytes): Padding; MUST be set to zero. Note there is no padding in the ANSI version of this structure.
                    */
                    auto dwPadding = utils::readBytes(m_file, 4, utils::toT_l<decltype(BTPage::dwPadding)>);

                    LOG("%i %i", cbEnt, cLevel);

                    /**
                     * pageTrailer (Unicode: 16 bytes; ANSI: 12 bytes): A PAGETRAILER structure (section 2.2.2.7.1). The ptype
                     *  subfield of pageTrailer MUST be set to ptypeBBT for a Block BTree page, or ptypeNBT for a Node BTree page.
                     *  The other subfields of pageTrailer MUST be set as specified in section 2.2.2.7.1.
                    */
                    std::vector<types::byte_t> pageTrailerBytes = utils::readBytes(m_file, 16);
                    PageTrailer pageTrailer = _readPageTrailer(pageTrailerBytes);

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
                    page.rgentries = _readEntries(rgentries, pageTrailer.ptype, cEnt, cbEnt, cLevel);
                    return page;
                }

                std::vector<Entry*> _readEntries(
                    const std::vector<types::byte_t>& entriesInBytes,
                    types::PType treePtype,
                    int64_t numEntries,
                    int64_t singleEntrySize,
                    int64_t cLevel
                )
                {
                    std::vector<Entry*> entries{};
                    for (int64_t i = 0; i < numEntries; i++)
                    {
                        int64_t start = i * singleEntrySize;
                        int64_t end = (i + 1) * singleEntrySize;
                        int64_t size = end - start;
                        ASSERT((size == singleEntrySize), "[ERROR] Size should be %i NOT %i", singleEntrySize, size);
                        std::vector<types::byte_t> entryBytes = utils::slice(entriesInBytes, start, end, size);

                        /**
                        * Entries of the BTree array. The entries in the array depend on the value of the cLevel field.
                        *   If cLevel is greater than 0, then each entry in the array is of type BTENTRY. If cLevel is 0,
                        *   then each entry is either of type BBTENTRY or NBTENTRY, depending on the ptype of the page.
                        */

                        if (cLevel == 0 && treePtype == types::PType::NBT) // entries are NBTENTRY 
                        {
                            NBTEntry* entry = new NBTEntry(_readNBTEntry(entryBytes));
                            entries.push_back(static_cast<Entry*>(entry));
                        }

                        else if ((cLevel > 0 && treePtype == types::PType::BBT) || (treePtype == types::PType::NBT && cLevel > 0)) // entries are BTENTRY 
                        {
                            BTEntry* entry = new BTEntry(_readBTEntry(entryBytes, treePtype));
                            entries.push_back(static_cast<Entry*>(entry));
                        }

                        else if (cLevel == 0 && treePtype == types::PType::BBT) // BBTENTRY (Leaf BBT Entry) 
                        {
                            BBTEntry* entry = new BBTEntry(_readBBTEntry(entryBytes));
                            entries.push_back(static_cast<Entry*>(entry));
                        }

                        else
                        {
                            ASSERT(false, "[ERROR] Invalid [cLevel] %i and [treePType] %i combination at iteration %i", cLevel, utils::PTypeToString(treePtype), i);
                        }
                    }
                    return entries;
                }

                BlockTrailer _readBlockTrailer(const std::vector<types::byte_t>& bytes)
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
                    trailer.bid = core::readBID(utils::slice(bytes, 8, 16, 8), true);
                    return trailer;
                }

                std::unique_ptr<Block> _readBlock(core::BREF blockBref, int blockSize, bool isDataTree)
                {
                    /*
                    * data (236 bytes): Raw data.
                    *
                    * padding (4 bytes): Reserved.
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
                    * Block type            Data structure    Internal BID?    Header level    Array content
                    *
                      Data Tree             Data block            No               N/A             Bytes
                                            XBLOCK                Yes              1               Data block reference
                                            XXBLOCK                                2               XBLOCK reference
                      Subnode BTree data    SLBLOCK                                0               SLENTRY
                                            SIBLOCK                                1               SIENTRY

                    */

                    m_file.seekg(blockBref.ib, std::ios::beg);

                    std::vector<types::byte_t> blockBytes = utils::readBytes(m_file, blockSize);
                    BlockTrailer trailer = _readBlockTrailer(utils::slice(blockBytes, blockBytes.size() - 16, blockBytes.size(), (size_t)16));

                    if (!trailer.bid.isInternal() && isDataTree) // Data Block
                    {
                        return std::make_unique<DataBlock>(_readDataBlock(blockBytes, trailer));
                    }

                    else if (trailer.bid.isInternal() && !isDataTree) // XBlock or XXBlock
                    {
                        int32_t btype = utils::slice(blockBytes, 0, 1, 1, utils::toT_l<int32_t>);
                        if (btype == 0x01) // XBlock
                        {
                            return std::make_unique<XBlock>(_readXBlock(blockBytes, trailer));
                        }

                        else if (btype == 0x02) // XXBlock
                        {
                            return std::make_unique<XXBlock>(_readXXBlock(blockBytes, trailer));
                        }
                        ASSERT(false, "[ERROR] Invalid btype must 0x01 or 0x02 not %i", btype);
                    }

                    ASSERT(false, "[ERROR] Unknown block type");
                    return nullptr;
                }

                DataBlock _readDataBlock(const std::vector<types::byte_t>& blockBytes, BlockTrailer trailer)
                {
                    DataBlock block{};
                    block.data = utils::slice(blockBytes, (int64_t)0, trailer.cb, trailer.cb);
                    block.trailer = trailer;
                    return block;
                }

                XBlock _readXBlock(const std::vector<types::byte_t>& blockBytes, BlockTrailer trailer)
                {
                    XBlock block{};
                    block.btype = utils::slice(blockBytes, 0, 1, 1, utils::toT_l<int32_t>);
                    block.cLevel = utils::slice(blockBytes, 1, 2, 1, utils::toT_l<int32_t>);
                    block.cEnt = utils::slice(blockBytes, 2, 4, 2, utils::toT_l<int32_t>);
                    block.lcbTotal = utils::slice(blockBytes, 4, 8, 4, utils::toT_l<int64_t>);
                    block.trailer = trailer;

                    ASSERT((block.btype == 0x01), "[ERROR] btype for XBlock should be 0x01 not %i", block.btype);
                    ASSERT((block.cLevel == 0x01), "[ERROR] cLevel for XBlock should be 0x01 not %i", block.cLevel);

                    int32_t bidSize = 8;
                    // from the end of lcbTotal to the end of the data field
                    std::vector<types::byte_t> bidBytes = utils::slice(blockBytes, (int64_t)8, trailer.cb + (int64_t)8, trailer.cb);
                    for (int32_t i = 0; i < block.cEnt; i++)
                    {
                        int32_t start = i * bidSize;
                        int32_t end = ((int64_t)i + 1) * bidSize;
                        ASSERT((end - start == bidSize), "[ERROR] BID must be of size 8 not %i", (end - start));
                        block.rgbid.push_back(core::readBID(utils::slice(bidBytes, start, end, bidSize), true));
                    }
                    return block;
                }

                XXBlock _readXXBlock(const std::vector<types::byte_t>& blockBytes, BlockTrailer trailer)
                {
                    XXBlock block{};
                    block.btype = utils::slice(blockBytes, 0, 1, 1, utils::toT_l<int32_t>);
                    block.cLevel = utils::slice(blockBytes, 1, 2, 1, utils::toT_l<int32_t>);
                    block.cEnt = utils::slice(blockBytes, 2, 4, 2, utils::toT_l<int32_t>);
                    block.lcbTotal = utils::slice(blockBytes, 4, 8, 4, utils::toT_l<int64_t>);
                    block.trailer = trailer;

                    ASSERT((block.btype == 0x01), "[ERROR] btype for XXBlock should be 0x01 not %i", block.btype);
                    ASSERT((block.cLevel == 0x02), "[ERROR] cLevel for XXBlock should be 0x02 not %i", block.cLevel);

                    int32_t bidSize = 8;
                    // from the end of lcbTotal to the end of the data field
                    std::vector<types::byte_t> bidBytes = utils::slice(blockBytes, (int64_t)8, trailer.cb + (int64_t)8, trailer.cb);
                    for (int32_t i = 0; i < block.cEnt; i++)
                    {
                        int32_t start = i * bidSize;
                        int32_t end = ((int64_t)i + 1) * bidSize;
                        ASSERT((end - start == bidSize), "[ERROR] BID must be of size 8 not %i", (end - start));
                        block.rgbid.push_back(core::readBID(utils::slice(bidBytes, start, end, bidSize), true));
                    }
                    return block;
                }


        private:
            std::ifstream& m_file;
            core::BREF m_rootNBTFilePos{};
            core::BREF m_rootBBTFilePos{};
            BTPage m_rootNBT{};
            BTPage m_rootBBT{};
        };

	}
}


#endif // READER_NDB_H