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
#include <format>

#include "types.h"
#include "utils.h"
#include "core.h"

#ifndef READER_NDB_H
#define READER_NDB_H

namespace reader::ndb {

    struct PageTrailer
    {
        /// (1 byte): This value indicates the type of data contained within the page.
        types::PType ptype{types::PType::INVALID};
        /// (1 byte): MUST be set to the same value as ptype.
        types::PType ptypeRepeat{};
        /// (2 bytes): Page signature. This value depends on the value of the ptype field.
        /// This value is zero(0x0000) for AMap, PMap, FMap, and FPMap pages.For BBT, NBT,
        /// and DList pages, a page / block signature is computed(see section 5.5).
        std::uint16_t wSig{};
        /// (4 bytes): 32-bit CRC of the page data, excluding the page trailer.
        std::uint32_t dwCRC{};
        /// (Unicode: 8 bytes; ANSI 4 bytes): The BID of the page's block. AMap, PMap,
        /// FMap, and FPMap pages have a special convention where their BID is assigned the
        /// same value as their IB(that is, the absolute file offset of the page).The bidIndex
        /// for other page types are allocated from the special bidNextP counter in the HEADER structure.
        core::BID bid{};

        explicit PageTrailer(const std::vector<types::byte_t>& bytes, core::BREF bref)
            : PageTrailer(bytes)
        {
            auto computedSig = utils::ms::ComputeSig(bref.ib, bref.bid.getBidRaw());
            if (ptype == types::PType::NBT || ptype == types::PType::BBT)
            {
                ASSERT((wSig == computedSig), "[ERROR] Page Sig [%i] != Computed Sig [%i]", wSig, computedSig);
            }
        }

        explicit PageTrailer(const std::vector<types::byte_t>& bytes)
        {
            utils::ByteView view(bytes);
            ptype = utils::getPType(view.read<uint8_t>(1));
            ptypeRepeat = utils::getPType(view.read<uint8_t>(1));
            wSig = view.read<uint16_t>(2);
            dwCRC = view.read<uint32_t>(4);
            bid = view.entry<core::BID>(8);
            ASSERT((ptype == ptypeRepeat), "[ERROR]");
        }
    };

    struct BlockTrailer
    {
        /// (2 bytes): The amount of data, in bytes, contained within the data section of the block.
        /// This value does not include the block trailer or any unused bytes that can exist after the end
        /// of the data and before the start of the block trailer.
        uint16_t cb{};
        /// (2 bytes): Block signature. See section 5.5
        /// *for the algorithm to calculate the block signature.
        uint16_t wSig{};
        /// (4 bytes): 32-bit CRC of the cb bytes of raw data, see section 5.3 for the algorithm to calculate the CRC.
        uint32_t dwCRC{};
        /// (Unicode: 8 bytes; ANSI 4 bytes): The BID (section 2.2.2.2) of the data block.
        core::BID bid{};

        explicit BlockTrailer(const std::vector<types::byte_t>& bytes, core::BREF bref)
            : BlockTrailer(bytes)
        {
            auto computedSig = utils::ms::ComputeSig(bref.ib, bref.bid.getBidRaw());
            ASSERT((wSig == computedSig),
                "[ERROR] Page Sig [%i] != Computed Sig [%i]", wSig, computedSig);
        }

        explicit BlockTrailer(const std::vector<types::byte_t>& bytes)
        {
            ASSERT((bytes.size() == 16), "[ERROR] Block Trailer has to be 16 bytes not %i", bytes.size());
            utils::ByteView view(bytes);
            cb = view.read<uint16_t>(2);
            wSig = view.read<uint16_t>(2);
            dwCRC = view.read<uint32_t>(4);
            bid = view.entry<core::BID>(8);
        }
    };

    struct BTEntry
    {
        /// (Unicode: 8 bytes; ANSI: 4 bytes): The key value associated with this BTENTRY
        /// The btkey is either an NID (zero extended to 8 bytes for Unicode PSTs)
        /// or a BID, depending on the ptype of the page.
        std::vector<types::byte_t> btkey{};
        /// (Unicode: 16 bytes): BREF structure (section 2.2.2.4) that points to the child BTPAGE.
        core::BREF bref{};

        static constexpr size_t size = 24;
        static constexpr size_t id() { return 1; }
    };

   struct BBTEntry
    {
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
        
        static constexpr size_t size = 24;
        static constexpr size_t id() { return 2; }
    };

