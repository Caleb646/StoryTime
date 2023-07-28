#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <fstream>
#include <utility>
#include <algorithm>
#include <type_traits>

#include "types.h"
#include "utils.h"

#ifndef READER_CORE_H
#define READER_CORE_H


namespace reader::core {
    /**
    * @brief = Every block allocated in the PST file is identified using the BID structure.
    *  This structure varies in size according the format of the file. In the case of ANSI files,
    *  the structure is a 32-bit unsigned value, while in Unicode files it is a 64-bit unsigned long.
    *  In addition, there are two types of BIDs:
    *      1. BIDs used in the context of Pages (section 2.2.2.7)
    *          use all of the bits of the structure (below) and are incremented by 1.
    *      2. Block BIDs (section 2.2.2.8)
    *          reserve the two least significant bits for flags (see below). As a result these increment
    *          by 4 each time a new one is assigned.
    * @param bidIndex (Unicode: 62 or 64 bits): A monotonically increasing value
    *       that uniquely identifies the BID within the PST file. bidIndex values are assigned
    *       based on the bidNextB value in the HEADER structure (see section 2.2.2.6). The bidIndex
    *       increments by one each time a new BID is assigned.
    * @param m_isInternal (Unicode: 2 bits; ANSI: 2 bits)
    */
    class BID
    {
    public:
        BID() = default;
        explicit BID(uint64_t _bid) : m_bid(_bid)
        {
			_init();
		}
        explicit BID(const std::vector<types::byte_t>& bytes)
            : m_bid(utils::toT_l<decltype(m_bid)>(bytes)) 
        {
			_init();
        }

        [[nodiscard]] bool isInternal() const
        {
            STORYT_ASSERT((m_isSetup == true), "BID not setup");
            return (m_bid & 0x02U) > 0U;
        }

        [[nodiscard]] uint64_t getBidIndex() const
		{
            STORYT_ASSERT((m_isSetup == true), "BID not setup");
			return (m_bid >> 2U) << 2U;
		}

        [[nodiscard]] uint64_t getBidRaw() const
        {
            STORYT_ASSERT((m_isSetup == true), "BID not setup");
            return m_bid;
        }

        template<typename T>
        bool operator==(const T& other) const
		{
            if constexpr (std::is_same_v<T, BID>)
            {
                return getBidRaw() == other.getBidRaw();
            }
            else
            {
                return getBidRaw() == other;
            }
		}

        template<typename T>
        bool operator<(const T& other) const
        {
            if constexpr (std::is_same_v<T, BID>)
            {
                return getBidRaw() < other.getBidRaw();
            }
            else
            {
                return this->getBidRaw() < other;
            }
        }
        template<typename T>
        bool operator>(const T& other) const
        {
            return !(*this < other);
        }

        [[nodiscard]] bool isSetup() const { return m_isSetup; }

        static constexpr size_t id()
        {
            return 4;
        }

    private:
        void _init()
        {
            m_isSetup = true;
        }
    private:
        /**
        * bidIndex (Unicode: 62 bits): A monotonically increasing value
        *  that uniquely identifies the BID within the PST file. bidIndex values are assigned
        *  based on the bidNextB value in the HEADER structure. The bidIndex
        *  increments by one each time a new BID is assigned.
        */
        uint64_t m_bid{};
        bool m_isSetup{ false };
    };

    class NID
    {
    public:
        NID() = default;
        constexpr explicit NID(uint32_t _nid) : m_nid(_nid) {}
        explicit NID(const std::vector<types::byte_t>& bytes) 
        {
            utils::ByteView view(bytes);
            m_nid = view.read<uint32_t>(4);
        }

        template<typename T>
        bool operator==(const T& other) const
		{
            if constexpr (std::is_same_v<T, NID>)
            {
                return getNIDRaw() == other.getNIDRaw();
            }
            else
            {
                return getNIDRaw() == other;
            }
		}
        template<typename T>
        bool operator<(const T& other) const
        {
            if constexpr (std::is_same_v<T, NID>)
            {
                return getNIDRaw() < other.getNIDRaw();
            }
            else
            {
                return getNIDRaw() < other;
            }
        }
        template<typename T>
        bool operator>(const T& other) const
        {
            return !(*this < other);
        }

        static constexpr size_t id()
        {
			return 5;
		}

