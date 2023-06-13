#include <iostream>
#include <fstream>
#include <stdarg.h>
#include <cassert>
#include <vector>
#include <bitset>

#define LOG(fmt, ...) reader::log(__LINE__, fmt, __VA_ARGS__)
#define ASSERT(con, fmt, ...) do {\
                                if(!con)\
                                    LOG(fmt, __VA_ARGS__);\
                                    assert(con);\
                            }\
                         while(false);\

#define NOTSET -1

//#define ASSERT(con) assert(con)

namespace reader
{
    typedef unsigned char byte_t;
    //typedef char byte_t;

    // valid call log("float %0.2f", 1.0)
    // formats must match vararg types
    void log(int lineNumber, const char* fmt, ...)
    {
        char buffer[1000]{};
        va_list vararglist;
        va_start(vararglist, fmt);
        vprintf(fmt, vararglist);
        va_end(vararglist);
        printf(" at line [%i]\n", lineNumber);
    }

    std::string toHex(const char byte)
    {
        static constexpr char HEXITS[] = "0123456789ABCDEF";
        std::string str(4, '\0');
        auto k = str.begin();
        *k++ = '0';
        *k++ = 'x';
        *k++ = HEXITS[byte >> 4];
        *k++ = HEXITS[byte & 0x0F];
        return str;
    }

    std::vector<std::string> toHexVector(const std::vector<byte_t>& bytes)
    {
        std::vector<std::string> hex(bytes.size());
        for (auto byte_t : bytes)
        {
            hex.push_back(toHex(byte_t));
        }
        return hex;
    }

    std::string toHexString(const std::vector<byte_t>& bytes, const char delimiter = ' ')
    {
        static constexpr char HEXITS[] = "0123456789ABCDEF";

        std::string str(5 * bytes.size(), '\0');
        auto k = str.begin();

        for (auto c : bytes) {
            *k++ = '0';
            *k++ = 'x';
            *k++ = HEXITS[c >> 4];
            *k++ = HEXITS[c & 0x0F];
            *k++ = delimiter;
        }
        return str;
    }

    std::vector<byte_t> pad(const std::vector<byte_t>& bytes, int64_t bytesToAdd)
    {
        std::vector<byte_t> result = bytes;
        if (bytesToAdd <= 0) return result;

        for (int64_t i = 0; i < bytesToAdd; i++)
        {
            result.push_back(0);
        }
        return result;
    }

    template<typename T = int>
    T toT_l(const std::vector<byte_t>& b)
    {
        auto bytes = pad(b, sizeof(T) - b.size());
        ASSERT((sizeof(T) == bytes.size()), "[ERROR] toT_l T not [%i].", sizeof(T));
        if constexpr (sizeof(T) == 1)
        {
            //static_assert(sizeof(T) == 1);
            return static_cast<T>
                (
                    static_cast<T>(bytes[0])
                );
        }
        else if constexpr (sizeof(T) == 2)
        {
            //static_assert(sizeof(T) == 2);
            return static_cast<T>
                (
                    static_cast<T>(bytes[1]) << 8 |
                    static_cast<T>(bytes[0])
                );
        }
        else if constexpr (sizeof(T) == 3)
        {
            //static_assert(sizeof(T) == 3);
            return static_cast<T>
                (
                    static_cast<T>(bytes[2]) << 16 |
                    static_cast<T>(bytes[1]) << 8 |
                    static_cast<T>(bytes[0])
                );
        }
        else if constexpr (sizeof(T) == 4)
        {
            //static_assert(sizeof(T) == 4);
            return static_cast<T>
                (
                    static_cast<T>(bytes[3]) << 24 |
                    static_cast<T>(bytes[2]) << 16 |
                    static_cast<T>(bytes[1]) << 8 |
                    static_cast<T>(bytes[0])
                );
        }
        else if constexpr (sizeof(T) == 8)
        {
            static_assert(sizeof(T) == 8);
            return static_cast<T>
                (
                    static_cast<T>(bytes[7]) << 56 |
                    static_cast<T>(bytes[6]) << 48 |
                    static_cast<T>(bytes[5]) << 40 |
                    static_cast<T>(bytes[4]) << 32 |
                    static_cast<T>(bytes[3]) << 24 |
                    static_cast<T>(bytes[2]) << 16 |
                    static_cast<T>(bytes[1]) << 8  |
                    static_cast<T>(bytes[0])
                );
        }
        ASSERT(false, "[ERROR] [bytes] &s vector can only be of size 2 to 4.", toHexString(bytes).c_str());
        return -1;
    }

    //std::uint64_t toUInt_64b(const std::vector<byte_t>& bytes)
    //{
    //    ASSERT((bytes.size() == 8), "[ERROR] toInt64 bytes buffer must be of size 8 [not] %i.", bytes.size());
    //    return  (((std::uint64_t)((std::uint8_t*)bytes[7])) << 0) +
    //        (((std::uint64_t)((std::uint8_t*)bytes[6])) << 8) +
    //        (((std::uint64_t)((std::uint8_t*)bytes[5])) << 16) +
    //        (((std::uint64_t)((std::uint8_t*)bytes[4])) << 24) +
    //        (((std::uint64_t)((std::uint8_t*)bytes[3])) << 32) +
    //        (((std::uint64_t)((std::uint8_t*)bytes[2])) << 40) +
    //        (((std::uint64_t)((std::uint8_t*)bytes[1])) << 48) +
    //        (((std::uint64_t)((std::uint8_t*)bytes[0])) << 56);
    //}
    //template<typename T>
    //T toT_l(const std::vector<byte_t>& byte)
    //{
    //    ASSERT((byte.size() == 1), "[ERROR] toT_l bytes buffer must be of size 8 not [%i].", byte.size());
    //    ASSERT((sizeof(T) == 1), "[ERROR] toT_l T not [%i].", sizeof(T));
    //    return  (T) byte[0];
    //}

    // For Windows
    std::uint64_t toUInt_64l(const std::vector<byte_t>& bytes)
    {
        ASSERT((bytes.size() == 8), "[ERROR] toInt64 bytes buffer must be of size 8 [not] %i.", bytes.size());
        return  (((std::uint64_t) ( (std::uint8_t*) bytes[0]) ) << 0)  +
                (((std::uint64_t) ( (std::uint8_t*) bytes[1]) ) << 8)  +
                (((std::uint64_t) ( (std::uint8_t*) bytes[2]) ) << 16) +
                (((std::uint64_t) ( (std::uint8_t*) bytes[3]) ) << 24) +
                (((std::uint64_t) ( (std::uint8_t*) bytes[4]) ) << 32) +
                (((std::uint64_t) ( (std::uint8_t*) bytes[5]) ) << 40) +
                (((std::uint64_t) ( (std::uint8_t*) bytes[6]) ) << 48) +
                (((std::uint64_t) ( (std::uint8_t*) bytes[7]) ) << 56);
    }