    struct NBTEntry
    {
        /// (Unicode: 8 bytes; ANSI: 4 bytes):
        /// The NID of the entry.
        /// To stay consistent with the size of the btkey member in BTENTRY,
        /// the 4 - byte NID is extended to its 8 - byte equivalent for Unicode PST files.
        core::NID nid{};
        /// (4 bytes): If this node
        /// represents a child of a Folder object defined in the Messaging Layer, then this value
        /// is nonzero and contains the NID of the parent Folder object's node. Otherwise,
        /// this value is zero.
        core::NID nidParent{};
        /// (Unicode: 8 bytes; ANSI: 4 bytes):
        /// The BID of the data block for this node.
        core::BID bidData{};
        /// (Unicode: 8 bytes; ANSI: 4 bytes):
        /// The BID of the subnode block for this node.If this value is zero, a
        /// subnode block does not exist for this node.
        core::BID bidSub{};
        /// (Unicode file format only, 4 bytes): Padding; MUST be set to zero.
        uint32_t dwPadding{};

        static constexpr size_t size = 32;
        bool hasSubNode() const { return bidSub.getBidIndex() != 0; }
        static constexpr int64_t id() { return 3; }
    };

    class Entry 
    {
    public:
        explicit Entry(const std::vector<types::byte_t>& data) : m_data(data) {}

        [[nodiscard]] BTEntry asBTEntry() const
        {
            ASSERT((m_data.size() == BTEntry::size), "[ERROR] Incorrect size for BTEntry");
            utils::ByteView view(m_data);
            BTEntry entry{};
            entry.btkey = view.read(8);
            entry.bref = view.entry<core::BREF>(16);
            return entry;
        }

        [[nodiscard]] BBTEntry asBBTEntry() const
        {
            ASSERT((m_data.size() == BBTEntry::size), "[ERROR] BBTEntry must be 24 bytes not %i", m_data.size());
            utils::ByteView view(m_data);
            BBTEntry entry{};
            entry.bref = view.entry<core::BREF>(16); 
            entry.cb = view.read<uint16_t>(2);
            entry.cRef = view.read<uint16_t>(2); 
            entry.dwPadding = view.read<uint32_t>(4);

            //ASSERT((entry.dwPadding == 0),
            //    "[ERROR] dwPadding must be 0 for BBTEntry not %i", entry.dwPadding);
            return entry;
        }

        [[nodiscard]] NBTEntry asNBTEntry() const
        {
            ASSERT((m_data.size() == NBTEntry::size), "[ERROR] NBTEntry must be 32 bytes not %i", m_data.size());
            utils::ByteView view(m_data);
            NBTEntry entry{};
            entry.nid = view.entry<core::NID>(8);
            entry.bidData = view.entry<core::BID>(8);
            entry.bidSub = view.entry<core::BID>(8); 
            entry.nidParent = view.entry<core::NID>(4);
            entry.dwPadding = view.read<uint32_t>(4);
            //ASSERT((entry.dwPadding == 0),
            //   "[ERROR] dwPadding must be 0 for BBTEntry not %i", entry.dwPadding);
            return entry;
        }

        template<typename EntryType>
        [[nodiscard]] EntryType as() const
		{
            if constexpr (std::is_same_v<EntryType, BTEntry>)
            {
                return asBTEntry();
            }
            else if constexpr (std::is_same_v<EntryType, BBTEntry>)
            {
                return asBBTEntry();
            }
            else if constexpr (std::is_same_v<EntryType, NBTEntry>)
            {
                return asNBTEntry();
            }	
            else
            {
                ASSERT(false, "[ERROR] Invalid Type");
                return EntryType();
            }
		}

        [[nodiscard]] size_t size() const { return m_data.size(); }

    private:
        std::vector<types::byte_t> m_data{};
    };

    class BTPage
    {
    public:
        std::vector<BTPage> subPages{};
        /// (Unicode: 488 bytes; ANSI: 496 bytes): Entries of the BTree array.
        /// The entries in the array depend on the value of the cLevel field. If cLevel is greater than 0,
        /// then each entry in the array is of type BTENTRY.If cLevel is 0, then each entry is either of type
        /// BBTENTRY or NBTENTRY, depending on the ptype of the page.
        std::vector<Entry> rgentries{};
        /// (1 byte): The number of BTree entries stored in the page data.
        std::uint8_t nEntries{};
        /// (1 byte): The maximum number of entries that can fit inside the page data.
        std::uint8_t maxNEntries{};
        /// (1 byte): The size of each BTree entry, in bytes. Note that in some cases,
        /// cbEnt can be greater than the corresponding size of the corresponding rgentries structure
        /// because of alignment or other considerations.Implementations MUST use the size specified in cbEnt to advance to the next entry.
        std::uint8_t singleEntrySize{};
        /// (1 byte): The depth level of this page.
        /// Leaf pages have a level of zero, whereas intermediate pages have a level greater than 0.
        /// This value determines the type of the entries in rgentries, and is interpreted as unsigned.
        std::uint8_t cLevel{};
        /// dwPadding (Unicode: 4 bytes): Padding; MUST be set to zero.
        std::int64_t dwPadding{};
        /// (Unicode: 16 bytes):
        /// A PAGETRAILER structure(section 2.2.2.7.1).The ptype subfield of pageTrailer MUST
        /// be set to ptypeBBT for a Block BTree page, or ptypeNBT for a Node BTree page.The other subfields
        /// of pageTrailer MUST be set as specified in section 2.2.2.7.1.
        const PageTrailer trailer;
        /// size in bytes
        static constexpr size_t size = 512;