        /*
        * @brief = nidType (5 bits): Identifies the type of the node represented by the NID.
        *   The following table specifies a list of values for nidType. However, it
        *   is worth noting that nidType has no meaning to the structures defined in the NDB Layer.
        */
        [[nodiscard]] types::NIDType getNIDType() const
        { 
            return utils::getNIDType(m_nid & 0x1FU);
        }

        [[nodiscard]] uint32_t getNIDIndex() const
        { 
            static_assert(std::is_same_v<decltype(m_nid), uint32_t>);
            return (m_nid >> 5U) << 5U;
        }

        [[nodiscard]] uint32_t getNIDRaw() const
        {
            return m_nid;
        }
    private:
        /// nid is represented by both: nidIndex and nidType
        /// nidIndex (27 bits): The identification portion of the NID.
        /// nidType (5 bits): Identifies the type of the node represented by the NID.
        uint32_t m_nid{};
    };

    /*
    * 
    * 
    * Special NID Values
    * 
    */
    constexpr NID NID_MESSAGE_STORE(0x21);
    constexpr NID NID_NAME_TO_ID_MAP(0x61);
    constexpr NID NID_NORMAL_FOLDER_TEMPLATE(0xA1);
    constexpr NID NID_SEARCH_FOLDER_TEMPLATE(0xC1);
    constexpr NID NID_ROOT_FOLDER(0x122);
    constexpr NID NID_SEARCH_MANAGEMENT_QUEUE(0x1E1);

    // The BREF is a record that maps a BID to its absolute file offset location. 
    struct BREF
    {

        BID bid{};
        /**
            * The IB (Byte Index) is used to represent an absolute offset within the PST file with
            *  respect to the beginning of the file. The IB is a simple unsigned integer value and
            *  is 64 bits in Unicode versions and 32 bits in ANSI versions.
        */
        /// The IB (Byte Index) is used to represent an absolute offset within the PST file with
        /// respect to the beginning of the file.The IB is a simple unsigned integer value and
        /// is 64 bits in Unicode versions and 32 bits in ANSI versions.
        std::uint64_t ib{};

        BREF() = default;

        explicit BREF(const std::vector<types::byte_t>& bytes)
        {
            STORYT_ASSERT((bytes.size() == 16), "BREF must be 16 bytes not [{}]", bytes.size());
            utils::ByteView view(bytes);
            bid = view.entry<BID>(8);
            ib = view.read<uint64_t>(8);
        }

        explicit BREF(uint64_t bid_, uint64_t ib_)
            : bid(bid_), ib(ib_) {}
    };

    struct Root
    {
        std::uint64_t fileSize{0};

        /*
        * BREFNBT(Unicode: 16 bytes; ANSI: 8 bytes) : A BREF structure(section 2.2.2.4) that references the
        *   root page of the Node BTree(NBT).
        *   The NBT contains references to all of the accessible nodes in the PST file.
        *   Its BTree implementation allows for efficient searches to locate any specific node.
        *   Each node reference is represented using a set of four properties that includes its NID,
        *   parent NID, data BID, and subnode BID. The data BID points to the block that contains the
        *   data associated with the node, and the subnode BID points to the block that contains references
        *   to subnodes of this node. Top-level NIDs are unique across the PST and are searchable from the NBT.
        *   Subnode NIDs are only unique within a node and are not searchable (or found) from the NBT.
        *   The parent NID is an optimization for the higher layers and has no meaning for the NDB Layer.
        *
        * BREFBBT(Unicode : 16 bytes; ANSI: 8 bytes) : A BREF
        *   structure that references the root page of the Block BTree(BBT).
        *   The BBT contains references to all of the data blocks of the PST file. Its BTree
        *   implementation allows for efficient searches to locate any specific block. A block
        *   reference is represented using a set of four properties, which includes its BID, IB, CB,
        *   and CREF. The IB is the offset within the file where the block is located. The CB is the
        *   count of bytes stored within the block. The CREF is the count of references to the data stored within the block.
        */
        core::BREF nodeBTreeRootPage;
        core::BREF blockBTreeRootPage;

        /*
        * ibAMapLast (Unicode: 8 bytes; ANSI 4 bytes): An IB structure (section 2.2.2.3)
        *  that contains the absolute file offset to the last AMap page of the PST file.
        */
        std::uint64_t ibAMapLast{};

