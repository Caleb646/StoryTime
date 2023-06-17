#include <iostream>
#include <stdarg.h>
#include <cassert>
#include <vector>
#include <string>
#include <fstream>
#include <type_traits>

#include "types.h"
#include "utils.h"

#ifndef READER_CORE_H
#define READER_CORE_H


namespace reader {
	namespace core {
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
            /**
             * bidIndex (Unicode: 62 bits; ANSI: 30 bits): A monotonically increasing value
             *  that uniquely identifies the BID within the PST file. bidIndex values are assigned
             *  based on the bidNextB value in the HEADER structure (see section 2.2.2.6). The bidIndex
             *  increments by one each time a new BID is assigned.
            */
        public:
            BID() = default;
            BID(const std::vector<types::byte_t>& bytes)
                : m_bid(utils::toT_l<decltype(m_bid)>(bytes)) 
            {
			    _init();
            }

            bool isInternal() const
            {
                ASSERT((m_isSetup == true), "[ERROR] BID not setup");
                return (m_bid & 0x02) > 0;
            }

            uint64_t getBidIndex() const
			{
                ASSERT((m_isSetup == true), "[ERROR] BID not setup");
				return (m_bid >> 2) << 2;
                //return m_bid;
			}

            uint64_t getBidRaw() const
            {
                ASSERT((m_isSetup == true), "[ERROR] BID not setup");
                return m_bid;
            }

            bool operator==(const BID& other) const
			{
                ASSERT((m_isSetup == true), "[ERROR] BID not setup");
                //return getBidIndex() == other.getBidIndex();
                return getBidRaw() == other.getBidRaw();
			}
        private:
            void _init()
            {
                m_isSetup = true;
            }
        private:
            uint64_t m_bid{};
            bool m_isSetup{ false };
        };

        class NID
        {
        public:
            NID() = default;
            NID(uint32_t _nid) : m_nid(_nid) {}
            NID(const std::vector<types::byte_t>& bytes) 
                : m_nid(utils::toT_l<decltype(m_nid)>(bytes)) {}

            bool operator==(const NID& other) const
			{
				return getNIDRaw() == other.getNIDRaw();
			}

            /*
            * @brief = nidType (5 bits): Identifies the type of the node represented by the NID.
            *   The following table specifies a list of values for nidType. However, it
            *   is worth noting that nidType has no meaning to the structures defined in the NDB Layer.
            */
            types::NIDType getNIDType() const 
            { 
                return utils::getNIDType(m_nid & 0x1F);
            }

            uint32_t getNIDIndex() const 
            { 
                static_assert(std::is_same_v<decltype(m_nid), uint32_t> == true);
                return (m_nid >> 5) << 5;
            }

            uint32_t getNIDRaw() const
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
        NID NID_MESSAGE_STORE(0x21);
        NID NID_NAME_TO_ID_MAP(0x61);
        NID NID_NORMAL_FOLDER_TEMPLATE(0xA1);
        NID NID_SEARCH_FOLDER_TEMPLATE(0xC1);
        NID NID_ROOT_FOLDER(0x122);
        NID NID_SEARCH_MANAGEMENT_QUEUE(0x1E1);

        // The BREF is a record that maps a BID to its absolute file offset location. 
        struct BREF
        {

            BID bid{};
            /**
             * The IB (Byte Index) is used to represent an absolute offset within the PST file with
             *  respect to the beginning of the file. The IB is a simple unsigned integer value and
             *  is 64 bits in Unicode versions and 32 bits in ANSI versions.
            */
            std::uint64_t ib{};
            std::string name{"NOT SET"};
        };

        core::BID readBID(const std::vector<types::byte_t>& bytes)
        {
            ASSERT((bytes.size() == 8), "[ERROR] BID must be 8 bytes not %i", bytes.size());

            /**
            * There are two types of BIDs
            *
            * BIDs used in the context of Pages (section 2.2.2.7) use all of the bits of the structure (below) and are incremented by 1. 2.

            * Block BIDs (section 2.2.2.8) reserve the two least significant bits for flags (see below). As
                a result these increment by 4 each time a new one is assigned.
            */
            return BID(bytes);
        }

        core::NID readNID(const std::vector<types::byte_t>& bytes)
        {
            /*
            * nidType (5 bits): Identifies the type of the node represented by the NID.
            *   The following table specifies a list of values for nidType. However, it is worth
            *   noting that nidType has no meaning to the structures defined in the NDB Layer.
            *
            * nidIndex (27 bits): The identification portion of the NID.
            */

            ASSERT((bytes.size() == 4), "[ERROR] NID must be 4 bytes not %i", bytes.size());
            return NID(bytes);
        }

        core::BREF readBREF(const std::vector<types::byte_t>& bytes, std::string name = "NOT SET")
        {
            ASSERT((bytes.size() == 16), "[ERROR] BREF must be 16 bytes not %i", bytes.size());
            // bid (Unicode: 64 bits; ANSI: 32 bits): A BID structure, as specified in section
            std::vector<types::byte_t> bid = utils::slice(bytes, 0, 8, 8);
            // ib (Unicode: 64 bits; ANSI: 32 bits): An IB structure, as specified in section 2.2.2.3
            core::BREF bref{};
            bref.name = name;
            bref.bid = readBID(bid);
            bref.ib = utils::slice(bytes, 8, 16, 8, utils::toT_l<decltype(bref.ib)>);

            return bref;
        }

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
            core::BREF nodeBTreeRootPage{};
            core::BREF blockBTreeRootPage{};

            /*
            * ibAMapLast (Unicode: 8 bytes; ANSI 4 bytes): An IB structure (section 2.2.2.3)
            *  that contains the absolute file offset to the last AMap page of the PST file.
            */
            std::uint64_t ibAMapLast{};
        };

        struct Header
        {
            Root root{};
        };
	}
}

#endif // !READER_CORE_H