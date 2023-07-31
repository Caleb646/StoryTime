#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <fstream>
#include <type_traits>
#include <utility>
#include <functional>
#include <unordered_map>
#include <array>
#include <optional>

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
                STORYT_ASSERT((wSig == computedSig), "[ERROR] Page Sig [{}] != Computed Sig [{}]", wSig, computedSig);
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
            STORYT_ASSERT((ptype == ptypeRepeat), "PageTrailer Constructor");
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
            STORYT_ASSERT((wSig == computedSig),
                "[ERROR] Page Sig [{}] != Computed Sig [{}]", wSig, computedSig);
        }

        explicit BlockTrailer(const std::vector<types::byte_t>& bytes)
        {
            STORYT_ASSERT((bytes.size() == 16), "Block Trailer has to be 16 bytes not [{}]", bytes.size());
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
        uint64_t btkey{};
        /// (Unicode: 16 bytes): BREF structure (section 2.2.2.4) that points to the child BTPAGE.
        core::BREF bref{};

        static constexpr size_t sizeNBytes = 24;
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
        
        static constexpr size_t sizeNBytes = 24;
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
    public:
 
        [[nodiscard]] bool hasSubNode() const { return bidSub.getBidRaw() != 0; }
        static constexpr size_t id() { return 3; }
        static constexpr size_t sizeNBytes = 32;

    };

    class Entry 
    {
    public:
        static constexpr uint8_t EntryMaxSize = 32;
    public:
        explicit Entry(const std::vector<types::byte_t>& data) 
            : m_data(data) 
        {
            _init();
        }
        explicit Entry(const utils::Array<types::byte_t, Entry::EntryMaxSize>& data) 
            : m_data(data) 
        {
            _init();
        }

        [[nodiscard]] BTEntry asBTEntry() const
        {
            utils::ArrayView view = m_data.view(0, BTEntry::sizeNBytes);
            BTEntry entry{};  
            entry.btkey = view.to<uint64_t>(8);
            const uint64_t bid = view.to<uint64_t>(8);
            const uint64_t ib = view.to<uint64_t>(8);
            entry.bref = core::BREF(bid, ib); 
            return entry;
        }

        [[nodiscard]] BBTEntry asBBTEntry() const
        {
            utils::ArrayView view = m_data.view(0, BBTEntry::sizeNBytes);
            BBTEntry entry{};
            const uint64_t bid = view.to<uint64_t>(8);
            const uint64_t ib = view.to<uint64_t>(8);
            entry.bref = core::BREF(bid, ib);
            entry.cb = view.to<uint16_t>(2); 
            entry.cRef = view.to<uint16_t>(2);
            entry.dwPadding = view.to<uint32_t>(4); 
            return entry;
        }

        [[nodiscard]] NBTEntry asNBTEntry() const
        {
            utils::ArrayView view = m_data.view(0, NBTEntry::sizeNBytes);
            NBTEntry entry{};
            entry.nid = core::NID(view.to<uint64_t>(8)); 
            entry.bidData = core::BID(view.to<uint64_t>(8));
            entry.bidSub = core::BID(view.to<uint64_t>(8));
            entry.nidParent = core::NID(view.to<uint32_t>(4));
            entry.dwPadding = view.to<uint32_t>(4); 
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
                STORYT_ASSERT(false, "Invalid EntryType");
                return EntryType();
            }
		}
        core::NID getCachedNBTNID() const
        {
            return m_nid;
        }
        core::BID getCachedBBTBID() const
        {
            return m_bid;
        }
        uint64_t getCachedBTKey() const
        {
            return m_btkey;
        }
    private:
        void _init()
        {
            utils::ArrayView view = m_data.view(0, Entry::EntryMaxSize);
            // Read the first 8 bytes of every entry and cache the NID and BID values
            // so searching is faster in the BTree
            m_nid = core::NID(view.setStart(0).to<uint64_t>(8));
            m_bid = core::BID(view.setStart(0).to<uint64_t>(8));
            m_btkey = view.setStart(0).to<uint64_t>(8);
        }
    private:
        // TODO: Should be maybe cache the results of as()
        utils::Array<types::byte_t, 32> m_data{};
        core::NID m_nid;
        core::BID m_bid;
        uint64_t m_btkey;
    };

    class BTPage
    {
    public:
        using GetBytes_t = std::function<std::vector<types::byte_t>(uint64_t position)>;
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

        static BTPage Init(const std::vector<types::byte_t>& bytes, core::BREF bref, std::ifstream& file, int32_t parentCLevel = -1)
        {
            STORYT_ASSERT((bytes.size() == BTPage::size), "BTPage size [{}] != bytes.size() [{}]", BTPage::size, bytes.size());
            utils::ByteView view(bytes);
            return { bytes, PageTrailer(view.takeLast(16), bref), file, parentCLevel };
        }

        static BTPage Init(const std::vector<types::byte_t>& bytes, core::BREF bref, int32_t parentCLevel = -1)
        {
            STORYT_ASSERT((bytes.size() == BTPage::size), "BTPage size [{}] != bytes.size() [{}]", BTPage::size, bytes.size());
            utils::ByteView view(bytes);
            return { bytes, PageTrailer(view.takeLast(16), bref), parentCLevel };
        }

        static BTPage Init(const std::vector<types::byte_t>& bytes, int32_t parentCLevel = -1)
        {
            STORYT_ASSERT((bytes.size() == BTPage::size), "BTPage size [{}] != bytes.size() [{}]", BTPage::size, bytes.size());
            utils::ByteView view(bytes);
            return { bytes, PageTrailer(view.takeLast(16)), parentCLevel };
        }

        BTPage(
            const std::vector<types::byte_t>& bytes,
            PageTrailer&& trailer_,
            std::ifstream& file,
            int32_t parentCLevel = -1
        )
            : trailer(trailer_)
        {
            setup(bytes, parentCLevel);
            if (hasBTEntries())
            {
                for (const auto& entry : rgentries)
                {
                    const BTEntry bt = entry.asBTEntry();
                    subPages.push_back(
                        BTPage::Init(utils::readBytes(file, bt.bref.ib, BTPage::size), bt.bref, file, cLevel)
                    );
                }
            }
        }

        BTPage(
            const std::vector<types::byte_t>& bytes, 
            PageTrailer&& trailer_, 
            int32_t parentCLevel = -1)
            : trailer(trailer_)
        {
            setup(bytes, parentCLevel);
        }

        void setup(const std::vector<types::byte_t>& bytes, int32_t parentCLevel = -1)
        {
            utils::ByteView view(bytes);
            view.skip(488); // skip entries for now

            nEntries = view.read<uint8_t>(1);
            maxNEntries = view.read<uint8_t>(1);
            singleEntrySize = view.read<uint8_t>(1);
            cLevel = view.read<uint8_t>(1);
            dwPadding = view.read<uint32_t>(4);
            // reset to beginning of bytes to read the entries
            rgentries = view.setStart(0).entries<Entry>(nEntries, singleEntrySize);

            STORYT_ASSERT((trailer.ptype == types::PType::BBT || trailer.ptype == types::PType::NBT), "Invalid ptype for pagetrailer");
            STORYT_ASSERT((nEntries <= maxNEntries), "Invalid cEnt [{}]", nEntries);
            STORYT_ASSERT((dwPadding == 0), "dwPadding should be 0 not [{}]", dwPadding);
            STORYT_ASSERT((nEntries == rgentries.size()), "nEntries [{}] != rgentries.size() [{}]", nEntries, rgentries.size());
            if (parentCLevel != -1)
            {
                STORYT_ASSERT((parentCLevel - 1 == cLevel),
                    "SubBTPage cLevel [{}] must be smaller than ParentBTPage cLevel [{}]", cLevel, parentCLevel);
            }
        }

        [[nodiscard]] std::unordered_map<types::NIDType, NBTEntry> all(core::NID nid) const
        {
            std::unordered_map<types::NIDType, NBTEntry> map{};
            all_(nid, map);
            return map;
        }

        void all_(core::NID nid, std::unordered_map<types::NIDType, NBTEntry>& entries) const
        {
            if (NBTEntry::id() == getEntryType())
            {
                for (auto entry : rgentries)
                {
                    // NID Index is used because a single PST Folder has 4 parts and those 4 parts
                    // have the same NID Index but different NID Types. The == operator for NID class compares
                    // the NID Type and NID Index.
                    if (entry.getCachedNBTNID().getNIDIndex() == nid.getNIDIndex())
                    {
                        NBTEntry nbt = entry.asNBTEntry();
                        STORYT_ASSERT(!entries.contains(nbt.nid.getNIDType()), "Duplicate NID Type found in NBT");
                        entries[nbt.nid.getNIDType()] = nbt;
                    }
                }
            }

            for (const auto& page : subPages)
            {
                page.all_(nid, entries);
            }
        }

        template<typename EntryType, typename EntryIDType>
        [[nodiscard]] std::optional<EntryType> get(EntryIDType id) const
        {
            if (EntryType::id() == getEntryType())
            {
                for (const auto& entry : rgentries)
                {
                    //const EntryType e = entry.as<EntryType>();
                    if constexpr (std::is_same_v<EntryIDType, core::NID>)
                    {
                        if (entry.getCachedNBTNID() == id)
                        {
                            return entry.asNBTEntry();
                        }
                    }
                    else
                    {
                        if (entry.getCachedBBTBID() == id)
                        {
                            return entry.asBBTEntry();
                        }
                    }
                }
            }
            else if (BTEntry::id() == getEntryType())
            {
                for (size_t i = 0; i < rgentries.size(); ++i)
                {
                    //const BTEntry bt = rgentries.at(i).as<BTEntry>();
                    uint64_t btkey = rgentries.at(i).getCachedBTKey();
                    if (id > btkey || id == btkey)
                    {
                        std::optional<EntryType> ret = subPages.at(i).get<EntryType>(id);
                        if (ret.has_value())
                        {
                            return ret;
                        }
                    }
                }
            }
            return {};
        }

        [[nodiscard]] size_t getEntryType() const
        {
            return getEntryType(trailer.ptype, cLevel);
        }

        template<typename T>
        static size_t getEntryType(types::PType pagePtype, T cLevel)
        {
            STORYT_ASSERT((pagePtype != types::PType::INVALID), 
                "Pagetrailer was not setup properly. types::PType is set to INVALID .");
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

            STORYT_ASSERT(false, "Invalid types::PType for BTPage [{}] with cLevel [{}]", 
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
                STORYT_ASSERT(false, "Invalid EntryType");
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
            [[nodiscard]] bool _verify(const BTPage& page) const
            {
                const types::PType ptype = trailer.ptype;
                STORYT_ASSERT((page.trailer.ptype == ptype), "Subpage has different ptype than parent page.");
                STORYT_ASSERT((page.nEntries == page.rgentries.size()), "Subpage has different number of entries than cEnt.");
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
            STORYT_ASSERT(!bref.bid.isInternal(), "A Data Block can NOT be marked as Internal");
            utils::ByteView view(bytes);
            return DataBlock( bytes, BlockTrailer(view.takeLast(16), bref) );
        }

        explicit DataBlock(const std::vector<types::byte_t>& bytes, BlockTrailer&& trailer_)
            : trailer(trailer_), sizeWPadding(bytes.size())
        {
            utils::ByteView view(bytes);
            std::vector<types::byte_t> data = view.read(trailer.cb);
            STORYT_ASSERT((data.size() == trailer.cb), "blockBytes.size() != trailer.cb");

            const size_t dwCRC = utils::ms::ComputeCRC(0, data.data(), static_cast<uint32_t>(trailer.cb));
            STORYT_ASSERT((trailer.dwCRC == dwCRC), "trailer.dwCRC != dwCRC");
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
            STORYT_ASSERT(bref.bid.isInternal(), "A XBlock can NOT be marked as Internal");
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

            STORYT_ASSERT((btype == 0x01), "btype for XBlock should be 0x01 not [{}]", btype);
            STORYT_ASSERT((cLevel == 0x01), "cLevel for XBlock should be 0x01 not [{}]", cLevel);
            STORYT_ASSERT((rgbid.size() == nBids), "rgbid.size() != nBids");
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
            STORYT_ASSERT(bref.bid.isInternal(), "A XBlock can NOT be marked as Internal");
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

            STORYT_ASSERT((btype == 0x01), "btype for XXBlock should be 0x01 not [{}]", btype);
            STORYT_ASSERT((cLevel == 0x02), "cLevel for XXBlock should be 0x02 not [{}]", cLevel);
            STORYT_ASSERT((rgbid.size() == nBids), "rgbid.size() != nBids");
        }
    };

    class DataTree
    {
    public:
        using GetBBT_t = std::function<std::optional<BBTEntry>(const core::BID& bid)>;

    public:
        DataTree(core::Ref<std::ifstream> file, const GetBBT_t& getBBT, core::BREF bref, size_t sizeOfBlockData)
            : m_file(file), m_firstBlockBREF(bref), m_getBBT(getBBT), m_sizeofFirstBlockData(sizeOfBlockData)
        {
            static_assert(std::is_move_constructible_v<DataTree>, "DataTree must be move constructible");
            static_assert(std::is_move_assignable_v<DataTree>, "DataTree must be move assignable");
            static_assert(std::is_copy_constructible_v<DataTree>, "DataTree must be copy constructible");
            static_assert(std::is_copy_assignable_v<DataTree>, "DataTree must be copy assignable");
        }

        [[nodiscard]] size_t nDataBlocks() const
        {
            STORYT_ASSERT(m_DataBlocksAreSetup, "The DataTree has NOT loaded its DataBlocks");
            return m_dataBlocks.size();
        }

        [[nodiscard]] size_t sizeOfDataBlockData(size_t dataBlockIdx) const
        {
            STORYT_ASSERT(m_DataBlocksAreSetup, "The DataTree has NOT loaded its DataBlocks");
            return m_dataBlocks.at(dataBlockIdx).data.size();
        }

        [[nodiscard]] const DataBlock& at(size_t idx) const
        {
            STORYT_ASSERT(m_DataBlocksAreSetup, "The DataTree has NOT loaded its DataBlocks");
            return m_dataBlocks.at(idx);
        }

        [[nodiscard]] std::vector<types::byte_t> combineDataBlocks()
        {
            STORYT_ASSERT(m_DataBlocksAreSetup, "The DataTree has NOT loaded its DataBlocks");
            const size_t maxBlockSize = 8192U;
            std::vector<types::byte_t> res;
            res.reserve(nDataBlocks() * maxBlockSize);
            for (const auto& block : m_dataBlocks)
            {
                for (auto byte : block.data)
                {
                    res.push_back(byte);
                }
            }
            return res;
        }

        auto begin()
        {
            load();
            return m_dataBlocks.begin();
        }
        auto end()
        {
            return m_dataBlocks.end();
        }

        DataTree& load() // A DataTree's Datablocks are only loaded into memory after the first access
        {
            if (m_DataBlocksAreSetup)
            {
                return *this;
            }
            const auto [blockSize, offset] = calcBlockAlignedSize(m_sizeofFirstBlockData);
            const size_t blockTrailerSize = 16U;

            std::vector<types::byte_t> blockBytes = _readBlockBytes(m_firstBlockBREF.ib, blockSize);
            utils::ByteView view(blockBytes);
            BlockTrailer trailer(view.takeLast(blockTrailerSize), m_firstBlockBREF);

            STORYT_ASSERT((trailer.bid == m_firstBlockBREF.bid), 
                "Bids should match");
            STORYT_ASSERT((blockSize - (blockTrailerSize + offset) == trailer.cb),
                "Given BlockSize [{}] != Trailer BlockSize [{}]", blockSize, trailer.cb);
            STORYT_ASSERT((m_sizeofFirstBlockData == trailer.cb),
                "Given sizeofBlockData [{}] != Trailer BlockSize [{}]", m_sizeofFirstBlockData, trailer.cb);

            if (!trailer.bid.isInternal()) // Data Block
            {
                m_dataBlocks.push_back(DataBlock::Init(blockBytes, m_firstBlockBREF)); // If the first block is a data block then we are done.
            }
            else if (trailer.bid.isInternal()) // the block internal
            {
                const auto btype = utils::slice(blockBytes, 0, 1, 1, utils::toT_l<uint32_t>);
                if (btype == 0x01U) // XBlock
                {
                    _xBlocktoDataBlocks(XBlock::Init(blockBytes, m_firstBlockBREF));
                }

                else if (btype == 0x02U) // XXBlock
                {
                    _xxBlocktoDataBlocks(XXBlock::Init(blockBytes, m_firstBlockBREF));
                }
                else
                {
                    STORYT_ASSERT(false, "Invalid btype must 0x01 or 0x02 not [{}]", btype);
                }
                _flush(); // only flush when there are X or XX Blocks
            }
            else // Invalid Block Type
            {
                STORYT_ASSERT(false, "Unknown block type");
            }
            m_DataBlocksAreSetup = true;
            return *this;
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

            STORYT_ASSERT((blockSize % 64 == 0),
                "Block Size must be a mutiple of 64");
            STORYT_ASSERT((blockSize <= 8192),
                "Block Size must less than or equal to the max blocksize of 8192");
            return { blockSize, offset };
        }

    private:

        std::vector<types::byte_t> _readBlockBytes(uint64_t position, uint64_t blockTotalSize)
        {
            m_file->seekg(position, std::ios::beg);
            return utils::readBytes(m_file.get(), blockTotalSize);
        }

        void _xBlocktoDataBlocks(const XBlock& xblock)
        {
            m_dataBlockBBTs.reserve(xblock.nBids);
            for (const core::BID& bid : xblock.rgbid)
            {
                const std::optional<BBTEntry> bbt = m_getBBT(bid);
                m_dataBlockBBTs.push_back(bbt.value());
            }
        }

        void _xxBlocktoDataBlocks(const XXBlock& xxblock)
        {
            for (const core::BID& bid : xxblock.rgbid)
            {
                const std::optional<BBTEntry> bbt = m_getBBT(bid);
                const auto [blockSize, offset] = calcBlockAlignedSize(bbt.value().cb);
                const std::vector <types::byte_t> bytes = _readBlockBytes(bbt.value().bref.ib, blockSize);
                _xBlocktoDataBlocks(XBlock::Init(bytes, bbt.value().bref));
            }
        }

        bool DataBlocksAreStoredContiguously_()
        {
            for (size_t i = 1; i < m_dataBlockBBTs.size(); ++i)
            {
                const uint64_t start = m_dataBlockBBTs.at(i - 1).bref.ib;
                const uint64_t end = m_dataBlockBBTs.at(i).bref.ib;
                auto [expectedSize, offset] = calcBlockAlignedSize(m_dataBlockBBTs.at(i - 1).cb);
                if (start > end || end - start != expectedSize)
                {
                    return false;
                }
            }
            return true;
        }

        size_t TotalDataBlockFileBytes_()
        {
            STORYT_ASSERT(DataBlocksAreStoredContiguously_(), "Data Blocks Are NOT Stored Contiguously");
            size_t nBytes{ 0 };
            for (const auto& entry : m_dataBlockBBTs)
            {
                auto [totalSize, offset] = calcBlockAlignedSize(entry.cb);
                nBytes += totalSize;
            }
            return nBytes;
        }

        void _flush()
        {
            if (m_dataBlockBBTs.empty())
            {
                STORYT_ERROR("[WARN] Flush was called with 0 data block BBTs");
                return;
            }
            m_dataBlocks.reserve(m_dataBlockBBTs.size());
            if (DataBlocksAreStoredContiguously_())
            {
                const size_t nBytes = TotalDataBlockFileBytes_();
                std::vector<types::byte_t> allBlocksBytes = _readBlockBytes(m_dataBlockBBTs.at(0).bref.ib, nBytes);
                utils::ByteView view(allBlocksBytes);
                for (const auto& entry : m_dataBlockBBTs)
                {
                    auto [totalSize, offset] = calcBlockAlignedSize(entry.cb);
                    m_dataBlocks.push_back(DataBlock::Init(view.read(totalSize), entry.bref));
                }
            }
            else
            {
                for (const auto& entry : m_dataBlockBBTs)
                {
                    auto [totalSize, offset] = calcBlockAlignedSize(entry.cb);
                    m_dataBlocks.push_back(DataBlock::Init(_readBlockBytes(entry.bref.ib, totalSize), entry.bref));
                }
            }
        }

    private:
        core::Ref<std::ifstream> m_file;
        core::BREF m_firstBlockBREF;
        GetBBT_t m_getBBT;
        size_t m_sizeofFirstBlockData{ 0 };
        std::vector<BBTEntry> m_dataBlockBBTs{};
        std::vector<DataBlock> m_dataBlocks{};
        bool m_DataBlocksAreSetup{ false };
    };
    /**
        * @brief SLENTRY are records that refer to internal subnodes of a node.
    */
    struct SLEntry
    {
        /// (8 bytes): Local NID of the subnode. This NID is guaranteed to be
        /// unique only within the parent node.
        core::NID nid;
        /// (8 bytes): The BID of the data block associated with the subnode.
        core::BID bidData;
        /// (8 bytes; ANSI: 4 bytes): If nonzero, the BID of the subnode of this subnode.
        core::BID bidSub;

        explicit SLEntry(const std::vector<types::byte_t>& bytes)
        {
            STORYT_ASSERT((bytes.size() == 24), "bytes.size() [{}] != SLEntry::size [24]", bytes.size());
            utils::ByteView view(bytes);
            nid = view.entry<core::NID>(8);
            bidData = view.entry<core::BID>(8);
            bidSub = view.entry<core::BID>(8);
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
            STORYT_ASSERT((bytes.size() == 16), "bytes.size() [{}] != SIEntry size [16]", bytes.size());
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
            STORYT_ASSERT(bref.bid.isInternal(), "SLBlock should be marked as an Internal Block");
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

            STORYT_ASSERT((btype == 0x02), "btype != 0x02");
            STORYT_ASSERT((cLevel == 0x00), "cLevel != 0x00");
            STORYT_ASSERT((cEnt != 0), "cEnt == 0");
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
            STORYT_ASSERT(bref.bid.isInternal(), "SIBlock should be marked as an Internal Block");
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
            STORYT_ASSERT((btype == 0x02), "btype != 0x02");
            STORYT_ASSERT((cLevel == 0x01), "cLevel != 0x01");
        }
    };

    class SubNodeBTree
    {
    public:
        using GetBBT_t = std::function<std::optional<BBTEntry>(const core::BID& bid)>;
            
    public:
        SubNodeBTree(core::BID bid, core::Ref<std::ifstream> file, const GetBBT_t& getBBT)
            : m_bid(bid), m_file(file), m_getBBT(getBBT)
        {
            if (m_bid.getBidRaw() != 0) //&& m_bid.getBidRaw() != 1978398) // When BID == 0 there is no subnode tree
            {
                const auto [bytes, bbt] = _readBlockBytes(m_bid);
                const uint8_t clevel = bytes.at(1);
                if (clevel == 0x00) // SL Block
                {
                    _slBlockToSLEntries(SLBlock::Init(bytes, bbt.bref));
                }
                else if (clevel == 0x01) // SI Block
                {
                    _siBlockToSLEntries(SIBlock::Init(bytes, bbt.bref));
                }
                else // Encountered Invalid Block Type
                {
                    STORYT_ASSERT(false, "Invalid Block Type [{}]", clevel);
                }

                m_subtrees.reserve(m_slentries.size());
                m_datatrees.reserve(m_slentries.size());

                for (const SLEntry& slentry : m_slentries)
                {
                    const uint32_t nidID = slentry.nid.getNIDRaw();
                    STORYT_ASSERT(!m_subtrees.contains(nidID), "Duplicate entry in Nested SubNodeBTree Map");
                    STORYT_ASSERT(!m_datatrees.contains(nidID), "Duplicate entry In DataTree Map");
                    if (slentry.bidSub.getBidRaw() != 0) // theres a nested subnode btree
                    {
                        m_subtrees.emplace(
                            std::piecewise_construct,
                            std::forward_as_tuple(nidID),
                            std::forward_as_tuple(slentry.bidSub, m_file, m_getBBT)
                        );
                    }
                    const std::optional<BBTEntry> dataTreeBBT = m_getBBT(slentry.bidData);
                    if (dataTreeBBT.has_value())
                    {
                        m_datatrees.emplace(
                            std::piecewise_construct,
                            std::forward_as_tuple(nidID),
                            std::forward_as_tuple(m_file, m_getBBT, dataTreeBBT.value().bref, dataTreeBBT.value().cb)
                        );
                    }
                    else
                    {
                        STORYT_ERROR("Failed to find BBTEntry with BID [{}]", slentry.bidData.getBidRaw());
                    }   
                }
            }
        }

        [[nodiscard]] DataTree* getDataTree(core::NID nid)
        {
            if (m_datatrees.contains(nid.getNIDRaw()))
            {
                return &m_datatrees.at(nid.getNIDRaw()).load();
            }
            for (auto& [_, subtree] : m_subtrees)
            {
                DataTree* data = subtree.getDataTree(nid);
                if (data != nullptr)
                {
                    return &data->load(); // Make sure Data Tree is loaded
                }
            }
            return nullptr;
        }

        [[nodiscard]] SubNodeBTree* getNestedSubNodeTree(core::NID nid)
        {
            if (m_subtrees.contains(nid.getNIDRaw()))
            {
                return &m_subtrees.at(nid.getNIDRaw());
            }
            return nullptr;
        }

        [[nodiscard]] const SubNodeBTree* const getNestedSubNodeTree(core::NID nid) const
        {
            if (m_subtrees.contains(nid.getNIDRaw()))
            {
                return &m_subtrees.at(nid.getNIDRaw());
            }
            return nullptr;
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
        void _slBlockToSLEntries(const SLBlock& block) // SL Block to SL Entries
        {
            for (const SLEntry& slentry : block.entries)
            {
                m_slentries.push_back(slentry);
            }
        }

        void _siBlockToSLEntries(const SIBlock& block) // SI Block to SL Entries
        {
            std::vector<BBTEntry> entries;
            entries.reserve(block.entries.size());
            for (const SIEntry& sientry : block.entries)
            {
                const std::optional<BBTEntry> bbt = m_getBBT(sientry.bid);
                entries.push_back(bbt.value());
            }
            for (size_t i = 1; i < entries.size(); ++i)
            {
                const size_t start = entries.at(i - 1).bref.ib;
                const size_t end = entries.at(i).bref.ib;
                const size_t expectedSize = calcBlockAlignedSize(entries.at(i - 1).bref.ib);
                STORYT_ASSERT((start < end), "start is not < end");
                STORYT_ASSERT((end - start == expectedSize), "end - start != expectedSize");
            }
            size_t nBytes{ 0 };
            for (const BBTEntry& bbt : entries)
            {
                nBytes += calcBlockAlignedSize(bbt.bref.ib);
            }
            std::vector<types::byte_t> SLBlocksBytes = _readBlockBytes(entries.at(0).bref.ib, nBytes);
            utils::ByteView view(SLBlocksBytes);
            for (const BBTEntry& bbt : entries)
            {
                const size_t blockSize = calcBlockAlignedSize(bbt.bref.ib);
                _slBlockToSLEntries(SLBlock::Init(view.read(blockSize), bbt.bref));
            }
        }

        std::vector<types::byte_t> _readBlockBytes(uint64_t position, uint64_t totalSize)
        {
            return utils::readBytes(m_file.get(), totalSize);
        }

        std::pair<std::vector<types::byte_t>, BBTEntry> _readBlockBytes(core::BID bid)
        {
            const std::optional<BBTEntry> bbt = m_getBBT(bid);
            const size_t totalBlockSize = calcBlockAlignedSize(bbt.value().cb);
            m_file->seekg(bbt.value().bref.ib, std::ios::beg);
            return { utils::readBytes(m_file.get(), totalBlockSize), bbt.value() };
        }

    private:
        core::BID m_bid;
        core::Ref<std::ifstream> m_file;
        GetBBT_t m_getBBT;
        std::vector<SLEntry> m_slentries;
        // uint32_t is a Raw NID
        std::unordered_map<uint32_t, SubNodeBTree> m_subtrees;
        // uint32_t is a Raw NID
        std::unordered_map<uint32_t, DataTree> m_datatrees;
    };

    class NDB
    {
    public:
        NDB(
            std::ifstream& file,
            core::Header header
        )
            :
            m_file(file),
            m_header(header),
            m_rootNBT(InitBTPage(m_header.root.nodeBTreeRootPage, types::PType::NBT)),
            m_rootBBT(InitBTPage(m_header.root.blockBTreeRootPage, types::PType::BBT))
        {
            verify();
        }

        [[nodiscard]] std::unordered_map<types::NIDType, NBTEntry> all(core::NID nid) const
		{
			return m_rootNBT.all(nid);
		}

        template<typename IDType>
        [[nodiscard]] auto get(IDType id) const
        {
            if constexpr (std::is_same_v<IDType, core::NID>)
            {
                return m_rootNBT.get<NBTEntry>(id);
            }
            else if constexpr (std::is_same_v<IDType, core::BID>)
            {
                return m_rootBBT.get<BBTEntry>(id);
            }			
            else
            {
                STORYT_ASSERT(false, "Invalid IDType");
            }
		}

        bool verify()
        {
            STORYT_ASSERT(m_rootNBT.get<NBTEntry>(core::NID_MESSAGE_STORE).has_value(),
                "Cannot be more than 1 Message Store");
            STORYT_ASSERT((m_rootNBT.get<NBTEntry>(core::NID_NAME_TO_ID_MAP).has_value()),
                "[ERROR]");
            STORYT_ASSERT((m_rootNBT.get<NBTEntry>(core::NID_ROOT_FOLDER).has_value()),
                "Cannot be more than 1 Root Folder");
            return true;
        }

        [[nodiscard]] DataTree InitDataTree(core::BREF blockBref, size_t sizeofBlockData) const
        {
            return DataTree(
                core::Ref<std::ifstream>(m_file), 
                [this](const core::BID& bid) { return this->get(bid); },
                blockBref, 
                sizeofBlockData
            );
        }

        [[nodiscard]] SubNodeBTree InitSubNodeBTree(core::BID bid) const
        {
            return SubNodeBTree(
                bid,
                core::Ref<std::ifstream>(m_file),
                [this](const core::BID& bid) { return this->get(bid); }
            );
        }

        [[nodiscard]] BTPage InitBTPage(core::BREF bref, types::PType treeType, int32_t parentCLevel = -1)
        {
            return BTPage::Init(
                utils::readBytes(m_file, bref.ib, BTPage::size),
                bref, 
                m_file,
                parentCLevel
            );
        }

    private:
        std::ifstream& m_file;
        core::Header m_header;
        BTPage m_rootNBT;
        BTPage m_rootBBT;
    };
}

#endif // READER_NDB_H