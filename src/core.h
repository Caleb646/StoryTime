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
            enum class PSTType
            {
                NOT_SET = -1,
                NOT_INTERNAL = 0, // not internal (contains data)
                INTERNAL = 1 // internal (contains metadata)
            };
            /**
             * bidIndex (Unicode: 62 bits; ANSI: 30 bits): A monotonically increasing value
             *  that uniquely identifies the BID within the PST file. bidIndex values are assigned
             *  based on the bidNextB value in the HEADER structure (see section 2.2.2.6). The bidIndex
             *  increments by one each time a new BID is assigned.
            */
            std::uint64_t bidIndex{};
        public:
            bool isInternal()
            {
                ASSERT((m_isInternal != -1 && (m_isInternal == 1 || m_isInternal == 0)), "[ERROR] isInternal is in an invalid state %i", m_isInternal);
                return m_isInternal == 1;
            }
            void setIsInternal(int32_t state)
            {
                ASSERT((state == 1 || state == 0), "[ERROR] Invalid state for isInternal %i", state);
                m_isInternal = state;
            }

            bool operator==(const BID& other) const
			{
				return bidIndex == other.bidIndex && m_isInternal == other.m_isInternal;
			}

        private:
            int32_t m_isInternal{ -1 }; // -1 = not set   || 0 = not internal (contains data)     || 1 = internal (contains metadata)
        };

        class NID
        {
        public:
            /*
            * nidType (5 bits): Identifies the type of the node represented by the NID.
            *   The following table specifies a list of values for nidType. However, it
            *   is worth noting that nidType has no meaning to the structures defined in the NDB Layer.
            */
            int64_t nidType{};

            /*
            * nidIndex (27 bits): The identification portion of the NID.
            */
            int64_t nidIndex{};
        public:
            bool operator==(const NID& other) const
			{
				return nidType == other.nidType && nidIndex == other.nidIndex;
			}

            types::NIDType getNIDType() const { return utils::getNIDType(nidType); }
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

        core::BID readBID(const std::vector<types::byte_t>& bytes, bool isBlockBID)
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
            if (isBlockBID)
            {
                // ignore this bit
                int64_t writersOnly = utils::sliceBits<int64_t>(bytes, 0, 1, 1);

                bid.setIsInternal(utils::sliceBits<int32_t>(bytes, 1, 2, 1));
                bid.bidIndex = utils::sliceBits<int64_t>(bytes, 2, 64, 62);

                return bid;
            }
            // Page BID
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
            nid.nidType = utils::sliceBits<int64_t>(bytes, 0, 5, 5);
            nid.nidIndex = utils::sliceBits<int64_t>(bytes, 5, 32, 27);
            // There are unlisted NID_TYPES
            // ASSERT((utils::isIn(nid.nidType, utils::NID_TYPES_VALUES)), "[ERROR] Invalid NID Type %i", (int64_t)nid.nidType);
            return nid;
        }

        core::BREF readBREF(const std::vector<types::byte_t>& bytes, bool isBlockBID, std::string name = "NOT SET")
        {
            ASSERT((bytes.size() == 16), "[ERROR] BREF must be 16 bytes not %i", bytes.size());
            // bid (Unicode: 64 bits; ANSI: 32 bits): A BID structure, as specified in section
            std::vector<types::byte_t> bid = utils::slice(bytes, 0, 8, 8);
            // ib (Unicode: 64 bits; ANSI: 32 bits): An IB structure, as specified in section 2.2.2.3
            core::BREF bref{};
            bref.name = name;
            bref.bid = readBID(bid, isBlockBID);
            bref.ib = utils::slice(bytes, 8, 16, 8, utils::toT_l<int64_t>);

            return bref;
        }
	}
}

#endif // !READER_CORE_H