    template<std::size_t BITS>
    std::bitset<BITS> bytesToBits(const std::vector<byte_t>& bytes)
    {
        ASSERT((bytes.size() * 8 == BITS), "[ERROR] Bytes vector is of incorrect size %i instead of %i", bytes.size(), BITS);
        std::bitset<BITS> bits{};
        for (int i = 0; i < bytes.size(); i++)
        {
            bits.set( (i * 8) + 0, bytes[i] & 0x80 );
            bits.set( (i * 8) + 1, bytes[i] & 0x40 );
            bits.set( (i * 8) + 2, bytes[i] & 0x20 );
            bits.set( (i * 8) + 3, bytes[i] & 0x10 );
            bits.set( (i * 8) + 4, bytes[i] & 0x08 );
            bits.set( (i * 8) + 5, bytes[i] & 0x04 );
            bits.set( (i * 8) + 6, bytes[i] & 0x02 );
            bits.set( (i * 8) + 7, bytes[i] & 0x01 );
        }
        return bits;    
    }

    bool isEqual(std::vector<byte_t> a, std::vector<byte_t> b, std::string name = "NOT SET", bool shouldFail = true)
    {
        bool e = a == b;
        const char* msg = "[ERROR] [%s] [a] %s != [b] %s";

        if (shouldFail)
        {
            ASSERT(e, msg, name.c_str(), toHexString(a).c_str(), toHexString(b).c_str());
        }
        
        return e;
    }

    bool isIn(int64_t a, std::vector<int64_t> b)
    {
        bool found = false;
        for (auto item : b)
        {
            found |= item == a;
        }
        return found;
    }

    template<typename T>
    std::vector<byte_t> slice(const std::vector<byte_t>& v, T startOffset, T endOffset)
    {
        return std::vector<byte_t>(v.begin() + startOffset, v.begin() + startOffset + (endOffset - startOffset));
    }

    template<class T>
    T slice(const std::vector<byte_t>& v, int startOffset, int endOffset, T(*convert)(const std::vector<byte_t>&))
    {
        std::vector<byte_t> res = slice(v, startOffset, endOffset);
        return convert(res);
    }

    template<std::size_t SRC_BITS, std::size_t DST_BITS>
    std::bitset<DST_BITS> slice(const std::bitset<SRC_BITS>& bits, int startBit, int numberOfBits)
    {
        std::bitset<DST_BITS> dst;
        for (int i = 0; i < numberOfBits; i++)
        {
            dst.set(i, bits[i + startBit]);
        }
        return dst;
    }

    enum class IDType {
        invalid,
        bid,
        nid
    };

    /**
     * Every block allocated in the PST file is identified using the BID structure. 
     *  This structure varies in size according the format of the file. In the case of ANSI files, 
     *  the structure is a 32-bit unsigned value, while in Unicode files it is a 64-bit unsigned long. 
     *  In addition, there are two types of BIDs: 
     *      1. BIDs used in the context of Pages (section 2.2.2.7) 
     *          use all of the bits of the structure (below) and are incremented by 1. 
     *      2. Block BIDs (section 2.2.2.8) 
     *          reserve the two least significant bits for flags (see below). As a result these increment 
     *          by 4 each time a new one is assigned.
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
            ASSERT((m_isInternal != -1 && (m_isInternal == 1 || m_isInternal == 0) ), "[ERROR] isInternal is in an invalid state %i", m_isInternal);
            return m_isInternal == 1;
        }
        void setIsInternal(int32_t state)
        {
            ASSERT( (state == 1 || state == 0), "[ERROR] Invalid state for isInternal %i", state);
            m_isInternal = state;
        }
    private:
        int32_t m_isInternal { -1 }; // -1 = not set   || 0 = not internal (contains data)     || 1 = internal (contains metadata)
    };

    class NID
    {
    public:
        enum class PSTType
        {
            HID = 0x00,
            NORMAL_FOLDER = 0x02,
            SEARCH_FOLDER = 0x03,
            NORMAL_MESSAGE = 0x04,
            ATTACHMENT = 0x05,
            SEARCH_UPDATE_QUEUE = 0x06,
            SEARCH_CRITERIA_OBJECT = 0x07,
            ASSOC_MESSAGE = 0x08,
            CONTENTS_TABLE_INDEX = 0x0A,
            RECEIVE_FOLDER_TABLE = 0X0B,
            OUTGOING_QUEUE_TABLE = 0x0C,
            HIERARCHY_TABLE = 0x0D,
            CONTENTS_TABLE = 0x0E,
            ASSOC_CONTENTS_TABLE = 0x0F,
            SEARCH_CONTENTS_TABLE = 0x10,
            ATTACHMENT_TABLE = 0x11,
            RECIPIENT_TABLE = 0x12,
            SEARCH_TABLE_INDEX = 0x13,
            LTP = 0x1F
        };
        /*
        * nidType (5 bits): Identifies the type of the node represented by the NID. 
        *   The following table specifies a list of values for nidType. However, it 
        *   is worth noting that nidType has no meaning to the structures defined in the NDB Layer. 
        * 
        * Value             Friendly name                       Description 
        * 0x00              NID_TYPE_HID                        Heap node 0x01 NID_TYPE_INTERNAL Internal node (section 2.4.1) 
        * 0x02              NID_TYPE_NORMAL_FOLDER              Normal Folder object (PC) 
        * 0x03              NID_TYPE_SEARCH_FOLDER              Search Folder object (PC) 
        * 0x04              NID_TYPE_NORMAL_MESSAGE             Normal Message object (PC) 
        * 0x05              NID_TYPE_ATTACHMENT                 Attachment object (PC) 
        * 0x06              NID_TYPE_SEARCH_UPDATE_QUEUE        Queue of changed objects for search Folder objects 
        * 0x07              NID_TYPE_SEARCH_CRITERIA_OBJECT     Defines the search criteria for a search Folder object 
        * 0x08              NID_TYPE_ASSOC_MESSAGE              Folder associated information (FAI) Message object (PC) 
        * 0x0A              NID_TYPE_CONTENTS_TABLE_INDEX       Internal, persisted view-related 
        * 0X0B              NID_TYPE_RECEIVE_FOLDER_TABLE       Receive Folder object (Inbox) 
        * 0x0C              NID_TYPE_OUTGOING_QUEUE_TABLE       Outbound queue (Outbox) 
        * 0x0D              NID_TYPE_HIERARCHY_TABLE            Hierarchy table (TC) 
        * 0x0E              NID_TYPE_CONTENTS_TABLE             Contents table (TC) 
        * 0x0F              NID_TYPE_ASSOC_CONTENTS_TABLE       FAI contents table (TC) 
        * 0x10              NID_TYPE_SEARCH_CONTENTS_TABLE      Contents table (TC) of a search Folder object 
        * 0x11              NID_TYPE_ATTACHMENT_TABLE           Attachment table (TC) 
        * 0x12              NID_TYPE_RECIPIENT_TABLE            Recipient table (TC) 
        * 0x13              NID_TYPE_SEARCH_TABLE_INDEX         Internal, persisted view-related 
        * 0x1F              NID_TYPE_LTP                        LTP  
        */
        int64_t nidType{};

