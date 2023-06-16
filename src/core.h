#include <iostream>
#include <stdarg.h>
#include <cassert>
#include <vector>
#include <string>
#include <fstream>

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
            std::uint64_t bidIndex{};
        public:
            bool isInternal() const
            {
                return (bidIndex & 0x02) > 0;
            }

            bool operator==(const BID& other) const
			{
				return bidIndex == other.bidIndex && isInternal() == other.isInternal();
			}
        };

        class NID
        {
        public:
            /*
            * nidType (5 bits): Identifies the type of the node represented by the NID.
            *   The following table specifies a list of values for nidType. However, it
            *   is worth noting that nidType has no meaning to the structures defined in the NDB Layer.
            */
            //int64_t nidType{};

            /*
            * nidIndex (27 bits): The identification portion of the NID.
            */
            uint32_t nidIndex{};
        public:
            bool operator==(const NID& other) const
			{
				return getNIDType() == other.getNIDType() && nidIndex == other.nidIndex;
			}

            types::NIDType getNIDType() const { return utils::getNIDType(nidIndex & 0x1F); }
        };

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
            core::BID bid{};
            bid.bidIndex = utils::toT_l<uint64_t>(bytes);
            return bid;
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
            core::NID nid{};
            nid.nidIndex = utils::toT_l<uint32_t>(bytes);
            return nid;
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