#include <iostream>
#include <stdarg.h>
#include <cassert>
#include <vector>
#include <string>
#include <fstream>

#ifndef READER_TYPES_H
#define READER_TYPES_H

namespace reader {
	namespace types {
		typedef unsigned char byte_t;
        typedef uint16_t utf16le_t;

        enum class PType : uint32_t
        {
            BBT = 0x80, // Block BTree page.  
            NBT = 0x81, // Node BTree page.   
            FMap = 0x82, // Free Map page.     
            PMap = 0x83, // Allocation Page Map
            AMap = 0x84, // Allocation Map page
            FPMap = 0x85, // Free Page Map page.
            DL = 0x86, // Density List page. 
            INVALID = 0xFFFFFF // Invalid page type.
        };

        enum class NIDType : uint32_t
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
            INVALID = 0xFFFFFF            // NID_TYPE_NONE                       Invalid NID
        };

        enum class BType : uint32_t
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

        enum class BlockType : uint32_t
        {
            DATA        = 0xFF,
            X           = 0x01,
            XX          = 0x02,
            INVALID     = 0xFFFFF
        };

        enum class FillLevel : uint32_t
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

        enum class PropertyType : uint32_t
        {
            Integer16               =   0x0002, /// 2 bytes; a 16 - bit integer
            Integer32               =   0x0003, /// 4 bytes; a 32 - bit integer
            Floating32              =   0x0004, /// 4 bytes; a 32 - bit floating point number
            Floating64              =   0x0005, /// 8 bytes; a 64 - bit floating point number
            Currency                =   0x0006, /// 8 bytes; a 64 - bit signed, scaled integer representation of a decimal currency value, with four places to the right of the decimal point
            FloatingTime            =   0x0007, /// 8 bytes; a 64 - bit floating point number in which the whole number part represents the number of days since December 30, 1899, and the fractional part represents the fraction of a day since midnight
            ErrorCode               =   0x000A, /// 4 bytes; a 32 - bit integer encoding error information as specified in section 2.4.1.
            Boolean                 =   0x000B, /// 1 byte; restricted to 1 or 0
            Integer64               =   0x0014, /// 8 bytes; a 64 - bit integer
            String                  =   0x001F, /// Variable size; a string of Unicode characters in UTF - 16LE format encoding with terminating null character(0x0000).
            String8                 =   0x001E, /// Variable size; a string of multibyte characters in externally specified encoding with terminating null character(single 0 byte).
            Time                    =   0x0040, /// 8 bytes; a 64 - bit integer representing the number of 100 - nanosecond intervals since January 1, 1601
            Guid                    =   0x0048, /// 16 bytes; a GUID with Data1, Data2, and Data3 fields in little - endian format
            ServerId                =   0x00FB, /// Variable size; a 16 - bit COUNT field followed by a structure as specified in section 2.11.1.4.
            Restriction             =   0x00FD, /// Variable size; a byte array representing one or more Restriction structures as specified in section 2.12.
            RuleAction              =   0x00FE, /// Variable size; a 16 - bit COUNT field followed by that many rule action structures, as specified in[MS - OXORULE] section 2.2.5.
            Binary                  =   0x0102, /// Variable size; a COUNT field followed by that many bytes.
            MultipleInteger16       =   0x1002, /// Variable size; a COUNT field followed by that many PtypInteger16 values.
            MultipleInteger32       =   0x1003, /// Variable size; a COUNT field followed by that many PtypInteger32 values.
            MultipleFloating32      =   0x1004, /// Variable size; a COUNT field followed by that many PtypFloating32 values.
            MultipleFloating64      =   0x1005, /// Variable size; a COUNT field followed by that many PtypFloating64 values.
            MultipleCurrency        =   0x1006, /// Variable size; a COUNT field followed by that many PtypCurrency values.
            MultipleFloatingTime    =   0x1007, /// Variable size; a COUNT field followed by that many PtypFloatingTime values.
            MultipleInteger64       =   0x1014, /// Variable size; a COUNT field followed by that many PtypInteger64 values.
            MultipleString          =   0x101F, /// Variable size; a COUNT field followed by that many PtypString values.
            MultipleString8         =   0x101E, /// Variable size; a COUNT field followed by that many PtypString8 values.
            MultipleTime            =   0x1040, /// Variable size; a COUNT field followed by that many PtypTime values.
            MultipleGuid            =   0x1048, /// Variable size; a COUNT field followed by that many PtypGuid values.
            MultipleBinary          =   0x1102, /// Variable size; a COUNT field followed by that many PtypBinary values.
            Unspecified             =   0x0000, /// Any: this property type value matches any type; a server MUST return the actual type in its response.Servers MUST NOT return this type in response to a client request other than NspiGetIDsFromNames or the RopGetPropertyIdsFromNames ROP request([MS - OXCROPS] section 2.2.8.1).
            Null                    =   0x0001, /// None: This property is a placeholder.
            Object                  =   0x000D, /// The property value is a Component Object Model(COM) object, as specified in section 2.11.1.5.
        };

