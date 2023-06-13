#include <iostream>
#include <fstream>
#include <stdarg.h>
#include <cassert>
#include <vector>

#include "utils.h"
#include "types.h"
#include "core.h"
#include "NDB.h"

#ifndef PST_READER_H
#define PST_READER_H



namespace reader
{
    
    //typedef char types::byte_t;

    // valid call log("float %0.2f", 1.0)
    // formats must match vararg types

    struct Header
    {
        std::vector<types::byte_t> root{ 72, '\0' }; // Only accepting unicode format so root should be 72 bytes in size.
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
        core::BREF nodeBTreeRootPage{};
        core::BREF blockBTreeRootPage{};

        /*
        * ibAMapLast (Unicode: 8 bytes; ANSI 4 bytes): An IB structure (section 2.2.2.3)
        *  that contains the absolute file offset to the last AMap page of the PST file.
        */
        std::uint64_t ibAMapLast{};
    };

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
            m_header = _readHeader(m_file);
            m_root = _readRoot(m_header);
            ndb::NDB ndb(m_file, m_root.nodeBTreeRootPage, m_root.blockBTreeRootPage);
        }

    private:

        void _open(const std::string path, int mode = std::ios::binary)
        {
            m_file.open(path, mode);
            ASSERT(m_file.is_open(), "[ERROR] Failed to open [file] %s", path.c_str());
        }

        Root _readRoot(const Header& header)
        {
            /*
            * dwReserved (4 bytes): Implementations SHOULD ignore this value and SHOULD NOT modify it. 
            *  Creators of a new PST file MUST initialize this value to zero.
            */
            utils::slice(header.root, 0, 4, 4);

            /*
            * ibFileEof (Unicode: 8 bytes; ANSI 4 bytes): The size of the PST file, in bytes. 
            */
            std::uint64_t ibFileEof = utils::slice(header.root, 4, 12, 8, utils::toT_l<std::uint64_t>);
            LOG("[INFO] [file size in bytes] %i", ibFileEof);

            /*
            * ibAMapLast (Unicode: 8 bytes; ANSI 4 bytes): An IB structure (section 2.2.2.3) 
            *  that contains the absolute file offset to the last AMap page of the PST file. 
            */
            std::uint64_t ibAMapLast = utils::slice(header.root, 12, 20, 8, utils::toT_l<uint64_t>);

            /*
            * cbAMapFree (Unicode: 8 bytes; ANSI 4 bytes): The total free space in all AMaps, combined. 
            
            * cbPMapFree (Unicode: 8 bytes; ANSI 4 bytes): The total free space in all PMaps, combined. 
            *  Because the PMap is deprecated, this value SHOULD be zero. Creators of new PST files MUST initialize this value to zero.
            */
            std::vector<types::byte_t> cbAMapFree = utils::slice(header.root, 20, 28, 8);
            std::vector<types::byte_t> cbPMapFree = utils::slice(header.root, 28, 36, 8);

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
            std::vector<types::byte_t> BREFNBT = utils::slice(header.root, 36, 52, 16);
            std::vector<types::byte_t> BREFBBT = utils::slice(header.root, 52, 68, 16);
            std::vector<types::byte_t> fAMapValid = utils::slice(header.root, 68, 69, 1);
            LOG("[INFO] AMapValid state [is] %s", utils::toHexString(fAMapValid).c_str());
            /*
            * bReserved (1 types::byte_t): Implementations SHOULD ignore this value and SHOULD NOT modify it. 
            *  Creators of a new PST file MUST initialize this value to zero.
            */
            std::vector<types::byte_t> bReserved = utils::slice(header.root, 69, 70, 1);

            /*
            * wReserved (2 bytes): Implementations SHOULD ignore this value and SHOULD NOT modify it. 
            *  Creators of a new PST file MUST initialize this value to zero.
            */
            std::vector<types::byte_t> wReserved = utils::slice(header.root, 70, 72, 2);

            Root myRoot{};
            myRoot.fileSize = ibFileEof;
            myRoot.nodeBTreeRootPage = core::readBREF(BREFNBT, "Root Page for Node BTree");
            myRoot.blockBTreeRootPage = core::readBREF(BREFBBT, "Root Page for Block BTree");
            myRoot.ibAMapLast = ibAMapLast;
            return myRoot;
        }

        Header _readHeader(const std::ifstream& file)
        {
            /**
             * dwMagic (4 bytes): MUST be "{ 0x21, 0x42, 0x44, 0x4E } ("!BDN")". 
            */
            std::vector<types::byte_t> dwMagic = utils::readBytes(m_file, 4);
            utils::isEqual(dwMagic, { 0x21, 0x42, 0x44, 0x4E });

            /**
             * dwCRCPartial (4 bytes): The 32-bit cyclic redundancy check (CRC) value 
             *  of the 471 bytes of data starting from wMagicClient (0ffset 0x0008)  
            */
            std::vector<types::byte_t> dwCRCPartial = utils::readBytes(m_file, 4);

            /**
             * wMagicClient (2 bytes): MUST be "{ 0x53, 0x4D }".  
            */
            std::vector<types::byte_t> wMagicClient = utils::readBytes(m_file, 2);
            utils::isEqual(wMagicClient, { 0x53, 0x4D });

            /**
            * wVer (2 bytes):
             1. This value MUST be 14 or 15 if the file is an ANSI PST file 
             2. MUST be greater than 23 if the file is a Unicode PST file. 
                 If the value is 37, it indicates that the file is written by an 
                 Outlook of version that supports Windows Information Protection (WIP). 
                 The data MAY have been protected by WIP.  
            */
            std::uint16_t wVer = utils::readBytes(m_file, 2, utils::toT_l<std::uint16_t>);
            ASSERT((wVer >= 23), "[ERROR] [wVer] %i was not greater than 23", wVer);

            /*
            * wVerClient (2 bytes): Client file format version. The version that corresponds to 
            * the format described in this document is 19. Creators of a new PST file 
            * based on this document SHOULD initialize this value to 19. 
            * 
            * bPlatformCreate (1 types::byte_t): This value MUST be set to 0x01. 
            * 
            * bPlatformAccess (1 types::byte_t): This value MUST be set to 0x01. 

            * dwReserved1 (4 bytes): Implementations SHOULD ignore this value and SHOULD NOT modify it. 
              Creators of a new PST file MUST initialize this value to zero.

            * dwReserved2 (4 bytes): Implementations SHOULD ignore this value and SHOULD NOT modify it. 
              Creators of a new PST file MUST initialize this value to zero.
            */
            utils::readBytes(m_file, 12);

            /*
            * bidUnused (8 bytes Unicode only): Unused padding added when the Unicode PST file format was created. 
            */
            utils::readBytes(m_file, 8);

            /*
            * bidNextP (Unicode: 8 bytes; ANSI: 4 bytes): Next page BID. 
            *   Pages have a special counter for allocating bidIndex values. 
            *   The value of bidIndex for BIDs for pages is allocated from this counter. 
            */
            utils::readBytes(m_file, 8);

            /*
            * dwUnique (4 bytes): This is a monotonically-increasing value that is modified every time the 
            *  PST file's HEADER structure is modified. The function of this value is to provide a unique value, 
            *  and to ensure that the HEADER CRCs are different after each header modification.
            */

            utils::readBytes(m_file, 4);

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
           std::vector<types::byte_t> rgnids = utils::readBytes(m_file, 128);

           /*
           * qwUnused (8 bytes): Unused space; MUST be set to zero. Unicode PST file format only. 
           */
           utils::readBytes(m_file, 8);

           /*
           * root (Unicode: 72 bytes; ANSI: 40 bytes): A ROOT structure
           */
           std::vector<types::byte_t> root = utils::readBytes(m_file, 72);

           /*
           * dwAlign (4 bytes): Unused alignment bytes; MUST be set to zero. Unicode PST file format only.
           */
           std::uint32_t dwAlign = utils::readBytes(m_file, 4, utils::toT_l<std::uint32_t>);
           ASSERT((dwAlign == 0), "[ERROR] [dwAlign] %i was not set to zero.", dwAlign);
            
           /*
           * rgbFM (128 bytes): Deprecated FMap. This is no longer used and MUST be filled with 0xFF. 
           *  Readers SHOULD ignore the value of these bytes. 
           
           * rgbFP (128 bytes): Deprecated FPMap. This is no longer used and MUST be filled with 0xFF. 
           *  Readers SHOULD ignore the value of these bytes.
           */

           std::vector<types::byte_t> rgb = utils::readBytes(m_file, 256);
           //std::cout << toHexString(rgb) << "\n\n" << std::endl;

           /*
           * bSentinel (1 types::byte_t): MUST be set to 0x80. 
           */
           std::vector<types::byte_t> bSentinel = utils::readBytes(m_file, 1);
           //std::cout << toHexString(bSentinel) << std::endl;
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
           std::vector<types::byte_t> bCryptMethod = utils::readBytes(m_file, 1);
           LOG("[bCryptMethod] %s", utils::toHexString(bCryptMethod).c_str());
            
           /*
           * rgbReserved (2 bytes): Reserved; MUST be set to zero.
           */
           std::vector<types::byte_t> rgbReserved = utils::readBytes(m_file, 2);
           utils::isEqual(rgbReserved, { 0x00, 0x00 }, "rgbReserved");

           /*
           * bidNextB (Unicode ONLY: 8 bytes): Next BID. 
           *  This value is the monotonic counter that indicates the 
           *  BID to be assigned for the next allocated block. BID values advance in increments of 4. 
           *  For more details, see section 2.2.2.2.
           */
           utils::readBytes(m_file, 8);

           /*
           * dwCRCFull (4 bytes): The 32-bit CRC value of the 516 bytes of data starting from 
           *  wMagicClient to bidNextB, inclusive. Unicode PST file format only. 
           */

           /*
           * rgbReserved2 (3 bytes): Implementations SHOULD ignore this value and SHOULD NOT modify it. 
           * Creators of a new PST MUST initialize this value to zero.
           */
           std::vector<types::byte_t> rgbReserved2 = utils::readBytes(m_file, 3);
           //isEqual(rgbReserved2, { 0x00, 0x00, 0x00 }, "rgbReserved2");

           /*
           * bReserved (1 types::byte_t): Implementations SHOULD ignore this value and SHOULD NOT modify it. 
           *  Creators of a new PST file MUST initialize this value to zero.
           */
           std::vector<types::byte_t> bReserved = utils::readBytes(m_file, 1);
           //isEqual(bReserved, { 0x00 }, "bReserved");

           /*
           * rgbReserved3 (32 bytes): Implementations SHOULD ignore this value and SHOULD NOT modify it. 
           *  Creators of a new PST MUST initialize this value to zero.
           */
           std::vector<types::byte_t> rgbReserved3 = utils::readBytes(m_file, 32);
           std::vector<types::byte_t> B(32, 0x00);
           //isEqual(rgbReserved3, B, "rgbReserved3");

           Header header{};
           header.root = root;

           return header;
        }

    private:
        std::ifstream m_file;
        std::string m_path;
        std::string m_encoding{ "UNICODE" };

        Header m_header{};
        Root m_root{};
    };
}


#endif // !PST_READER_H