        static BTPage Init(const std::vector<types::byte_t>& bytes, core::BREF bref, int32_t parentCLevel = -1)
        {
            ASSERT((bytes.size() == BTPage::size), "[ERROR]");
            utils::ByteView view(bytes);
            return { bytes, PageTrailer(view.takeLast(16), bref), parentCLevel };
        }

        static BTPage Init(const std::vector<types::byte_t>& bytes, int32_t parentCLevel = -1)
        {
            ASSERT((bytes.size() == BTPage::size), "[ERROR]");
            utils::ByteView view(bytes);
            return { bytes, PageTrailer(view.takeLast(16)), parentCLevel };
        }

        BTPage(const std::vector<types::byte_t>& bytes, PageTrailer&& trailer_, int32_t parentCLevel = -1)
            : trailer(trailer_)
        {
            utils::ByteView view(bytes);
            view.skip(488); // skip entries for now

            nEntries = view.read<uint8_t>(1);
            maxNEntries = view.read<uint8_t>(1);
            singleEntrySize = view.read<uint8_t>(1);
            cLevel = view.read<uint8_t>(1);
            dwPadding = view.read<uint32_t>(4);
            view.setStart(0); // reset to beginning of bytes to read the entries
            rgentries = view.entries<Entry>(nEntries, singleEntrySize);

            ASSERT((trailer.ptype == types::PType::BBT || trailer.ptype == types::PType::NBT), "[ERROR] Invalid ptype for pagetrailer");
            ASSERT((nEntries <= maxNEntries), "[ERROR] Invalid cEnt %i", nEntries);
            ASSERT((dwPadding == 0), "[ERROR] dwPadding should be 0 not %i", dwPadding);
            ASSERT((nEntries == rgentries.size()), "[ERROR]");
            if (parentCLevel != -1)
            {
                ASSERT((parentCLevel - 1 == cLevel),
                    "[ERROR] SubBTPage cLevel [%i] must be smaller than ParentBTPage cLevel [%i]", cLevel, parentCLevel);
            }
        }

    public:

        [[nodiscard]] size_t getEntryType() const
        {
            return getEntryType(trailer.ptype, cLevel);
        }

        template<typename T>
        static size_t getEntryType(types::PType pagePtype, T cLevel)
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

            else if (cLevel == 0 && pagePtype == types::PType::BBT) // BBTENTRY
            {
                return BBTEntry::id();
            }

            ASSERT(false, "[ERROR] Invalid types::PType for BTPage %i with cLevel %i", 
                static_cast<uint32_t>(pagePtype), cLevel);
            return 0;
        }

        [[nodiscard]] bool hasBTEntries() const
		{
			return getEntryType() == BTEntry::id();
		}

        [[nodiscard]] bool hasBBTEntries() const
        {
            return getEntryType() == BBTEntry::id();
		}

        [[nodiscard]] bool hasNBTEntries() const
        {
            return getEntryType() == NBTEntry::id();
        }

        [[nodiscard]] bool isLeafPage() const
        {
            return hasNBTEntries() || hasBBTEntries();
        }

        template<typename EntryType>
        [[nodiscard]] bool has() const
        {
            if constexpr (std::is_same_v<EntryType, BTEntry>)
            {
                return hasBTEntries();
            }               
            else if constexpr (std::is_same_v<EntryType, BBTEntry>)
            {
                return hasBBTEntries();
            }                
            else if constexpr (std::is_same_v<EntryType, NBTEntry>)
            {
                return hasNBTEntries();
            }    
            else
            {
                ASSERT(false, "[ERROR] Invalid EntryType");
                return false;
            }
		}

        [[nodiscard]] bool verify() const
        {
            _verify(*this);
            for(const auto& page : subPages)
            {
                _verify(page);
            }
            return true;
        }

