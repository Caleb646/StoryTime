#include <iostream>
#include <fstream>
#include <stdarg.h>
#include <cassert>
#include <vector>
#include <optional>

#include "utils.h"
#include "types.h"
#include "core.h"
#include "NDB.h"
#include "LTP.h"
#include "Messaging.h"

#ifndef STORYT_PST_READER_H
#define STORYT_PST_READER_H



namespace storyt
{
    class PSTReader
    {
    public:
        explicit PSTReader(std::string path) 
            : m_path(std::move(path)) {}
        ~PSTReader()
        {
            if (m_file.is_open())
            {
                m_file.close();
            }
        }

        void read()
        {
            _open();
            m_ndb.reset(new ndb::NDB(m_file, _readHeader(m_file)));
            m_ltp.reset(new ltp::LTP(core::Ref<const ndb::NDB>{*m_ndb}));
            m_msg.reset(new Messaging(core::Ref<const ndb::NDB>{*m_ndb}, core::Ref<const ltp::LTP>{*m_ltp}));
        }

        template<typename FolderID>
        Folder* getFolder(const FolderID& folderID)
        {
            return m_msg->getFolder(folderID);
        }

    private:
        void _open()
        {
            m_file.open(m_path, std::ios::binary);
            STORYT_ASSERT(m_file.is_open(), "Failed to open file [{}]", m_path.c_str());
        }

