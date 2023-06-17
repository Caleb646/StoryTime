#include <iostream>
#include <fstream>
#include <stdarg.h>
#include <cassert>
#include <vector>

#include "utils.h"
#include "types.h"
#include "core.h"
#include "NDB.h"
#include "LTP.h"

#ifndef PST_READER_H
#define PST_READER_H



namespace reader
{
    class PSTReader
    {
    public:
        PSTReader(const std::string path) : m_path(path) {}
        ~PSTReader()
        {
            if(m_file.is_open())
                m_file.close();
        }

        void read()
        {
            _open(m_path);
            ndb::NDB ndb(m_file, _readHeader(m_file));
            ltp::LTP ltp(ndb);
        }

    private:

        void _open(const std::string path, int mode = std::ios::binary)
        {
            m_file.open(path, mode);
            ASSERT(m_file.is_open(), "[ERROR] Failed to open [file] %s", path.c_str());
        }

        core::Root _readRoot(const std::vector<types::byte_t>& bytes)
        {
            /*
            * dwReserved (4 bytes): Implementations SHOULD ignore this value and SHOULD NOT modify it. 
            *  Creators of a new PST file MUST initialize this value to zero.
            */
            utils::slice(bytes, 0, 4, 4);

            /*
            * ibFileEof (Unicode: 8 bytes; ANSI 4 bytes): The size of the PST file, in bytes. 
            */
            std::uint64_t ibFileEof = utils::slice(bytes, 4, 12, 8, utils::toT_l<std::uint64_t>);
            LOG("[INFO] [file size in bytes] %i", ibFileEof);

            /*
            * ibAMapLast (Unicode: 8 bytes; ANSI 4 bytes): An IB structure (section 2.2.2.3) 
            *  that contains the absolute file offset to the last AMap page of the PST file. 
            */
            std::uint64_t ibAMapLast = utils::slice(bytes, 12, 20, 8, utils::toT_l<uint64_t>);

            /*
            * cbAMapFree (Unicode: 8 bytes; ANSI 4 bytes): The total free space in all AMaps, combined. 
            
            * cbPMapFree (Unicode: 8 bytes; ANSI 4 bytes): The total free space in all PMaps, combined. 
            *  Because the PMap is deprecated, this value SHOULD be zero. Creators of new PST files MUST initialize this value to zero.
            */
            std::vector<types::byte_t> cbAMapFree = utils::slice(bytes, 20, 28, 8);
            std::vector<types::byte_t> cbPMapFree = utils::slice(bytes, 28, 36, 8);

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
            std::vector<types::byte_t> BREFNBT = utils::slice(bytes, 36, 52, 16);
            std::vector<types::byte_t> BREFBBT = utils::slice(bytes, 52, 68, 16);
            std::vector<types::byte_t> fAMapValid = utils::slice(bytes, 68, 69, 1);
            LOG("[INFO] AMapValid state [is] %s", utils::toHexString(fAMapValid).c_str());
            /*
            * bReserved (1 types::byte_t): Implementations SHOULD ignore this value and SHOULD NOT modify it. 
            *  Creators of a new PST file MUST initialize this value to zero.
            */
            std::vector<types::byte_t> bReserved = utils::slice(bytes, 69, 70, 1);

            /*
            * wReserved (2 bytes): Implementations SHOULD ignore this value and SHOULD NOT modify it. 
            *  Creators of a new PST file MUST initialize this value to zero.
            */
            std::vector<types::byte_t> wReserved = utils::slice(bytes, 70, 72, 2);

            core::Root myRoot{};
            myRoot.fileSize = ibFileEof;
            myRoot.nodeBTreeRootPage = core::readBREF(BREFNBT, "Root Page for Node BTree");
            myRoot.blockBTreeRootPage = core::readBREF(BREFBBT, "Root Page for Block BTree");
            myRoot.ibAMapLast = ibAMapLast;
            return myRoot;
        }

