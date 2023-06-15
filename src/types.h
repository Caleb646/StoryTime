#ifndef READER_TYPES_H
#define READER_TYPES_H

namespace reader {
	namespace types {
		typedef unsigned char byte_t;

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

        enum class NIDType
        {
            HID = 0x00,                     // NID_TYPE_HID                        Heap node
            INTERNAL = 0x01,                // NID_TYPE_INTERNAL                   Internal node (section 2.4.1)
            NORMAL_FOLDER = 0x02,           // NID_TYPE_NORMAL_FOLDER              Normal Folder object (PC)
            SEARCH_FOLDER = 0x03,           // NID_TYPE_SEARCH_FOLDER              Search Folder object (PC)
            NORMAL_MESSAGE = 0x04,          // NID_TYPE_NORMAL_MESSAGE             Normal Message object (PC)
            ATTACHMENT = 0x05,              // NID_TYPE_ATTACHMENT                 Attachment object (PC)
            SEARCH_UPDATE_QUEUE = 0x06,     // NID_TYPE_SEARCH_UPDATE_QUEUE        Queue of changed objects for search Folder objects
            SEARCH_CRITERIA_OBJECT = 0x07,  // NID_TYPE_SEARCH_CRITERIA_OBJECT     Defines the search criteria for a search Folder object
            ASSOC_MESSAGE = 0x08,           // NID_TYPE_ASSOC_MESSAGE              Folder associated information (FAI) Message object (PC)
            CONTENTS_TABLE_INDEX = 0x0A,    // NID_TYPE_CONTENTS_TABLE_INDEX       Internal, persisted view-related
            RECEIVE_FOLDER_TABLE = 0X0B,    // NID_TYPE_RECEIVE_FOLDER_TABLE       Receive Folder object (Inbox)
            OUTGOING_QUEUE_TABLE = 0x0C,    // NID_TYPE_OUTGOING_QUEUE_TABLE       Outbound queue (Outbox)
            HIERARCHY_TABLE = 0x0D,         // NID_TYPE_HIERARCHY_TABLE            Hierarchy table (TC)
            CONTENTS_TABLE = 0x0E,          // NID_TYPE_CONTENTS_TABLE             Contents table (TC)
            ASSOC_CONTENTS_TABLE = 0x0F,    // NID_TYPE_ASSOC_CONTENTS_TABLE       FAI contents table (TC)
            SEARCH_CONTENTS_TABLE = 0x10,   // NID_TYPE_SEARCH_CONTENTS_TABLE      Contents table (TC) of a search Folder object
            ATTACHMENT_TABLE = 0x11,        // NID_TYPE_ATTACHMENT_TABLE           Attachment table (TC)
            RECIPIENT_TABLE = 0x12,         // NID_TYPE_RECIPIENT_TABLE            Recipient table (TC)
            SEARCH_TABLE_INDEX = 0x13,      // NID_TYPE_SEARCH_TABLE_INDEX         Internal, persisted view-related
            LTP = 0x1F,                     // NID_TYPE_LTP                        LTP
            INVALID
        };

        enum class BType
        {
            Reserved1   = 0x6C,   // Reserved 
            TC          = 0x7C,   // Table Context(TC / HN) 
            Reserved2   = 0x8C,   // Reserved 
            Reserved3   = 0x9C,   // Reserved 
            Reserved4   = 0xA5,   // Reserved 
            Reserved5   = 0xAC,   // Reserved 
            BTH         = 0xB5,   // BTree - on - Heap(BTH) 
            PC          = 0xBC,   // Property Context(PC / BTH) 
            Reserved6   = 0xCC,   // Reserved
            Invalid
        };

        enum class FillLevel
        {
            LEVEL_EMPTY = 0x0, //  At least 3584 bytes free / data block does not exist
            LEVEL_1     = 0x1, // 2560 - 3584 bytes free
            LEVEL_2     = 0x2, // 2048 - 2560 bytes free
            LEVEL_3     = 0x3, // 1792 - 2048 bytes free
            LEVEL_4     = 0x4, // 1536 - 1792 bytes free
            LEVEL_5     = 0x5, // 1280 - 1536 bytes free
            LEVEL_6     = 0x6, // 1024 - 1280 bytes free
            LEVEL_7     = 0x7, // 768 - 1024 bytes free
            LEVEL_8     = 0x8, // 512 - 768 bytes free
            LEVEL_9     = 0x9, // 256 - 512 bytes free
            LEVEL_10    = 0xA, // 128 - 256 bytes free
            LEVEL_11    = 0xB, // 64 - 128 bytes free
            LEVEL_12    = 0xC, // 32 - 64 bytes free
            LEVEL_13    = 0xD, // 16 - 32 bytes free
            LEVEL_14    = 0xE, // 8 - 16 bytes free
            LEVEL_FULL  = 0xF, // Data block has less than 8 bytes free
        };

        enum class Entry
        {
            INVALID,
            BTENTRY, 
            BBTENTRY,
            NBTENTRY
        };
	}
}

#endif // READER_TYPES_H