        private:
            bool _verify(const BTPage& page) const
            {
                types::PType ptype = trailer.ptype;
                ASSERT((page.trailer.ptype == ptype), "[ERROR] Subpage has different ptype than parent page.");
                ASSERT((page.nEntries == page.rgentries.size()), "[ERROR] Subpage has different number of entries than cEnt.");
                //ASSERT(page.getEntryType(), -1), "[ERROR] Subpage has invalid entry type.");
                for (const auto& entry : page.rgentries)
                {
                    ASSERT((entry.size() == page.singleEntrySize), "[ERROR] Entry Size mismatch");
                }
                return true;
            }
    };

    struct DataBlock
    {
        /// data (Variable): Raw data.
        std::vector<types::byte_t> data{};
        const BlockTrailer trailer;
        /// Total Size of the Block including the padding and trailer.
        const size_t sizeWPadding{ 0 };
        

        static DataBlock Init(const std::vector<types::byte_t>& bytes, core::BREF bref)
        {
            ASSERT(!bref.bid.isInternal(), "[ERROR] A Data Block can NOT be marked as Internal");
            utils::ByteView view(bytes);
            return DataBlock( bytes, BlockTrailer(view.takeLast(16), bref) );
        }

        explicit DataBlock(const std::vector<types::byte_t>& bytes, BlockTrailer&& trailer_)
            : trailer(trailer_), sizeWPadding(bytes.size())
        {
            utils::ByteView view(bytes);
            std::vector<types::byte_t> data = view.read(trailer.cb);
            ASSERT((data.size() == trailer.cb), "[ERROR] blockBytes.size() != trailer.cb");

            size_t dwCRC = utils::ms::ComputeCRC(0, data.data(), static_cast<uint32_t>(trailer.cb));
            ASSERT((trailer.dwCRC == dwCRC), "[ERROR] trailer.dwCRC != dwCRC");
            // TODO: The data block is not always encrypted or could be encrypted with a different algorithm
            utils::ms::CryptPermute(
                data.data(),
                static_cast<int>(data.size()),
                utils::ms::DECODE_DATA
            );
            this->data = std::move(data);
        }
    };

    /*
    * @brief: XBLOCKs are used when the data associated with a node data that exceeds 8,176 bytes in size.
        The XBLOCK expands the data that is associated with a node by using an array of BIDs that reference data
        blocks that contain the data stream associated with the node. A BLOCKTRAILER is present at the end of an XBLOCK,
        and the end of the BLOCKTRAILER MUST be aligned on a 64-byte boundary.
    */
    struct XBlock
    {
        /// btype (1 byte): Block type; MUST be set to 0x01 to indicate an XBLOCK or XXBLOCK.
        uint8_t btype{};
        /// cLevel (1 byte): MUST be set to 0x01 to indicate an XBLOCK.
        uint8_t cLevel{};
        /// cEnt (2 bytes): The count of BID entries in the XBLOCK.
        uint16_t nBids{};
        /// lcbTotal (4 bytes): Total count of bytes of all the external data stored in the data blocks referenced by XBLOCK.
        uint32_t lcbTotal{};
        /// rgbid (variable): Array of BIDs that reference data blocks. The size is equal to the number of
        /// entries indicated by cEnt multiplied by the size of a BID(8 bytes for Unicode PST files).
        std::vector<core::BID> rgbid{};
        /// (variable, optional): This field is present if the total size of all of the other fields is not a multiple of 64.
        /// The size of this field is the smallest number of bytes required to make the size of the XBLOCK a multiple of 64.
        /// Implementations MUST ignore this field.
        uint32_t rgbPadding{};
        /// blockTrailer (ANSI: 12 bytes; Unicode: 16 bytes): A BLOCKTRAILER structure (section 2.2.2.8.1).
        const BlockTrailer trailer;

        static XBlock Init(const std::vector<types::byte_t>& bytes, core::BREF bref)
        {
            ASSERT(bref.bid.isInternal(), "[ERROR] A XBlock can NOT be marked as Internal");
            utils::ByteView view(bytes);
            return XBlock( bytes, BlockTrailer(view.takeLast(16), bref) );
        }

        explicit XBlock(const std::vector<types::byte_t>& bytes, BlockTrailer&& trailer_)
            : trailer(trailer_)
        {
            utils::ByteView view(bytes);
            btype = view.read<uint8_t>(1);
            cLevel = view.read<uint8_t>(1);
            nBids = view.read<uint16_t>(2);
            lcbTotal = view.read<uint32_t>(4);
            rgbid = view.entries<core::BID>(nBids, 8);

            ASSERT((btype == 0x01), "[ERROR] btype for XBlock should be 0x01 not %X", btype);
            ASSERT((cLevel == 0x01), "[ERROR] cLevel for XBlock should be 0x01 not %X", cLevel);
            ASSERT((rgbid.size() == nBids), "[ERROR]");
        }
    };

    /**
    * @brief The XXBLOCK further expands the data that is associated with a node by using an array of BIDs that reference XBLOCKs.
        A BLOCKTRAILER is present at the end of an XXBLOCK, and the end of the BLOCKTRAILER MUST be aligned on a 64-byte boundary.
    */
    class XXBlock
    {
    public:
        /// btype (1 byte): Block type; MUST be set to 0x01 to indicate an XBLOCK or XXBLOCK.
        uint8_t btype{};
        /// cLevel (1 byte): MUST be set to 0x02 to indicate an XXBLOCK.
        uint8_t cLevel{};
        /// cEnt (2 bytes): The count of BID entries in the XXBLOCK.
        uint16_t nBids{};
        /// lcbTotal (4 bytes): Total count of bytes of all the external data stored in XBLOCKs under this XXBLOCK.
        uint32_t lcbTotal{};
        /// rgbid (variable): Array of BIDs that reference XBLOCKs. The size is equal to the number of entries indicated by cEnt
        /// multiplied by the size of a BID(8 bytes for Unicode PST files).
        std::vector<core::BID> rgbid{};
        /// rgbPadding (variable, optional): This field is present if the total size of all of the other fields is not a multiple of 64.
        /// The size of this field is the smallest number of bytes required to make the size of the XXBLOCK a multiple of 64.
        /// Implementations MUST ignore this field.
        int64_t rgbPadding{};
        /// blockTrailer (Unicode: 16 bytes): A BLOCKTRAILER structure (section 2.2.2.8.1).
        const BlockTrailer trailer;

        static XXBlock Init(const std::vector<types::byte_t>& bytes, core::BREF bref)
        {
            ASSERT(bref.bid.isInternal(), "[ERROR] A XBlock can NOT be marked as Internal");
            utils::ByteView view(bytes);
            return XXBlock(bytes, BlockTrailer(view.takeLast(16), bref));
        }

        explicit XXBlock(const std::vector<types::byte_t>& bytes, BlockTrailer&& trailer_)
            : trailer(trailer_)
        {
            utils::ByteView view(bytes);
            btype = view.read<uint8_t>(1);
            cLevel = view.read<uint8_t>(1);
            nBids = view.read<uint16_t>(2);
            lcbTotal = view.read<uint32_t>(4);
            rgbid = view.entries<core::BID>(nBids, 8);

            ASSERT((btype == 0x01), "[ERROR] btype for XXBlock should be 0x01 not [%X]", btype);
            ASSERT((cLevel == 0x02), "[ERROR] cLevel for XXBlock should be 0x02 not [%X]", cLevel);
            ASSERT((rgbid.size() == nBids), "[ERROR]");
        }
    };

    //struct Block 
    //{
    //    types::BlockType blockType{types::BlockType::INVALID};
    //    std::vector<types::byte_t> data;
    //    BlockTrailer trailer;
    //};

  

    class DataTree
    {
    public:
        using GetBBT_t = std::function<BBTEntry(const core::BID& bid)>;

    public:
        DataTree(core::Ref<std::ifstream> file, const GetBBT_t& getBBT, core::BREF bref, size_t sizeOfBlockData)
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

        [[nodiscard]] size_t nDataBlocks() const
        {
            return m_dataBlocks.size();
        }

        [[nodiscard]] size_t size(size_t dataBlockIdx) const
        {
            return m_dataBlocks.at(dataBlockIdx).data.size();
        }

        [[nodiscard]] bool isValid() const
		{
            return !m_dataBlocks.empty();
		}

        const std::vector<DataBlock>& blocks()
        {
            init(); // does nothing if m_dataBlocks is NOT empty
            return m_dataBlocks;
        }

        void init()
        {
            if (m_dataBlocks.empty())
            {
                _init();
            }
        }

        [[nodiscard]] const DataBlock& at(size_t idx) const
        {
            return m_dataBlocks.at(idx);
        }

        std::vector<types::byte_t> combineDataBlocks() const
        {
            std::vector<types::byte_t> res;
            res.reserve(nDataBlocks() * 8192);
            for (const auto& block : m_dataBlocks)
            {
                for (auto byte : block.data)
                {
                    res.push_back(byte);
                }
            }
            return res;
        }

        /**
        * @return pair<totalAlignedBlockSize, offset or padding>
        */
        static std::pair<size_t, size_t> calcBlockAlignedSize(size_t sizeofBlockData)
        {
            const size_t blockTrailerSize = 16;
            const size_t multiple = 64;
            const size_t remainder = (sizeofBlockData + blockTrailerSize) % multiple;
            const size_t offset = (multiple * (remainder != 0)) - remainder;
            // The block size is the smallest multiple of 64 that can hold both the data and the block trailer.
            const size_t blockSize = (sizeofBlockData + blockTrailerSize) + offset;

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
            const auto [blockSize, offset] = calcBlockAlignedSize(m_sizeofFirstBlockData);
            const size_t blockTrailerSize = 16;

            std::vector<types::byte_t> blockBytes = readBlockBytes(m_bref, blockSize);
            utils::ByteView view(blockBytes);
            BlockTrailer trailer(view.takeLast(16), m_bref);

            ASSERT((trailer.bid == m_bref.bid), "[ERROR] Bids should match");
            ASSERT((blockSize - (blockTrailerSize + offset) == trailer.cb),
                "[ERROR] Given BlockSize [%i] != Trailer BlockSize [%i]", blockSize, trailer.cb);
            ASSERT((m_sizeofFirstBlockData == trailer.cb),
                "[ERROR] Given sizeofBlockData [%i] != Trailer BlockSize [%i]", m_sizeofFirstBlockData, trailer.cb);

            if (!trailer.bid.isInternal()) // Data Block
            {
                m_dataBlocks.push_back(DataBlock::Init(blockBytes, m_bref)); // If the first block is a data block then we are done.
            }
            else if (trailer.bid.isInternal()) // the block internal
            {
                const auto btype = utils::slice(blockBytes, 0, 1, 1, utils::toT_l<uint32_t>);
                if (btype == 0x01) // XBlock
                {
                    xBlocktoDataBlocks(XBlock::Init(blockBytes, m_bref));
                }

                else if (btype == 0x02) // XXBlock
                {
                    xxBlocktoDataBlocks(XXBlock::Init(blockBytes, m_bref));
                }
                else
                {
                    ASSERT(false, "[ERROR] Invalid btype must 0x01 or 0x02 not [%X]", btype);
                }
            }
            else // Invalid Block Type
            {
                ASSERT(false, "[ERROR] Unknown block type");
            }  
        }

        void xBlocktoDataBlocks(const XBlock& xblock)
        {
            for (const core::BID& bid : xblock.rgbid)
            {
                const BBTEntry bbt = m_getBBT(bid);
                const auto [blockSize, offset] = calcBlockAlignedSize(bbt.cb);
                const std::vector <types::byte_t> bytes = readBlockBytes(bbt.bref, blockSize);
                m_dataBlocks.push_back(DataBlock::Init(bytes, bbt.bref));
            }
        }

        void xxBlocktoDataBlocks(const XXBlock& xxblock)
        {
            for (const core::BID& bid : xxblock.rgbid)
            {
                const BBTEntry bbt = m_getBBT(bid);
                const auto [blockSize, offset] = calcBlockAlignedSize(bbt.cb);
                const std::vector <types::byte_t> bytes = readBlockBytes(bbt.bref, blockSize);
                xBlocktoDataBlocks(XBlock::Init(bytes, bbt.bref));
            }
        }

    private:
        core::Ref<std::ifstream> m_file;
        core::BREF m_bref;
        GetBBT_t m_getBBT;
        size_t m_sizeofFirstBlockData{ 0 };
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

        explicit SLEntry(const std::vector<types::byte_t>& bytes)
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

        explicit SIEntry(const std::vector<types::byte_t>& bytes)
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
        const BlockTrailer trailer;

        static SLBlock Init(const std::vector<types::byte_t>& bytes, core::BREF bref)
        {
            ASSERT(bref.bid.isInternal(), "[ERROR]");
            utils::ByteView view(bytes);
            return SLBlock(bytes, BlockTrailer(view.takeLast(16), bref));
        }

        explicit SLBlock(const std::vector<types::byte_t>& bytes, BlockTrailer&& trailer_)
            : trailer(trailer_)
        {  
            utils::ByteView view(bytes);
            btype = view.read<uint8_t>(1); //utils::slice(bytes, 0, 1, 1, utils::toT_l<uint8_t>);
            cLevel = view.read<uint8_t>(1); //utils::slice(bytes, 1, 2, 1, utils::toT_l<uint8_t>);
            cEnt = view.read<uint16_t>(2); //utils::slice(bytes, 2, 4, 2, utils::toT_l<uint16_t>);
            dwPadding = view.read<uint32_t>(4); //utils::slice(bytes, 4, 8, 2, utils::toT_l<uint32_t>);
            entries = view.entries<SLEntry>(cEnt, 24);

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
        const BlockTrailer trailer;

        static SIBlock Init(const std::vector<types::byte_t>& bytes, core::BREF bref)
        {
            ASSERT(bref.bid.isInternal(), "[ERROR]");
            utils::ByteView view(bytes);
            return SIBlock(bytes, BlockTrailer(view.takeLast(16), bref));
        }

        explicit SIBlock(const std::vector<types::byte_t>& bytes, BlockTrailer&& trailer_)
            : trailer(trailer_)
        {
            utils::ByteView view(bytes);
            btype = view.read<uint8_t>(1);
            cLevel = view.read<uint8_t>(1);
            cEnt = view.read<uint16_t>(2);
            dwPadding = view.read<uint32_t>(4);
            entries = view.entries<SIEntry>(cEnt, 16);
            ASSERT((btype == 0x02), "[ERROR]");
            ASSERT((cLevel == 0x01), "[ERROR]");
        }
    };

    class SubNodeBTree
    {
    public:
        using GetBBT_t = std::function<BBTEntry(const core::BID& bid)>;
            
    public:

        SubNodeBTree(core::BID bid, core::Ref<std::ifstream> file, const GetBBT_t& getBBT)
            : m_bid(bid), m_file(file), m_getBBT(getBBT)
        {
            if (m_bid.getBidRaw() != 0) // When BID == 0 there is no subnode tree
            {
                const auto [bytes, bbt] = readBlockBytes(m_bid);
                const uint8_t clevel = bytes.at(1);
                if (clevel == 0x00) // SL Block
                {
                    m_slblocks.push_back(SLBlock::Init(bytes, bbt.bref));
                }
                else if (clevel == 0x01) // SI Block
                {
                    const SIBlock siblock = SIBlock::Init(bytes, bbt.bref);
                    for (const auto& sientry : siblock.entries)
                    {
                        const auto [slBlockBytes, slBlockBBT] = readBlockBytes(sientry.bid);
                        m_slblocks.emplace_back(SLBlock::Init(slBlockBytes, slBlockBBT.bref));
                    }
                }
                else // Encountered Invalid Block Type
                {
                    ASSERT(false, "[ERROR] Unknown SubNode Type");
                }

                for (const auto& slblock : m_slblocks)
                {
                    for (const SLEntry& slentry : slblock.entries)
                    {
                        const uint32_t nidID = slentry.nid.getNIDRaw();
                        ASSERT(!m_subtrees.contains(nidID), "[ERROR] Duplicate entry in Nested SubNodeBTree Map");
                        ASSERT(!m_datatrees.contains(nidID), "[ERROR] Duplicate entry In DataTree Map");
                        if (slentry.bidSub.getBidRaw() != 0) // theres a nested subnode btree
                        {
                            LOG("[WARN] A nested SubNodeBTree was created");
                            m_subtrees.emplace(
                                std::piecewise_construct,
                                std::forward_as_tuple(nidID),
                                std::forward_as_tuple(slentry.bidSub, m_file, m_getBBT)
                            );
                        }
                        const auto [dataTreeBytes, dataTreeBBT] = readBlockBytes(slentry.bidData);
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

        [[nodiscard]] DataTree* getDataTree(core::NID nid)
        {
            if (m_datatrees.contains(nid.getNIDRaw()))
            {
                return &m_datatrees.at(nid.getNIDRaw());
            }
            for (auto& [_, subtree] : m_subtrees)
            {
                DataTree* data = subtree.getDataTree(nid);
                if (data != nullptr)
                {
                    return data;
                }
            }
            LOG("[WARN] Failed to find DataBlock in SubNode Tree");
            return nullptr;
        }

        [[nodiscard]] const DataTree* const getDataTree(core::NID nid) const
        {
            if (m_datatrees.contains(nid.getNIDRaw()))
            {
                return &m_datatrees.at(nid.getNIDRaw());
            }
            for (const auto& [_, subtree] : m_subtrees)
            {
                const auto data = subtree.getDataTree(nid);
                if (data != nullptr)
                {
                    return data;
                }
            }
            LOG("[WARN] Failed to find DataBlock in SubNode Tree");
            return nullptr;
        }

        [[nodiscard]] const SubNodeBTree* const getNestedSubNodeTree(core::NID nid) const
        {
            if (m_subtrees.contains(nid.getNIDRaw()))
            {
                return &m_subtrees.at(nid.getNIDRaw());
            }
            LOG("[WARN] Failed to find matching subnode btree");
            return nullptr;
        }

        std::pair<std::vector<types::byte_t>, BBTEntry> readBlockBytes(core::BID bid)
        {
            const BBTEntry bbt = m_getBBT(bid);
            const size_t totalBlockSize = calcBlockAlignedSize(bbt.cb);
            m_file->seekg(bbt.bref.ib, std::ios::beg);
            return { utils::readBytes(m_file.get(), totalBlockSize), bbt };
        }

        /**
         * @brief 
         * @param dataSize is the size of btype + clevel + cEnt + dwpadding + plus cEnt * Entry Size  
        */
        static size_t calcBlockAlignedSize(size_t dataSize)
        {
            const size_t blockTrailerSize = 16;
            const size_t totalBlockSizeWithoutPadding = dataSize + blockTrailerSize; // +sBType + sCLevel + sCEnt + sDwPadding;

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
        // uint32_t is a Raw NID
        std::unordered_map<uint32_t, SubNodeBTree> m_subtrees;
        // uint32_t is a Raw NID
        std::unordered_map<uint32_t, DataTree> m_datatrees;
    };

    class SubNodeTreeContainer
    {
    public:
        SubNodeTreeContainer(core::BID bid, core::Ref<std::ifstream> file, const SubNodeBTree::GetBBT_t& getBBT)
            : m_bid(bid), m_file(file), m_getBBT(getBBT)
        {

        }
    private:
        std::unordered_map<uint32_t, SubNodeBTree> m_subtrees;
        core::Ref<std::ifstream> m_file;
        core::BID m_bid;
        SubNodeBTree::GetBBT_t m_getBBT;
    };

    template<typename LeafEntryType>
    class BTree
    {
    public:
        explicit BTree(BTPage rootPage)
        {
            addSubPage(rootPage);
            _init();
        }
        [[nodiscard]] const BTPage& rootPage() const
        {
			return m_pages.at(0);
		}

        [[nodiscard]] types::PType ptype() const
        {
			return rootPage().trailer.ptype;
		}

        [[nodiscard]] uint8_t cLevel() const
        {
            return rootPage().cLevel;
        }
        [[nodiscard]] size_t size() const
        {
            return m_pages.size();
        }

        [[nodiscard]] const BTPage& at(size_t index) const
        {
			return m_pages.at(index);
		}

        [[nodiscard]] auto begin() const
        {
            return m_pages.begin();
        }

        [[nodiscard]] auto end() const
        {
            return m_pages.end();
        }

        bool addSubPage(BTPage page)
        {
			m_pages.push_back(page);
			return true;
		}

        [[nodiscard]] std::map<types::NIDType, NBTEntry> all(core::NID nid) const
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
        [[nodiscard]] LeafEntryType get(NIDorBID id) const
        {
            for (size_t i = 0; i < m_pages.size(); i++)
            {
                const BTPage& page = m_pages.at(i);
                if (page.has<LeafEntryType>())
                {
                    for (size_t j = 0; j < page.rgentries.size(); j++)
                    {
                        auto entry = page.rgentries.at(j).as<LeafEntryType>();
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
            ASSERT(false, "Failed to find iD");
            return LeafEntryType{};
        }

        template<typename NIDorBID>
        uint32_t count(NIDorBID id)
        {
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
            for (const auto& page : m_pages)
            {
                page.verify();
            }
            return true;
        }


    private:
        void _init()
        {
            m_pages.reserve(20000);
            if constexpr (std::is_same_v<LeafEntryType, NBTEntry>)
            {
                ASSERT((rootPage().trailer.ptype == types::PType::NBT), "[ERROR] Root BTPage with NBTEntries must be NBT");
            }
            else if constexpr (std::is_same_v<LeafEntryType, BBTEntry>)
            {
                ASSERT((rootPage().trailer.ptype == types::PType::BBT), "[ERROR] Root BTPage with BBTEntries must be BBT");
            }
            else
            {
                ASSERT(false, "[ERROR]");
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
            m_rootNBT(BTree<NBTEntry>(InitBTPage(m_header.root.nodeBTreeRootPage, types::PType::NBT))),
            m_rootBBT(BTree<BBTEntry>(InitBTPage(m_header.root.blockBTreeRootPage, types::PType::BBT)))
        {
            _init();
        }

        [[nodiscard]] std::map<types::NIDType, NBTEntry> all(core::NID nid) const
		{
			return m_rootNBT.all(nid);
		}

        template<typename IDType>
        [[nodiscard]] auto get(IDType id) const
        {
            if constexpr (std::is_same_v<IDType, core::NID>)
            {
                return m_rootNBT.get(id);
            }
            else if constexpr (std::is_same_v<IDType, core::BID>)
            {
                return m_rootBBT.get(id);
            }			
            else
            {
                ASSERT(false, "[ERROR] Invalid IDType");
            }
		}

        bool verify()
        {
            m_rootBBT.verify();
            m_rootNBT.verify();

            for (const auto& page : m_rootBBT)
            {
                if (page.hasBBTEntries())
                {
                    for (const auto& entry : page.rgentries)
                    {
                        const auto bbt = entry.as<BBTEntry>();
                        ASSERT((bbt.bref.bid.isSetup()), "[ERROR]");
                    }
                }
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

        DataTree InitDataTree(core::BREF blockBref, size_t sizeofBlockData) const
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

        BTPage InitBTPage(core::BREF bref, types::PType treeType, int32_t parentCLevel = -1)
        {
            // from the begning of the file move to .ib
            m_file.seekg(bref.ib, std::ios::beg);
            return BTPage::Init(utils::readBytes(m_file, BTPage::size), bref, parentCLevel);
        }

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
                        btree.addSubPage(InitBTPage(btentry.bref, rootPagePType, currentPage.cLevel));
                    }
                }
                idxProcessing++;
            }
            LOG("hehll");
            LOG("[INFO] %s has %i subpages", 
                utils::PTypeToString(rootPagePType).c_str(), btree.size());
        }

    private:
        std::ifstream& m_file;
        core::Header m_header;
        BTree<NBTEntry> m_rootNBT;
        BTree<BBTEntry> m_rootBBT;
    };
}

#endif // READER_NDB_H