        /*
        * nidIndex (27 bits): The identification portion of the NID.
        */
        int64_t nidIndex{};
    };

    std::vector<int64_t> NID_TYPES_VALUES = {
        0x00,
        0x02,
        0x03,
        0x04,
        0x05,
        0x06,
        0x07,
        0x08,
        0x0A,
        0X0B,
        0x0C,
        0x0D,
        0x0E,
        0x0F,
        0x10,
        0x11,
        0x12,
        0x13,
        0x1F
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

    struct Header
    {
        std::vector<byte_t> root{ 72, '\0' }; // Only accepting unicode format so root should be 72 bytes in size.
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
        BREF nodeBTreeRootPage{};
        BREF blockBTreeRootPage{};

        /*
        * ibAMapLast (Unicode: 8 bytes; ANSI 4 bytes): An IB structure (section 2.2.2.3)
        *  that contains the absolute file offset to the last AMap page of the PST file.
        */
        std::uint64_t ibAMapLast{};
    };

    enum class PType
    {
        BBT = 0x80, // Block BTree page.  
        NBT = 0x81, // Node BTree page.   
        FMap = 0x82, // Free Map page.     
        PMap = 0x83, // Allocation Page Map
        AMap = 0x84, // Allocation Map page
        FPMap = 0x85, // Free Page Map page.
        DL = 0x86, // Density List page. 
        INVALID
    };

    struct PageTrailer
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
        PType ptype{PType::INVALID};

        /*
        * ptypeRepeat (1 byte): MUST be set to the same value as ptype.
        */
        PType ptypeRepeat{};

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
        BID bid{};
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
        BID bid{};
    };

    class Entry {};


    class BTEntry : public Entry
    {
    public:
        /*
        * btkey (Unicode: 8 bytes; ANSI: 4 bytes): The key value associated with this BTENTRY. 
        *   All the entries in the child BTPAGE referenced by BREF have key values greater than 
        *   or equal to this key value. The btkey is either an NID (zero extended to 8 bytes for Unicode PSTs) 
        *   or a BID, depending on the ptype of the page. 
        */
        //std::unique_ptr<ID> btKey{ nullptr }; // NID or BID

        BID bid{}; // Only NID or BID is Valid
        NID nid{};
        IDType idType{ IDType::invalid };

        /*
        * BREF (Unicode: 16 bytes; ANSI: 8 bytes): BREF structure (section 2.2.2.4) that points to the child BTPAGE. 
        */
        BREF bref{};
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

        BREF bref{};
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

        NID nid{};
        NID nidParent{};
        BID bidData{};
        BID bidSub{};
        int64_t dwPadding{};
    };

    PType getPType(uint8_t ptype)
    {
        switch (ptype)
        {
        case 0x80:
            return PType::BBT;
        case 0x81:
            return PType::NBT;
        case 0x82:
            return PType::FMap;
        case 0x83:
            return PType::PMap;
        case 0x84:
            return PType::AMap;
        case 0x85:
            return PType::FPMap;
        case 0x86:
            return PType::DL;
        default:
            ASSERT(false, "[ERROR] Invalid PType %i", ptype);
            return PType::INVALID;
        }
    }

    std::string PTypeToString(PType t)
    {
        switch (t)
        {
        case PType::BBT:
            return "PType::BBT";
        case PType::NBT:
            return "PType::NBT";
        case PType::FMap:
            return "PType::FMap";
        case PType::PMap:
            return "PType::PMap";
        case PType::AMap:
            return "PType::AMap";
        case PType::FPMap:
            return "PType::FPMap";
        case PType::DL:
            return "PType::DL";
        default:
            return "Unknown PType";
        }
    }

    template<typename T>
    std::string NIDTypeToString(T t)
    {
        switch ((int64_t)t)
        {
        case 0x00:
            return "NID_TYPE_HID";
        case 0x02:
            return "NID_TYPE_NORMAL_FOLDER";
        case 0x03:
            return "NID_TYPE_SEARCH_FOLDER";
        case 0x04:
            return "NID_TYPE_NORMAL_MESSAGE";
        case 0x05:
            return "NID_TYPE_ATTACHMENT";
        case 0x06:
            return "NID_TYPE_SEARCH_UPDATE_QUEUE";
        case 0x07:
            return "NID_TYPE_SEARCH_CRITERIA_OBJECT";
        case 0x08:
            return "NID_TYPE_ASSOC_MESSAGE";
        case 0x0A:
            return "NID_TYPE_CONTENTS_TABLE_INDEX";
        case 0X0B:
            return "NID_TYPE_RECEIVE_FOLDER_TABLE";
        case 0x0C:
            return "NID_TYPE_OUTGOING_QUEUE_TABLE";
        case 0x0D:
            return "NID_TYPE_HIERARCHY_TABLE";
        case 0x0E:
            return "NID_TYPE_CONTENTS_TABLE";
        case 0x0F:
            return "NID_TYPE_ASSOC_CONTENTS_TABLE";
        case 0x10:
            return "NID_TYPE_SEARCH_CONTENTS_TABLE";
        case 0x11:
            return "NID_TYPE_ATTACHMENT_TABLE";
        case 0x12:
            return "NID_TYPE_RECIPIENT_TABLE";
        case 0x13:
            return "NID_TYPE_SEARCH_TABLE_INDEX";
        case 0x1F:
            return "NID_TYPE_LTP";
        default:
            return "Invalid NID Type";
        }

    }

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
            ASSERT((pageTrailer.ptype != PType::INVALID), "[ERROR] Pagetrailer was not setup properly. PType is set to INVALID .");
            auto ptype = pageTrailer.ptype;
            if (cLevel == 0 && ptype == PType::NBT) // entries are NBTENTRY 
            {
                return EntryType::N_BTENTRY;
            }

            else if ((cLevel > 0 && ptype == PType::BBT) || (ptype == PType::NBT && cLevel > 0)) // entries are BTENTRY 
            {
                return EntryType::_BTENTRY;
            }

            else if (cLevel == 0 && ptype == PType::BBT) // BBTENTRY (Leaf BBT Entry) 
            {
                return EntryType::B_BTENTRY;
            }

            ASSERT(false, "[ERROR] Invalid PType for BTPage %s", PTypeToString(ptype));
            return EntryType::INVALID;
        }

        bool isLeafPage() const
        {
            return getEntryType() == EntryType::B_BTENTRY || getEntryType() == EntryType::N_BTENTRY;
        }
    };

    /*
    * data (Variable): Raw data. 
    *
    * padding (Variable, Optional): Reserved.
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
    class Block {};

    class DataBlock : public Block
    {
    public:
        std::vector<byte_t> data{};
        int32_t padding{};
        BlockTrailer trailer{};
    };


    /*
    * @brief: XBLOCKs are used when the data associated with a node data that exceeds 8,176 bytes in size. 
        The XBLOCK expands the data that is associated with a node by using an array of BIDs that reference data 
        blocks that contain the data stream associated with the node. A BLOCKTRAILER is present at the end of an XBLOCK, 
        and the end of the BLOCKTRAILER MUST be aligned on a 64-byte boundary. 

    * btype (1 byte): Block type; MUST be set to 0x01 to indicate an XBLOCK or XXBLOCK. 
    *
    * cLevel (1 byte): MUST be set to 0x01 to indicate an XBLOCK. 
    * 
    * cEnt (2 bytes): The count of BID entries in the XBLOCK. 
    * 
    * lcbTotal (4 bytes): Total count of bytes of all the external data stored in the data blocks referenced by XBLOCK. 
    * 
    * rgbid (variable): Array of BIDs that reference data blocks. The size is equal to the number of 
    *   entries indicated by cEnt multiplied by the size of a BID (8 bytes for Unicode PST files, 4 bytes for ANSI PST files). 
    *   
    * rgbPadding (variable, optional): This field is present if the total size of all of the other fields is not a multiple of 64. 
    *   The size of this field is the smallest number of bytes required to make the size of the XBLOCK a multiple of 64. 
    *   Implementations MUST ignore this field. 
    *   
    * blockTrailer (ANSI: 12 bytes; Unicode: 16 bytes): A BLOCKTRAILER structure (section 2.2.2.8.1).
    * 
    */
    class XBlock : public Block
    {
    public:
        int32_t btype{};
        int32_t cLevel{};
        int32_t cEnt{};
        int64_t lcbTotal{};
        std::vector<BID> rgbid{};
        int64_t rgbPadding{};
        BlockTrailer trailer{};
    };

    /**
    * @brief The XXBLOCK further expands the data that is associated with a node by using an array of BIDs that reference XBLOCKs. 
        A BLOCKTRAILER is present at the end of an XXBLOCK, and the end of the BLOCKTRAILER MUST be aligned on a 64-byte boundary. 

    * btype (1 byte): Block type; MUST be set to 0x01 to indicate an XBLOCK or XXBLOCK.
    *
    * cLevel (1 byte): MUST be set to 0x02 to indicate an XXBLOCK.
    *
    * cEnt (2 bytes): The count of BID entries in the XXBLOCK.
    *
    * lcbTotal (4 bytes): Total count of bytes of all the external data stored in XBLOCKs under this XXBLOCK. 
    *
    * rgbid (variable): Array of BIDs that reference XBLOCKs. The size is equal to the number of entries indicated by cEnt 
        multiplied by the size of a BID (8 bytes for Unicode PST files, 4 bytes for ANSI PST Files). 
    *
    * rgbPadding (variable, optional): This field is present if the total size of all of the other fields is not a multiple of 64.
    *   The size of this field is the smallest number of bytes required to make the size of the XXBLOCK a multiple of 64.
    *   Implementations MUST ignore this field.
    *
    * blockTrailer (ANSI: 12 bytes; Unicode: 16 bytes): A BLOCKTRAILER structure (section 2.2.2.8.1).
    */
    class XXBlock : public Block
    {
    public:
        int32_t btype{};
        int32_t cLevel{};
        int32_t cEnt{};
        int64_t lcbTotal{};
        std::vector<BID> rgbid{};
        int64_t rgbPadding{};
        BlockTrailer trailer{};
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
            BTPage nodeBTPage = _readBTPage(m_root.nodeBTreeRootPage, PType::NBT);
            BTPage blockBTPage = _readBTPage(m_root.blockBTreeRootPage, PType::BBT);

            _buildBTPage(nodeBTPage);
            _buildBTPage(blockBTPage);
        }


    private:

        void _buildBTPage(BTPage& rootPage)
        {
            for (const Entry* entry : rootPage.rgentries)
            {
                auto etype = rootPage.getEntryType();
                if (etype == BTPage::EntryType::_BTENTRY)
                {
                    const BTEntry* btentry = static_cast<const BTEntry*>(entry);
                    rootPage.subPages.push_back(_readBTPage(btentry->bref, rootPage.pageTrailer.ptype));
                }
            }
            LOG("[INFO] BTPage has %i subpages", rootPage.subPages.size());

            int idxProcessing = 0;
            int maxIter = 1000;
            while (idxProcessing < rootPage.subPages.size() && maxIter > 0)
            {
                const BTPage& currentPage = rootPage.subPages.at(idxProcessing);
                for (const Entry* entry : currentPage.rgentries)
                {
                    auto etype = currentPage.getEntryType();
                    if (etype == BTPage::EntryType::_BTENTRY)
                    {
                        const BTEntry* btentry = static_cast<const BTEntry*>(entry);
                        rootPage.subPages.push_back(_readBTPage(btentry->bref, currentPage.pageTrailer.ptype));
                    }
                    maxIter--;
                    idxProcessing++;
                }
            }
        }

        std::vector<byte_t> _readBytes(std::uint32_t numBytes)
        {
            std::vector<byte_t> res(numBytes, '\0');
            m_file.read((char*)res.data(), sizeof(byte_t) * res.size());
            //m_file.read(res.data(), sizeof(byte_t) * res.size());
            return res;
        }

        template<class T>
        T _readBytes(std::uint32_t numBytes, T(*convert)(const std::vector<byte_t>&))
        {
            std::vector<byte_t> res = _readBytes(numBytes);
            return convert(res);
        }

        void _open(const std::string path, int mode = std::ios::binary)
        {
            m_file.open(path, mode);
            ASSERT(m_file.is_open(), "[ERROR] Failed to open [file] %s", path.c_str());
        }

        BID _readBID(const std::vector<byte_t>& bytes, bool isBlockBID)
        {
            ASSERT((bytes.size() == 8), "[ERROR] BID must be 8 bytes not %i", bytes.size());
            std::bitset<64> bits = bytesToBits<64>(bytes);

            /**
            * There are two types of BIDs
            * 
            * BIDs used in the context of Pages (section 2.2.2.7) use all of the bits of the structure (below) and are incremented by 1. 2. 
             
            * Block BIDs (section 2.2.2.8) reserve the two least significant bits for flags (see below). As 
                a result these increment by 4 each time a new one is assigned. 
            */
            BID bid{};
            if (isBlockBID)
            {
                // ignore this bit
                uint64_t writersOnly = slice<64, 1>(bits, 0, 1).to_ulong();
                //ASSERT((writersOnly == 0), "[ERROR] 1st bit of BID has to be 0 not %i", writersOnly);

                bid.setIsInternal(slice<64, 1>(bits, 1, 1).to_ulong());
                bid.bidIndex = slice<64, 62>(bits, 2, 62).to_ullong();
                //LOG("[INFO] [bidIndex] %i and [isInternal] %i", bid.bidIndex, bid.isInternal);

                return bid;
            }
            // Page BID
            bid.bidIndex = slice<64, 64>(bits, 0, 64).to_ullong();
            return bid;

        }

        NID _readNID(const std::vector<byte_t>& bytes)
        {
            /*
            * nidType (5 bits): Identifies the type of the node represented by the NID. 
            *   The following table specifies a list of values for nidType. However, it is worth 
            *   noting that nidType has no meaning to the structures defined in the NDB Layer. 
            * 
            * nidIndex (27 bits): The identification portion of the NID.
            */

            ASSERT((bytes.size() == 4), "[ERROR] NID must be 4 bytes not %i", bytes.size());
            std::bitset<32> bits = bytesToBits<32>(bytes);
            NID nid{};

            //int32_t temp = slice(bytes, 0, 4, toT_l<int32_t>);

            nid.nidType = slice<32, 5>(bits, 0, 5).to_ulong(); //temp & 0xF800;
            nid.nidIndex = slice<32, 27>(bits, 5, 27).to_ulong(); //temp & ~0xF800;

            ASSERT( (isIn(nid.nidType, NID_TYPES_VALUES) ), "[ERROR] Invalid NID Type %i", (int64_t)nid.nidType);
            //LOG("[INFO] [nidType] %i and [nidIndex] %i", nid.nidType, nid.nidIndex);
            return nid;
        }

        BREF _readBREF(const std::vector<byte_t>& bytes, bool isBlockBID, std::string name = "NOT SET")
        {
            ASSERT((bytes.size() == 16), "[ERROR] BREF must be 16 bytes not %i", bytes.size());
            // bid (Unicode: 64 bits; ANSI: 32 bits): A BID structure, as specified in section
            std::vector<byte_t> bid = slice(bytes, 0, 8);
            // ib (Unicode: 64 bits; ANSI: 32 bits): An IB structure, as specified in section 2.2.2.3
            // std::vector<byte_t> ib = slice(bytes, 8, 16);

            BREF bref{};
            bref.name = name;
            bref.bid = _readBID(bid, isBlockBID);
            bref.ib = slice(bytes, 8, 16, toT_l<int64_t>);

            return bref;
        }


        NBTEntry _readNBTEntry(const std::vector<byte_t>& bytes)
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
            entry.nid = _readNID(slice(bytes, 0, 4)); // NID is zero padded from 4 to 8
            entry.bidData = _readBID(slice(bytes, 8, 16), false);
            entry.bidSub = _readBID(slice(bytes, 16, 24), false);
            entry.nidParent = _readNID(slice(bytes, 24, 28));
            entry.dwPadding = slice(bytes, 28, 32, toT_l<int64_t>);

            LOG("[INFO] Created NBTEntry with [nid] of %s", NIDTypeToString(entry.nid.nidType).c_str());
            return entry;
        }

        BTEntry _readBTEntry(const std::vector<byte_t>& bytes, PType pagePType)
        {
            /*
            * 
            * btkey (Unicode: 8 bytes; ANSI: 4 bytes): The key value associated with this BTENTRY. 
            *   All the entries in the child BTPAGE referenced by BREF have key values greater than or equal to this key value. 
            *   The btkey is either an NID (zero extended to 8 bytes for Unicode PSTs) or a BID, depending on the ptype of the page. 
            * 
            * BREF (Unicode: 16 bytes; ANSI: 8 bytes): BREF structure (section 2.2.2.4) that points to the child BTPAGE. 
            */

            ASSERT( (bytes.size() == 24), "[ERROR] BTEntry must be 24 bytes not %i", bytes.size());

            BTEntry entry{};
            if (pagePType == PType::NBT)
            {
                entry.nid = _readNID(slice(bytes, 0, 4)); // NID is zero padded from 4 to 8
                entry.idType = IDType::nid;
                entry.bref = _readBREF(slice(bytes, 8, 24), false);
            }

            else if (pagePType == PType::BBT)
            {
                entry.bid = _readBID(slice(bytes, 0, 8), true);
                entry.idType = IDType::bid;
                entry.bref = _readBREF(slice(bytes, 8, 24), true);
            }

            else
            {
                ASSERT(false, "[ERROR] Unknown Page [PType] %s for BTEntry", PTypeToString(pagePType));
            }
            LOG("[INFO] Created BTEntry with bid %i and BREF bid %i", entry.bid.bidIndex, entry.bref.bid.bidIndex);
            return entry;
        }

        BBTEntry _readBBTEntry(const std::vector<byte_t>& bytes)
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
            entry.bref = _readBREF(slice(bytes, 0, 16), false);
            entry.cb = slice(bytes, 16, 18, toT_l<uint16_t>);
            entry.cRef = slice(bytes, 18, 20, toT_l<uint16_t>);
            entry.dwPadding = slice(bytes, 20, 24, toT_l<uint32_t>);

            ASSERT((entry.dwPadding == 0), "[ERROR] dwPadding must be 0 for BBTEntry not %i", entry.dwPadding);
            return entry;
        }

        PageTrailer _readPageTrailer(const std::vector<byte_t>& pageTrailerBytes)
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
            std::uint8_t ptypeByte = slice(pageTrailerBytes, 0, 1, &toT_l<std::uint8_t>);
            PType ptype = getPType(ptypeByte);

            /*
            * ptypeRepeat (1 byte): MUST be set to the same value as ptype.
            */
            std::uint8_t ptypeRepeatByte = slice(pageTrailerBytes, 1, 2, &toT_l<std::uint8_t>);
            PType ptypeRepeat = getPType(ptypeRepeatByte);

            ASSERT((ptypeByte == ptypeRepeatByte), "Ptype [%i] and PtypeRepeat [%i]", ptypeByte, ptypeRepeatByte);

            /*
            * wSig (2 bytes): Page signature. This value depends on the value of the ptype field.
            *   This value is zero (0x0000) for AMap, PMap, FMap, and FPMap pages. For BBT, NBT,
            *   and DList pages, a page / block signature is computed (see section 5.5).
            */
            std::uint16_t wSig = slice(pageTrailerBytes, 2, 4, &toT_l<std::uint16_t>);

            /*
            * dwCRC (4 bytes): 32-bit CRC of the page data, excluding the page trailer.
            *   See section 5.3 for the CRC algorithm. Note the locations of the dwCRC and
            *   bid are differs between the Unicode and ANSI version of this structure.
            */
            std::uint32_t dwCRC = slice(pageTrailerBytes, 4, 8, &toT_l<std::uint32_t>);

            /*
            * bid (Unicode: 8 bytes; ANSI 4 bytes): The BID of the page's block. AMap, PMap,
            *   FMap, and FPMap pages have a special convention where their BID is assigned the
            *   same value as their IB (that is, the absolute file offset of the page). The bidIndex
            *   for other page types are allocated from the special bidNextP counter in the HEADER structure.
            */
            BID bid = _readBID(slice(pageTrailerBytes, 8, 16), false);

            PageTrailer pt{};
            pt.ptype = ptype;
            pt.ptypeRepeat = ptypeRepeat;
            pt.wSig = wSig;
            pt.dwCRC = dwCRC;
            pt.bid = bid;

            LOG("[INFO] Create Page Trailer w [ptype] %s", PTypeToString(ptype).c_str());

            return pt;
        }

        BTPage _readBTPage(BREF bTreeBref, PType treeType)
        {
            // from the begging of the file move to .ib
            m_file.seekg(bTreeBref.ib, std::ios::beg);
            /*
            * Entries of the BTree array. The entries in the array depend on the value of the cLevel field. 
            *   If cLevel is greater than 0, then each entry in the array is of type BTENTRY. If cLevel is 0, 
            *   then each entry is either of type BBTENTRY or NBTENTRY, depending on the ptype of the page.
            */
            std::vector<byte_t> rgentries = _readBytes(488);
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
            auto cEnt = _readBytes(1, &toT_l<decltype(BTPage::cbEnt)>);
            auto cEntMax = _readBytes(1, &toT_l<decltype(BTPage::cEntMax)>);
            auto cbEnt = _readBytes(1, &toT_l<decltype(BTPage::cbEnt)>);
            auto cLevel = _readBytes(1, &toT_l<decltype(BTPage::cLevel)>);

            /*
            * dwPadding (Unicode: 4 bytes): Padding; MUST be set to zero. Note there is no padding in the ANSI version of this structure.
            */
            auto dwPadding = _readBytes(4, &toT_l<decltype(BTPage::dwPadding)>);

            LOG("%i %i", cbEnt, cLevel);

            /**
             * pageTrailer (Unicode: 16 bytes; ANSI: 12 bytes): A PAGETRAILER structure (section 2.2.2.7.1). The ptype 
             *  subfield of pageTrailer MUST be set to ptypeBBT for a Block BTree page, or ptypeNBT for a Node BTree page. 
             *  The other subfields of pageTrailer MUST be set as specified in section 2.2.2.7.1. 
            */
            std::vector<byte_t> pageTrailerBytes = _readBytes(16);
            PageTrailer pageTrailer = _readPageTrailer(pageTrailerBytes);

            ASSERT((pageTrailer.ptype == PType::BBT || pageTrailer.ptype == PType::NBT), "[ERROR] Invalid ptype for pagetrailer");
            ASSERT((pageTrailer.ptype == treeType), "[ERROR] Pagetrailer PType and given Tree PType do not match");
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
            const std::vector<byte_t>& entriesInBytes, 
            PType treePtype, 
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
                ASSERT( (size == singleEntrySize), "[ERROR] Size should be %i NOT %i", singleEntrySize, size);
                std::vector<byte_t> entryBytes = slice(entriesInBytes, start, end);

                /**
                * Entries of the BTree array. The entries in the array depend on the value of the cLevel field. 
                *   If cLevel is greater than 0, then each entry in the array is of type BTENTRY. If cLevel is 0, 
                *   then each entry is either of type BBTENTRY or NBTENTRY, depending on the ptype of the page.
                */

                if (cLevel == 0 && treePtype == PType::NBT) // entries are NBTENTRY 
                {
                    NBTEntry* entry = new NBTEntry(_readNBTEntry(entryBytes));
                    entries.push_back(static_cast<Entry*>(entry));
                }

                else if ((cLevel > 0 && treePtype == PType::BBT) || (treePtype == PType::NBT && cLevel > 0) ) // entries are BTENTRY 
                {
                    BTEntry* entry = new BTEntry(_readBTEntry(entryBytes, treePtype));
                    entries.push_back(static_cast<Entry*>(entry));
                }

                else if (cLevel == 0 && treePtype == PType::BBT) // BBTENTRY (Leaf BBT Entry) 
                {
                    BBTEntry* entry = new BBTEntry(_readBBTEntry(entryBytes));
                    entries.push_back(static_cast<Entry*>(entry));
                }

                else
                {
                    ASSERT(false, "[ERROR] Invalid [cLevel] %i and [treePType] %i combination at iteration %i", cLevel, PTypeToString(treePtype), i);
                }
            }
            return entries;
        }

        BlockTrailer _readBlockTrailer(const std::vector<byte_t>& bytes)
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
            trailer.cb = slice(bytes, 0, 2, toT_l<int64_t>);
            trailer.wSig = slice(bytes, 2, 4, toT_l<int64_t>);
            trailer.dwCRC = slice(bytes, 4, 8, toT_l<int64_t>);
            trailer.bid = _readBID(slice(bytes, 8, 16), true);
            return trailer;
        }

        std::unique_ptr<Block> _readBlock(BREF blockBref, int blockSize, bool isDataTree)
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

            std::vector<byte_t> blockBytes = _readBytes(blockSize);
            BlockTrailer trailer = _readBlockTrailer(slice(blockBytes, blockBytes.size() - 16, blockBytes.size()) );

            if (!trailer.bid.isInternal() && isDataTree) // Data Block
            {
                return std::make_unique<DataBlock>(_readDataBlock(blockBytes, trailer));
            }

            else if (trailer.bid.isInternal() && !isDataTree) // XBlock or XXBlock
            {
                int32_t btype = slice(blockBytes, 0, 1, toT_l<int32_t>);
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

        DataBlock _readDataBlock(const std::vector<byte_t>& blockBytes, BlockTrailer trailer)
        {
            DataBlock block{};
            block.data = slice(blockBytes, (int64_t)0, trailer.cb);
            block.trailer = trailer;
            return block;
        }

        XBlock _readXBlock(const std::vector<byte_t>& blockBytes, BlockTrailer trailer)
        {
            XBlock block{};
            block.btype = slice(blockBytes, 0, 1, toT_l<int32_t>);
            block.cLevel = slice(blockBytes, 1, 2, toT_l<int32_t>);
            block.cEnt = slice(blockBytes, 2, 4, toT_l<int32_t>);
            block.lcbTotal = slice(blockBytes, 4, 8, toT_l<int64_t>);
            block.trailer = trailer;

            ASSERT((block.btype == 0x01), "[ERROR] btype for XBlock should be 0x01 not %i", block.btype);
            ASSERT((block.cLevel == 0x01), "[ERROR] cLevel for XBlock should be 0x01 not %i", block.cLevel);

            int32_t bidSize = 8;
            // from the end of lcbTotal to the end of the data field
            std::vector<byte_t> bidBytes = slice(blockBytes, (int64_t)8, trailer.cb + (int64_t)bidSize);
            for (int32_t i = 0; i < block.cEnt; i++)
            {
                int32_t start = i * bidSize;
                int32_t end = ((int64_t)i + 1) * bidSize;
                ASSERT((end - start == bidSize), "[ERROR] BID must be of size 8 not %i", (end - start));
                block.rgbid.push_back(_readBID(slice(bidBytes, start, end), true));
            }
            return block;
        }

        XXBlock _readXXBlock(const std::vector<byte_t>& blockBytes, BlockTrailer trailer)
        {
            XXBlock block{};
            block.btype = slice(blockBytes, 0, 1, toT_l<int32_t>);
            block.cLevel = slice(blockBytes, 1, 2, toT_l<int32_t>);
            block.cEnt = slice(blockBytes, 2, 4, toT_l<int32_t>);
            block.lcbTotal = slice(blockBytes, 4, 8, toT_l<int64_t>);
            block.trailer = trailer;

            ASSERT((block.btype == 0x01), "[ERROR] btype for XXBlock should be 0x01 not %i", block.btype);
            ASSERT((block.cLevel == 0x02), "[ERROR] cLevel for XXBlock should be 0x02 not %i", block.cLevel);

            int32_t bidSize = 8;
            // from the end of lcbTotal to the end of the data field
            std::vector<byte_t> bidBytes = slice(blockBytes, (int64_t)8, trailer.cb + (int64_t)bidSize);
            for (int32_t i = 0; i < block.cEnt; i++)
            {
                int32_t start = i * bidSize;
                int32_t end = ((int64_t)i + 1) * bidSize;
                ASSERT((end - start == bidSize), "[ERROR] BID must be of size 8 not %i", (end - start));
                block.rgbid.push_back(_readBID(slice(bidBytes, start, end), true));
            }
            return block;
        }

        Root _readRoot(const Header& header)
        {
            /*
            * dwReserved (4 bytes): Implementations SHOULD ignore this value and SHOULD NOT modify it. 
            *  Creators of a new PST file MUST initialize this value to zero.
            */
            slice(header.root, 0, 4);

            /*
            * ibFileEof (Unicode: 8 bytes; ANSI 4 bytes): The size of the PST file, in bytes. 
            */
            std::uint64_t ibFileEof = slice(header.root, 4, 12, &toT_l<std::uint64_t>);
            LOG("[INFO] [file size in bytes] %i", ibFileEof);

            /*
            * ibAMapLast (Unicode: 8 bytes; ANSI 4 bytes): An IB structure (section 2.2.2.3) 
            *  that contains the absolute file offset to the last AMap page of the PST file. 
            */
            std::uint64_t ibAMapLast = slice(header.root, 12, 20, &toUInt_64l);

            /*
            * cbAMapFree (Unicode: 8 bytes; ANSI 4 bytes): The total free space in all AMaps, combined. 
            
            * cbPMapFree (Unicode: 8 bytes; ANSI 4 bytes): The total free space in all PMaps, combined. 
            *  Because the PMap is deprecated, this value SHOULD be zero. Creators of new PST files MUST initialize this value to zero.
            */
            std::vector<byte_t> cbAMapFree = slice(header.root, 20, 28);
            std::vector<byte_t> cbPMapFree = slice(header.root, 28, 36);

            /*
            * BREFNBT (Unicode: 16 bytes; ANSI: 8 bytes): A BREF structure (section 2.2.2.4) that references the 
            *  root page of the Node BTree (NBT). 
            
            * BREFBBT (Unicode: 16 bytes; ANSI: 8 bytes): A BREF 
            *  structure that references the root page of the Block BTree (BBT). 
            
            * fAMapValid (1 byte_t): Indicates whether all of the AMaps in this PST file are valid. 
            *  For more details, see section 2.6.1.3.7. This value MUST be set to one of the pre-defined values specified in the following table.
            * 
            * Value Friendly name Meaning 
            * 0x00 INVALID_AMAP One or more AMaps in the PST are INVALID 
            * 0x01 VALID_AMAP1 Deprecated. Implementations SHOULD NOT use this The AMaps are VALID.
            * 0x02 VALID_AMAP2 value. The AMaps are VALID.
            */
            std::vector<byte_t> BREFNBT = slice(header.root, 36, 52);
            std::vector<byte_t> BREFBBT = slice(header.root, 52, 68);
            std::vector<byte_t> fAMapValid = slice(header.root, 68, 69);
            LOG("[INFO] AMapValid state [is] %s", toHexString(fAMapValid).c_str());
            /*
            * bReserved (1 byte_t): Implementations SHOULD ignore this value and SHOULD NOT modify it. 
            *  Creators of a new PST file MUST initialize this value to zero.
            */
            std::vector<byte_t> bReserved = slice(header.root, 69, 70);

            /*
            * wReserved (2 bytes): Implementations SHOULD ignore this value and SHOULD NOT modify it. 
            *  Creators of a new PST file MUST initialize this value to zero.
            */
            std::vector<byte_t> wReserved = slice(header.root, 70, 72);

            Root myRoot{};
            myRoot.fileSize = ibFileEof;
            myRoot.nodeBTreeRootPage = _readBREF(BREFNBT, "Root Page for Node BTree");
            myRoot.blockBTreeRootPage = _readBREF(BREFBBT, "Root Page for Block BTree");
            myRoot.ibAMapLast = ibAMapLast;
            return myRoot;
        }

        Header _readHeader(const std::ifstream& file)
        {
            /**
             * dwMagic (4 bytes): MUST be "{ 0x21, 0x42, 0x44, 0x4E } ("!BDN")". 
            */
            std::vector<byte_t> dwMagic = _readBytes(4);
            isEqual(dwMagic, { 0x21, 0x42, 0x44, 0x4E });

            /**
             * dwCRCPartial (4 bytes): The 32-bit cyclic redundancy check (CRC) value 
             *  of the 471 bytes of data starting from wMagicClient (0ffset 0x0008)  
            */
            std::vector<byte_t> dwCRCPartial = _readBytes(4);

            /**
             * wMagicClient (2 bytes): MUST be "{ 0x53, 0x4D }".  
            */
            std::vector<byte_t> wMagicClient = _readBytes(2);
            isEqual(wMagicClient, { 0x53, 0x4D });

            /**
            * wVer (2 bytes):
             1. This value MUST be 14 or 15 if the file is an ANSI PST file 
             2. MUST be greater than 23 if the file is a Unicode PST file. 
                 If the value is 37, it indicates that the file is written by an 
                 Outlook of version that supports Windows Information Protection (WIP). 
                 The data MAY have been protected by WIP.  
            */
            std::uint16_t wVer = _readBytes(2, &toT_l<std::uint16_t>);
            ASSERT((wVer >= 23), "[ERROR] [wVer] %i was not greater than 23", wVer);

            /*
            * wVerClient (2 bytes): Client file format version. The version that corresponds to 
            * the format described in this document is 19. Creators of a new PST file 
            * based on this document SHOULD initialize this value to 19. 
            * 
            * bPlatformCreate (1 byte_t): This value MUST be set to 0x01. 
            * 
            * bPlatformAccess (1 byte_t): This value MUST be set to 0x01. 

            * dwReserved1 (4 bytes): Implementations SHOULD ignore this value and SHOULD NOT modify it. 
              Creators of a new PST file MUST initialize this value to zero.

            * dwReserved2 (4 bytes): Implementations SHOULD ignore this value and SHOULD NOT modify it. 
              Creators of a new PST file MUST initialize this value to zero.
            */
            _readBytes(12);

            /*
            * bidUnused (8 bytes Unicode only): Unused padding added when the Unicode PST file format was created. 
            */
            _readBytes(8);

            /*
            * bidNextP (Unicode: 8 bytes; ANSI: 4 bytes): Next page BID. 
            *   Pages have a special counter for allocating bidIndex values. 
            *   The value of bidIndex for BIDs for pages is allocated from this counter. 
            */
            _readBytes(8);

            /*
            * dwUnique (4 bytes): This is a monotonically-increasing value that is modified every time the 
            *  PST file's HEADER structure is modified. The function of this value is to provide a unique value, 
            *  and to ensure that the HEADER CRCs are different after each header modification.
            */

           _readBytes(4);

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
           std::vector<byte_t> rgnids = _readBytes(128);

           /*
           * qwUnused (8 bytes): Unused space; MUST be set to zero. Unicode PST file format only. 
           */
           _readBytes(8);

           /*
           * root (Unicode: 72 bytes; ANSI: 40 bytes): A ROOT structure
           */
           std::vector<byte_t> root = _readBytes(72);

           /*
           * dwAlign (4 bytes): Unused alignment bytes; MUST be set to zero. Unicode PST file format only.
           */
           std::uint32_t dwAlign = _readBytes(4, &toT_l<std::uint32_t>);
           ASSERT((dwAlign == 0), "[ERROR] [dwAlign] %i was not set to zero.", dwAlign);
            
           /*
           * rgbFM (128 bytes): Deprecated FMap. This is no longer used and MUST be filled with 0xFF. 
           *  Readers SHOULD ignore the value of these bytes. 
           
           * rgbFP (128 bytes): Deprecated FPMap. This is no longer used and MUST be filled with 0xFF. 
           *  Readers SHOULD ignore the value of these bytes.
           */

           std::vector<byte_t> rgb = _readBytes(256);
           //std::cout << toHexString(rgb) << "\n\n" << std::endl;

           /*
           * bSentinel (1 byte_t): MUST be set to 0x80. 
           */
           std::vector<byte_t> bSentinel = _readBytes(1);
           //std::cout << toHexString(bSentinel) << std::endl;
           isEqual(bSentinel, { 0x80 }, "bSentinel");

           /*
           * bCryptMethod (1 byte_t): Indicates how the data within the PST file is encoded. 
           * MUST be set to one of the pre-defined values described in the following table. 
           * 
           * Value Friendly name Meaning 
           * 0x00 NDB_CRYPT_NONE Data blocks are not encoded. 
           * 0x01 NDB_CRYPT_PERMUTE Encoded with the Permutation algorithm (section 5.1). 
           * 0x02 NDB_CRYPT_CYCLIC Encoded with the Cyclic algorithm (section 5.2). 
           * 0x10 NDB_CRYPT_EDPCRYPTED Encrypted with Windows Information Protection. 
           */
           std::vector<byte_t> bCryptMethod = _readBytes(1);
           LOG("[bCryptMethod] %s", toHexString(bCryptMethod).c_str());
            
           /*
           * rgbReserved (2 bytes): Reserved; MUST be set to zero.
           */
           std::vector<byte_t> rgbReserved = _readBytes(2);
           isEqual(rgbReserved, { 0x00, 0x00 }, "rgbReserved");

           /*
           * bidNextB (Unicode ONLY: 8 bytes): Next BID. 
           *  This value is the monotonic counter that indicates the 
           *  BID to be assigned for the next allocated block. BID values advance in increments of 4. 
           *  For more details, see section 2.2.2.2.
           */
           _readBytes(8);

           /*
           * dwCRCFull (4 bytes): The 32-bit CRC value of the 516 bytes of data starting from 
           *  wMagicClient to bidNextB, inclusive. Unicode PST file format only. 
           */

           /*
           * rgbReserved2 (3 bytes): Implementations SHOULD ignore this value and SHOULD NOT modify it. 
           * Creators of a new PST MUST initialize this value to zero.
           */
           std::vector<byte_t> rgbReserved2 = _readBytes(3);
           //isEqual(rgbReserved2, { 0x00, 0x00, 0x00 }, "rgbReserved2");

           /*
           * bReserved (1 byte_t): Implementations SHOULD ignore this value and SHOULD NOT modify it. 
           *  Creators of a new PST file MUST initialize this value to zero.
           */
           std::vector<byte_t> bReserved = _readBytes(1);
           //isEqual(bReserved, { 0x00 }, "bReserved");

           /*
           * rgbReserved3 (32 bytes): Implementations SHOULD ignore this value and SHOULD NOT modify it. 
           *  Creators of a new PST MUST initialize this value to zero.
           */
           std::vector<byte_t> rgbReserved3 = _readBytes(32);
           std::vector<byte_t> B(32, 0x00);
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