        core::Header _readHeader(std::ifstream& file)
        {
            STORYT_ASSERT((file.fail() == false), "Failed to read file [{}]", m_path.c_str());
            file.seekg(0);
            STORYT_ASSERT((file.fail() == false), "Failed to open file [{}]", m_path.c_str());
            const std::vector<types::byte_t> bytes = utils::readBytes(file, 564);

            /**
             * dwMagic (4 bytes): MUST be { 0x21, 0x42, 0x44, 0x4E }
            */
            const uint32_t dwMagic = utils::slice(bytes, 0, 4, 4, utils::toT_l<uint32_t>);
            utils::isIn(dwMagic, { 0x21, 0x42, 0x44, 0x4E });

            /**
             * dwCRCPartial (4 bytes): The 32-bit cyclic redundancy check (CRC) value 
             *  of the 471 bytes of data starting from wMagicClient (0ffset 0x0008)  
            */
            std::vector<types::byte_t> dwCRCPartial = utils::slice(bytes, 4, 8, 4);

            /**
             * wMagicClient (2 bytes): MUST be "{ 0x53, 0x4D }".  
            */
            const uint16_t wMagicClient = utils::slice(bytes, 8, 10, 2, utils::toT_l<uint16_t>);
            utils::isIn(wMagicClient, { 0x53, 0x4D });

            /**
            * wVer (2 bytes):
             1. This value MUST be 14 or 15 if the file is an ANSI PST file 
             2. MUST be greater than 23 if the file is a Unicode PST file. 
                 If the value is 37, it indicates that the file is written by an 
                 Outlook of version that supports Windows Information Protection (WIP). 
                 The data MAY have been protected by WIP.  
            */
            const std::uint16_t wVer = utils::slice(bytes, 10, 12, 2, utils::toT_l<std::uint16_t>);
            STORYT_ASSERT((wVer >= 23), "wVer [{}] was not greater than 23", wVer);

            /*
            * wVerClient (2 bytes): Client file format version. The version that corresponds to 
            * the format described in this document is 19. Creators of a new PST file 
            * based on this document SHOULD initialize this value to 19. 
            */ 
            const std::uint16_t wVerClient = utils::slice(bytes, 12, 14, 2, utils::toT_l<std::uint16_t>);
            STORYT_ASSERT((wVerClient == 19), "wVerClient != 19 but [{}]", wVerClient);

            /* bPlatformCreate (1 types::byte_t): This value MUST be set to 0x01. 
            */ 
            const std::uint8_t bPlatformCreate = utils::slice(bytes, 14, 15, 1, utils::toT_l<std::uint8_t>);
            STORYT_ASSERT((bPlatformCreate == 0x01), "bPlatformCreate != 0x01 but [{}]", bPlatformCreate);

            /* bPlatformAccess (1 types::byte_t): This value MUST be set to 0x01. 
            */
            const std::uint8_t bPlatformAccess = utils::slice(bytes, 15, 16, 1, utils::toT_l<std::uint8_t>);
            STORYT_ASSERT((bPlatformAccess == 0x01), "bPlatformCreate != 0x01 but [{}]", bPlatformAccess);

            /* dwReserved1 (4 bytes): Implementations SHOULD ignore this value and SHOULD NOT modify it. 
              Creators of a new PST file MUST initialize this value to zero.
             */
            utils::slice(bytes, 16, 20, 4, utils::toT_l<std::int32_t>);

            /* dwReserved2 (4 bytes): Implementations SHOULD ignore this value and SHOULD NOT modify it. 
              Creators of a new PST file MUST initialize this value to zero.
            */
            utils::slice(bytes, 20, 24, 4, utils::toT_l<std::int32_t>);

            /*
            * bidUnused (8 bytes Unicode only): Unused padding added when the Unicode PST file format was created. 
            */
            utils::slice(bytes, 24, 32, 8, utils::toT_l<std::int64_t>);

            /*
            * bidNextP (Unicode: 8 bytes; ANSI: 4 bytes): Next page BID. 
            *   Pages have a special counter for allocating bidIndex values. 
            *   The value of bidIndex for BIDs for pages is allocated from this counter. 
            */
            utils::slice(bytes, 32, 40, 8, utils::toT_l<std::int64_t>);

            /*
            * dwUnique (4 bytes): This is a monotonically-increasing value that is modified every time the 
            *  PST file's HEADER structure is modified. The function of this value is to provide a unique value, 
            *  and to ensure that the HEADER CRCs are different after each header modification.
            */
            utils::slice(bytes, 40, 44, 4, utils::toT_l<std::int64_t>);

           /*
           * rgnid[] (128 bytes): A fixed array of 32 NIDs, each corresponding to one of the 
           *  32 possible NID_TYPEs. Different NID_TYPEs can have different 
           *  starting nidIndex values. When a blank PST file is created, these values are initialized 
           *  by NID_TYPE according to the following table. Each of these NIDs indicates the last nidIndex value 
           *  that had been allocated for the corresponding NID_TYPE. When an NID of a particular type is assigned, 
           *  the corresponding slot in rgnid is also incremented by 1. 
           * 
           * NID_TYPE Starting nidIndex 
           *   NID_TYPE_NORMAL_FOLDER 1024 (0x400) 
           *   NID_TYPE_SEARCH_FOLDER 16384 (0x4000) 
           *   NID_TYPE_NORMAL_MESSAGE 65536 (0x10000) 
           *   NID_TYPE_ASSOC_MESSAGE 32768 (0x8000) 
           *   Any other NID_TYPE 1024 (0x400)
           */
           std::vector<types::byte_t> rgnidsBytes = utils::slice(bytes, 44U, 172U, 128U);
           core::NID nids[32];

           for (size_t i = 0; i < 32; i++)
           {
               size_t start = i * 4;
               size_t end = (i + 1) * 4;
               nids[i] = core::NID(utils::slice(rgnidsBytes, start, end, 4ULL));
           }

           for (int64_t i = 0; i < 32; i++)
           {
               uint32_t nidIndex = nids[i].getNIDIndex();
               types::NIDType nidType = nids[i].getNIDType();
               std::string nidTypeString = utils::NIDTypeToString(nidType);

               if ((types::NIDType)i == types::NIDType::NORMAL_FOLDER)
               {
                   STORYT_ASSERT((nidIndex >= 1024),
                       "nidType [{}] nidIndex [{}] was not 1024", nidTypeString.c_str(), nidIndex);
               }
               else if ((types::NIDType)i == types::NIDType::SEARCH_FOLDER)
               {
                   STORYT_ASSERT((nidIndex >= 16384),
                       "nidType [{}] nidIndex [{}] was not 1024", nidTypeString.c_str(), nidIndex);
               }
               else if ((types::NIDType)i == types::NIDType::NORMAL_MESSAGE)
               {
                   STORYT_ASSERT((nidIndex >= 65536),
                       "nidType [{}] nidIndex [{}] was not 65536", nidTypeString.c_str(), nidIndex);
               }
               else
               {
                   STORYT_ASSERT((nidIndex >= 1024),
                       "nidType [{}] nidIndex [{}] was not 1024", nidTypeString.c_str(), nidIndex);
               }
           }


           /*
           * qwUnused (8 bytes): Unused space; MUST be set to zero. Unicode PST file format only. 
           */
           utils::slice(bytes, 172, 180, 8, utils::toT_l<std::int64_t>);

           /*
           * root (Unicode: 72 bytes; ANSI: 40 bytes): A ROOT structure
           */
           std::vector<types::byte_t> root = utils::slice(bytes, 180, 252, 72);

           /*
           * dwAlign (4 bytes): Unused alignment bytes; MUST be set to zero. Unicode PST file format only.
           */
           std::uint32_t dwAlign = utils::slice(bytes, 252, 256, 4, utils::toT_l<std::uint32_t>);
           STORYT_ASSERT((dwAlign == 0), "dwAlign [{}] was not set to zero.", dwAlign);
            
           /*
           * rgbFM (128 bytes): Deprecated FMap. This is no longer used and MUST be filled with 0xFF. 
           *  Readers SHOULD ignore the value of these bytes. 
           
           * rgbFP (128 bytes): Deprecated FPMap. This is no longer used and MUST be filled with 0xFF. 
           *  Readers SHOULD ignore the value of these bytes.
           */
           std::vector<types::byte_t> rgbFM = utils::slice(bytes, 256, 384, 128);
           std::vector<types::byte_t> rgbFP = utils::slice(bytes, 384, 512, 128);

           /*
           * bSentinel (1 byte): MUST be set to 0x80. 
           */
           const uint8_t bSentinel = utils::slice(bytes, 512, 513, 1, utils::toT_l<uint8_t>);
           utils::isIn(bSentinel, { 0x80 });

           /*
           * bCryptMethod (1 byte): Indicates how the data within the PST file is encoded. 
           * MUST be set to one of the pre-defined values described in the following table. 
           * 
           * Value Friendly name Meaning 
           * 0x00 NDB_CRYPT_NONE Data blocks are not encoded. 
           * 0x01 NDB_CRYPT_PERMUTE Encoded with the Permutation algorithm (section 5.1). 
           * 0x02 NDB_CRYPT_CYCLIC Encoded with the Cyclic algorithm (section 5.2). 
           * 0x10 NDB_CRYPT_EDPCRYPTED Encrypted with Windows Information Protection. 
           */
           const uint8_t bCryptMethod = utils::slice(bytes, 513, 514, 1, utils::toT_l<uint8_t>);
           STORYT_ASSERT( (utils::isIn(bCryptMethod, { 0, 1, 2, 0x10 })) , "Invalid Encryption");
           STORYT_VERIFY((bCryptMethod == 0x01)); // Only support Permute currently
           STORYT_INFO("bCryptMethod [{}]", bCryptMethod);
            
           /*
           * rgbReserved (2 bytes): Reserved; MUST be set to zero.
           */
           const uint16_t rgbReserved = utils::slice(bytes, 514, 516, 2, utils::toT_l<uint16_t>);
           utils::isIn(rgbReserved, { 0x00, 0x00 });

           /*
           * bidNextB (Unicode ONLY: 8 bytes): Next BID. 
           *  This value is the monotonic counter that indicates the 
           *  BID to be assigned for the next allocated block. BID values advance in increments of 4. 
           *  For more details, see section 2.2.2.2.
           */
           std::vector<types::byte_t> bidNextB = utils::slice(bytes, 516, 524, 8);

           /*
           * dwCRCFull (4 bytes): The 32-bit CRC value of the 516 bytes of data starting from 
           *  wMagicClient to bidNextB, inclusive. Unicode PST file format only. 
           */
           std::vector<types::byte_t> dwCRCFull = utils::slice(bytes, 524, 528, 4);
           /*
           * rgbReserved2 (3 bytes): Implementations SHOULD ignore this value and SHOULD NOT modify it. 
           * Creators of a new PST MUST initialize this value to zero.
           */
           std::vector<types::byte_t> rgbReserved2 = utils::slice(bytes, 528, 531, 3);

           /*
           * bReserved (1 types::byte_t): Implementations SHOULD ignore this value and SHOULD NOT modify it. 
           *  Creators of a new PST file MUST initialize this value to zero.
           */
           std::vector<types::byte_t> bReserved = utils::slice(bytes, 531, 532, 1);

           /*
           * rgbReserved3 (32 bytes): Implementations SHOULD ignore this value and SHOULD NOT modify it. 
           *  Creators of a new PST MUST initialize this value to zero.
           */
           std::vector<types::byte_t> rgbReserved3 = utils::slice(bytes, 532, 564, 32);

           return core::Header(core::Root::Init(root));
        }

    private:
        std::ifstream m_file;
        std::string m_path;
        std::unique_ptr<ndb::NDB> m_ndb{nullptr};
        std::unique_ptr<ltp::LTP> m_ltp{nullptr};
        std::unique_ptr<Messaging> m_msg{nullptr};
    };
}


#endif // !PST_READER_H