        enum class PidTagType : uint32_t
        {
            RecordKey                 =   0x0FF9,       // Record key. This is the Provider UID of this PST.
            DisplayName               =   0x3001,
            IpmSubTreeEntryId         =   0x35E0,
            IpmWastebasketEntryId     =   0x35E3,
            FinderEntryId             =   0x35E7,
            ContentCount              =   0x3602,
            ContentUnreadCount        =   0x3603,
            Subfolders                =   0x360A,
            NameidBucketCount         =   0x0001,       /// PtypInteger32
            NameidStreamGuid          =   0x0002,       /// PtypBinary
            NameidStreamEntry         =   0x0003,       /// PtypBinary
            NameidStreamString        =   0x0004,       /// PtypBinary
            NameidBucketBase          =   0x1000,       /// PtypBinary
            ItemTemporaryFlags        =   0x1097,       /// PtypInteger32
            PstBestBodyProptag        =   0x661D,       /// PtypInteger32
            PstHiddenCount            =   0x6635,       /// PtypInteger32
            PstHiddenUnread           =   0x6636,       /// PtypInteger32
            PstIpmsubTreeDescendant   =   0x6705,       /// PtypBoolean
            PstSubTreeContainer       =   0x6772,       /// PtypInteger32
            LtpParentNid              =   0x67F1,       /// PtypInteger32
            LtpRowId                  =   0x67F2,       /// PtypInteger32
            LtpRowVer                 =   0x67F3,       /// PtypInteger32
            PstPassword               =   0x67FF,       /// PtypInteger32
            MapiFormComposeCommand    =   0x682F,       /// PtypString
            ContainerClass            =   0x3613,       /// PtypBinary
            ReplItemid                =   0x0E30, /// PtypInteger32  Replication Item ID.N
            ReplChangenum             =   0x0E33, /// PtypInteger64  Replication Change Number.N
            ReplVersionHistory        =   0x0E34, /// PtypBinary  Replication Version History.N
            ReplFlags                 =   0x0E38, /// PtypInteger32  Replication flags.Y

            Importance                      =   0x0017,
            MessageClassW                   =   0x001A,  // PtypString PidTag
            Sensitivity                     =   0x0036,  // PtypInteger3 PidTag
            SubjectW                        =   0x0037,
            ClientSubmitTime                =   0x0039, //PtypTime PidTag Submit timestamp Y
            SentRepresentingNameW           =   0x0042, //PtypString PidTag Sender representative name.
            MessageToMe                     =   0x0057, //PtypBoolean PidTag Whether recipient is in To : line Y
            MessageCcMe                     =   0x0058, //PtypBoolean PidTag Whether recipient is in Cc : line Y
            ConversationTopicW              =   0x0070, //PtypString PidTag Conversation topic.This property has
            ConversationIndex               =   0x0071, //PtypBinary PidTag Conversation index Y
            DisplayCcW                      =   0x0E03, //PtypString PidTag Cc : line.This property has an
            DisplayToW                      =   0x0E04, //PtypString PidTag To : line.This property has an
            MessageDeliveryTime             =   0x0E06, //PtypTime PidTag Message delivery timestamp Y
            MessageFlags                    =   0x0E07, //PtypInteger32
            MessageSize                     =   0x0E08, //PtypInteger32
            MessageStatus                   =   0x0E17, //PtypInteger32
            ReplCopiedfromVersionhistory    =   0x0E3C, // PtypBinary PidTag
            ReplCopiedfromItemid            =   0x0E3D, //PtypBinary PidTag Replication item ID information Y

            CreationTime                    =   0x3007, // 
            LastModificationTime            =   0x3008,
            SearchKey                       =   0x300b,

            RecipientType                   =   0x0c15, //PtypInteger32 PidTag Type of recipient.
            Responsibility                  =   0x0E0F, // PtypBoolean PidTag Handling Responsibility.
            ObjectType                      =   0x0FFE, //PtypInteger32 PidTag Recipient Object type.
            EntryId                         =   0x0FFF, //PtypBinary PidTag EntryID of the recipient.
            AddressType                     =   0x3002, // PtypString PidTag Type of recipient address.
            EmailAddress                    =   0x3003, // PtypString PidTag E - mail address of recipient.
            DisplayType                     =   0x3900, // PtypInteger32 PidTag Display type.
            SevenBitDisplayName             =   0x39FF, //PtypString PidTag 7 - bit Display name.
            SendRichInfo                    =   0x3A40, //PtypBoolean PidTag Send Rich info for recipient.



            Unknown				            =   0xFFFFFFF      
        };
	}
}

#endif // READER_TYPES_H