        core::Header _readHeader(const std::ifstream& file)
        {
            ASSERT((m_file.fail() == false), "[ERROR] Failed to read [file] %s", m_path.c_str());
            m_file.seekg(0);
            ASSERT((m_file.fail() == false), "[ERROR] Failed to read [file] %s", m_path.c_str());
            std::vector<types::byte_t> bytes = utils::readBytes(m_file, 564);

            /**
             * dwMagic (4 bytes): MUST be "{ 0x21, 0x42, 0x44, 0x4E } ("!BDN")". 
            */
            std::vector<types::byte_t> dwMagic = utils::slice(bytes, 0, 4, 4);
            utils::isEqual(dwMagic, { 0x21, 0x42, 0x44, 0x4E });

            /**
             * dwCRCPartial (4 bytes): The 32-bit cyclic redundancy check (CRC) value 
             *  of the 471 bytes of data starting from wMagicClient (0ffset 0x0008)  
            */
            std::vector<types::byte_t> dwCRCPartial = utils::slice(bytes, 4, 8, 4);

            /**
             * wMagicClient (2 bytes): MUST be "{ 0x53, 0x4D }".  
            */
            std::vector<types::byte_t> wMagicClient = utils::slice(bytes, 8, 10, 2);
            utils::isEqual(wMagicClient, { 0x53, 0x4D });

            /**
            * wVer (2 bytes):
             1. This value MUST be 14 or 15 if the file is an ANSI PST file 
             2. MUST be greater than 23 if the file is a Unicode PST file. 
                 If the value is 37, it indicates that the file is written by an 
                 Outlook of version that supports Windows Information Protection (WIP). 
                 The data MAY have been protected by WIP.  
            */
            std::uint16_t wVer = utils::slice(bytes, 10, 12, 2, utils::toT_l<std::uint16_t>);
            ASSERT((wVer >= 23), "[ERROR] [wVer] %i was not greater than 23", wVer);

            /*
            * wVerClient (2 bytes): Client file format version. The version that corresponds to 
            * the format described in this document is 19. Creators of a new PST file 
            * based on this document SHOULD initialize this value to 19. 
            */ 
            std::uint16_t wVerClient = utils::slice(bytes, 12, 14, 2, utils::toT_l<std::uint16_t>);
            ASSERT((wVerClient == 19), "[ERROR] [wVerClient] != 19 but %i", wVerClient);

            /* bPlatformCreate (1 types::byte_t): This value MUST be set to 0x01. 
            */ 
            std::uint8_t bPlatformCreate = utils::slice(bytes, 14, 15, 1, utils::toT_l<std::uint8_t>);
            ASSERT((bPlatformCreate == 0x01), "[ERROR] [bPlatformCreate] != 0x01 but %i", bPlatformCreate);

            /* bPlatformAccess (1 types::byte_t): This value MUST be set to 0x01. 
            */
            std::uint8_t bPlatformAccess = utils::slice(bytes, 15, 16, 1, utils::toT_l<std::uint8_t>);
            ASSERT((bPlatformAccess == 0x01), "[ERROR] [bPlatformCreate] != 0x01 but %i", bPlatformAccess);

            /* dwReserved1 (4 bytes): Implementations SHOULD ignore this value and SHOULD NOT modify it. 
              Creators of a new PST file MUST initialize this value to zero.
             */
            std::int32_t dwReserved1 = utils::slice(bytes, 16, 20, 4, utils::toT_l<std::int32_t>);

            /* dwReserved2 (4 bytes): Implementations SHOULD ignore this value and SHOULD NOT modify it. 
              Creators of a new PST file MUST initialize this value to zero.
            */
            std::int32_t dwReserved2 = utils::slice(bytes, 20, 24, 4, utils::toT_l<std::int32_t>);

            /*
            * bidUnused (8 bytes Unicode only): Unused padding added when the Unicode PST file format was created. 
            */
            std::int64_t bidUnused = utils::slice(bytes, 24, 32, 8, utils::toT_l<std::int64_t>);

            /*
            * bidNextP (Unicode: 8 bytes; ANSI: 4 bytes): Next page BID. 
            *   Pages have a special counter for allocating bidIndex values. 
            *   The value of bidIndex for BIDs for pages is allocated from this counter. 
            */
            std::int64_t bidNextP = utils::slice(bytes, 32, 40, 8, utils::toT_l<std::int64_t>);

            /*
            * dwUnique (4 bytes): This is a monotonically-increasing value that is modified every time the 
            *  PST file's HEADER structure is modified. The function of this value is to provide a unique value, 
            *  and to ensure that the HEADER CRCs are different after each header modification.
            */
            std::int64_t dwUnique = utils::slice(bytes, 40, 44, 4, utils::toT_l<std::int64_t>);

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
           std::vector<types::byte_t> rgnidsBytes = utils::slice(bytes, 44, 172, 128);
           core::NID nids[32];

           for (int64_t i = 0; i < 32; i++)
           {
               int64_t start = i * 4;
               int64_t end = (i + 1) * 4;
               nids[i] = core::readNID(utils::slice(rgnidsBytes, start, end, (int64_t)4));
           }

           for (int64_t i = 0; i < 32; i++)
           {
               uint32_t nidIndex = nids[i].getNIDIndex();
               types::NIDType nidType = nids[i].getNIDType();
               std::string nidTypeString = utils::NIDTypeToString(nidType);

               if ((types::NIDType)i == types::NIDType::NORMAL_FOLDER)
               {
                   ASSERT((nidIndex >= 1024),
                       "[ERROR] nidType [%s] nidIndex [%i] was not 1024", nidTypeString.c_str(), nidIndex);
               }
               else if ((types::NIDType)i == types::NIDType::SEARCH_FOLDER)
               {
                   ASSERT((nidIndex >= 16384),
                       "[ERROR] nidType [%s] nidIndex [%i] was not 1024", nidTypeString.c_str(), nidIndex);
               }
               else if ((types::NIDType)i == types::NIDType::NORMAL_MESSAGE)
               {
                   ASSERT((nidIndex >= 65536),
                       "[ERROR] nidType [%s] nidIndex [%i] was not 65536", nidTypeString.c_str(), nidIndex);
               }
               else
               {
                   ASSERT((nidIndex >= 1024),
                       "[ERROR] nidType [%s] nidIndex [%i] was not 1024", nidTypeString.c_str(), nidIndex);
               }
           }


           /*
           * qwUnused (8 bytes): Unused space; MUST be set to zero. Unicode PST file format only. 
           */
           std::int64_t qwUnused = utils::slice(bytes, 172, 180, 8, utils::toT_l<std::int64_t>);

           /*
           * root (Unicode: 72 bytes; ANSI: 40 bytes): A ROOT structure
           */
           std::vector<types::byte_t> root = utils::slice(bytes, 180, 252, 72);

           /*
           * dwAlign (4 bytes): Unused alignment bytes; MUST be set to zero. Unicode PST file format only.
           */
           std::uint32_t dwAlign = utils::slice(bytes, 252, 256, 4, utils::toT_l<std::uint32_t>);
           ASSERT((dwAlign == 0), "[ERROR] [dwAlign] %i was not set to zero.", dwAlign);
            
           /*
           * rgbFM (128 bytes): Deprecated FMap. This is no longer used and MUST be filled with 0xFF. 
           *  Readers SHOULD ignore the value of these bytes. 
           
           * rgbFP (128 bytes): Deprecated FPMap. This is no longer used and MUST be filled with 0xFF. 
           *  Readers SHOULD ignore the value of these bytes.
           */
           std::vector<types::byte_t> rgbFM = utils::slice(bytes, 256, 384, 128);
           std::vector<types::byte_t> rgbFP = utils::slice(bytes, 384, 512, 128);

           /*
           * bSentinel (1 types::byte_t): MUST be set to 0x80. 
           */
           std::vector<types::byte_t> bSentinel = utils::slice(bytes, 512, 513, 1);
           utils::isEqual(bSentinel, { 0x80 }, "bSentinel");

           /*
           * bCryptMethod (1 types::byte_t): Indicates how the data within the PST file is encoded. 
           * MUST be set to one of the pre-defined values described in the following table. 
           * 
           * Value Friendly name Meaning 
           * 0x00 NDB_CRYPT_NONE Data blocks are not encoded. 
           * 0x01 NDB_CRYPT_PERMUTE Encoded with the Permutation algorithm (section 5.1). 
           * 0x02 NDB_CRYPT_CYCLIC Encoded with the Cyclic algorithm (section 5.2). 
           * 0x10 NDB_CRYPT_EDPCRYPTED Encrypted with Windows Information Protection. 
           */
           uint8_t bCryptMethod = utils::slice(bytes, 513, 514, 1, utils::toT_l<uint8_t>);
           ASSERT( (utils::isIn(bCryptMethod, { 0, 1, 2, 0x10 })) , "[ERROR] Invalid Encryption");
           //LOG("[bCryptMethod] %s", utils::toHexString(bCryptMethod).c_str());
            
           /*
           * rgbReserved (2 bytes): Reserved; MUST be set to zero.
           */
           std::vector<types::byte_t> rgbReserved = utils::slice(bytes, 514, 516, 2);
           utils::isEqual(rgbReserved, { 0x00, 0x00 }, "rgbReserved");

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
           //utils::isEqual(rgbReserved2, { 0x00, 0x00, 0x00 }, "rgbReserved2");

           /*
           * bReserved (1 types::byte_t): Implementations SHOULD ignore this value and SHOULD NOT modify it. 
           *  Creators of a new PST file MUST initialize this value to zero.
           */
           std::vector<types::byte_t> bReserved = utils::slice(bytes, 531, 532, 1);
           //utils::isEqual(bReserved, { 0x00 }, "bReserved");

           /*
           * rgbReserved3 (32 bytes): Implementations SHOULD ignore this value and SHOULD NOT modify it. 
           *  Creators of a new PST MUST initialize this value to zero.
           */
           std::vector<types::byte_t> rgbReserved3 = utils::slice(bytes, 532, 564, 32);
           //utils::isEqual(rgbReserved3, std::vector<types::byte_t>(32, 0x00), "rgbReserved3");

           core::Header header{};
           header.root = _readRoot(root);

           return header;
        }

    private:
        std::ifstream m_file;
        std::string m_path;
    };
}


#endif // !PST_READER_H