        static Root Init(const std::vector<types::byte_t>& bytes)
        {
            STORYT_ASSERT((bytes.size() == 72), "bytes.size() [{}] != Root Size [72]", bytes.size());
            utils::ByteView view(bytes);
            /*
            * dwReserved (4 bytes): Implementations SHOULD ignore this value and SHOULD NOT modify it.
            *  Creators of a new PST file MUST initialize this value to zero.
            */
            view.skip(4);

            /*
            * ibFileEof (Unicode: 8 bytes; ANSI 4 bytes): The size of the PST file, in bytes.
            */
            std::uint64_t ibFileEof = view.read<uint64_t>(8); 
            STORYT_INFO("file size in bytes [{}]", ibFileEof);

            /*
            * ibAMapLast (Unicode: 8 bytes; ANSI 4 bytes): An IB structure (section 2.2.2.3)
            *  that contains the absolute file offset to the last AMap page of the PST file.
            */
            std::uint64_t ibAMapLast = view.read<uint64_t>(8); // utils::slice(bytes, 12, 20, 8, utils::toT_l<uint64_t>);

            /*
            * cbAMapFree (Unicode: 8 bytes; ANSI 4 bytes): The total free space in all AMaps, combined.

            * cbPMapFree (Unicode: 8 bytes; ANSI 4 bytes): The total free space in all PMaps, combined.
            *  Because the PMap is deprecated, this value SHOULD be zero. Creators of new PST files MUST initialize this value to zero.
            */
            std::vector<types::byte_t> cbAMapFree = view.read(8); //utils::slice(bytes, 20, 28, 8);
            std::vector<types::byte_t> cbPMapFree = view.read(8); //utils::slice(bytes, 28, 36, 8);

            /*
            * BREFNBT (Unicode: 16 bytes; ANSI: 8 bytes): A BREF structure (section 2.2.2.4) that references the
            *  root page of the Node BTree (NBT).

            * BREFBBT (Unicode: 16 bytes; ANSI: 8 bytes): A BREF
            *  structure that references the root page of the Block BTree (BBT).

            * fAMapValid (1 types::byte_t): Indicates whether all of the AMaps in this PST file are valid.
            *  For more details, see section 2.6.1.3.7. This value MUST be set to one of the pre-defined values specified in the following table.
            *
            * Value Friendly name Meaning
            * 0x00 INVALID_AMAP One or more AMaps in the PST are INVALID
            * 0x01 VALID_AMAP1 Deprecated. Implementations SHOULD NOT use this The AMaps are VALID.
            * 0x02 VALID_AMAP2 value. The AMaps are VALID.
            */
            const BREF BREFNBT = view.entry<BREF>(16); // utils::slice(bytes, 36, 52, 16);
            const BREF BREFBBT = view.entry<BREF>(16);  //utils::slice(bytes, 52, 68, 16);
            const uint8_t fAMapValid = view.read<uint8_t>(1); // utils::slice(bytes, 68, 69, 1, utils::toT_l<uint8_t>);
            STORYT_ASSERT((fAMapValid == 0x02), "Invalid AMaps");
            /*
            * bReserved (1 types::byte_t): Implementations SHOULD ignore this value and SHOULD NOT modify it.
            *  Creators of a new PST file MUST initialize this value to zero.
            */
            const uint8_t bReserved = view.read<uint8_t>(1);// utils::slice(bytes, 69, 70, 1);

            /*
            * wReserved (2 bytes): Implementations SHOULD ignore this value and SHOULD NOT modify it.
            *  Creators of a new PST file MUST initialize this value to zero.
            */
            const uint16_t wReserved = view.read<uint16_t>(2);// utils::slice(bytes, 70, 72, 2);

            return Root(BREFNBT, ibFileEof, BREFBBT, ibAMapLast);
        }

        explicit Root(BREF nodeRootPage, uint64_t fileSize_, BREF blockRootPage, uint64_t ibMap)
            : nodeBTreeRootPage(nodeRootPage), blockBTreeRootPage(blockRootPage), fileSize(fileSize_),
            ibAMapLast(ibMap) {}
    };

    struct Header
    {
        const Root root;

        explicit Header(Root&& root)
            : root(root) {}
    };

    template<typename T>
    class Ref
    {
    public :
        explicit Ref(T& t) : m_ref(t) {}
        T* operator->() { return &m_ref.get(); }
        T& get() { return m_ref.get(); }
    private:
        std::reference_wrapper<T> m_ref;
    };
} // end namespace reader::core

#endif // !READER_CORE_H