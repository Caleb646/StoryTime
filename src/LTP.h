#include <iostream>
#include <stdarg.h>
#include <cassert>
#include <vector>
#include <string>
#include <fstream>
#include <utility>
#include <algorithm>

#include "types.h"
#include "utils.h"
#include "core.h"
#include "NDB.h"

#ifndef READER_LTP_H
#define READER_LTP_H

namespace reader {
	namespace ltp {
		/**
		 * @brief = An HID is a 4-byte value that identifies an item allocated from the heap. 
		 * The value is unique only within the heap itself. The following is the structure of an HID. 
		*/
		class HID
		{
		public:
			HID() = default;
			HID(const std::vector<types::byte_t>& data)
				: m_hid(utils::slice(data, 0, 4, 4, utils::toT_l<size_t>))
			{
				_init();
			}
			/// @brief (5 bits) MUST be set to 0 (NID_TYPE_HID) to indicate a valid HID.
			int16_t getHIDType() const 
			{ 
				return m_hid & 0x1F;
			}
			/// @brief (11 bits) This is the 1-based index value that identifies 
			/// an item allocated from the heap node. This value MUST NOT be zero.
			size_t getHIDAllocIndex() const
			{ 
				size_t index = (m_hid >> 5) & 0x7FF;
				ASSERT((index != 0), "[ERROR] Invalid HID Index");
				return index;
			}
			/// @brief (16 bits) This is the zero-based data block index. 
			/// This number indicates the zerobased index of the data block in which this heap item resides.
			size_t getHIDBlockIndex() const 
			{ 
				return (m_hid >> 16) & 0xFFFF;
			}

			size_t getHIDRaw() const
			{
				return m_hid;
			}
			static constexpr size_t id() { return 10; }
		private:
			void _init()
			{
				ASSERT(((types::NIDType)getHIDType() == types::NIDType::HID),
					"[ERROR] Invalid HID Type");

				// hidRoot for the BTH Header can be 0 if the BTH is empty
				//ASSERT((getHIDAllocIndex() > 0), "[ERROR]");
			}
		private:
			size_t m_hid;
		};

		/**
		 * @brief = An HNID is a 32-bit hybrid value that represents either an HID or an NID.
		 * The determination is made by examining the hidType (or equivalently, nidType) value.
		 * The HNID refers to an HID if the hidType is NID_TYPE_HID. Otherwise, the HNID refers to an NID.
		 * An HNID that refers to an HID indicates that the item is stored in the data block. An HNID that
		 * refers to an NID indicates that the item is stored in the subnode block, and the NID is the local
		 * NID under the subnode where the raw data is located.
		*/
		class HNID
		{
		public:
			HNID() = default;
			HNID(const std::vector<types::byte_t>& data)
				: m_data(data)
			{
				ASSERT((m_data.size() == 4), "[ERROR]");
			}

			bool isHID() const
			{
				return (m_data[0] & 0x1F) == 0;
			}

			template<typename IDType>
			IDType as() const
			{
				/// The HNID refers to an HID if the hidType is NID_TYPE_HID. 
				/// Otherwise, the HNID refers to an NID. 
				if constexpr (IDType::id() == HID::id())
				{
					return HID(m_data);
				}
				/// The HNID refers to an HID if the hidType is NID_TYPE_HID. 
				/// Otherwise, the HNID refers to an NID. 
				else if constexpr (IDType::id() == core::NID::id())
				{
					return core::NID(m_data);
				}
			}
			size_t static constexpr size() { return 4; }
		private:
			std::vector<types::byte_t> m_data{};
		};

		/**
		 * @brief = The HNHDR record resides at the beginning of the first data block 
		 * in the HN (an HN can span several blocks), which contains root information about the HN. 
		*/
		struct HNHDR
		{
			/// (2 bytes): The byte offset to the HN page Map record (section 2.3.1.5), with respect to the 
			/// beginning of the HNHDR structure. 
			int64_t ibHnpm{};
			/// (1 byte): Block signature; MUST be set to 0xEC to indicate an HN. 
			int64_t bSig{};
			/// (1 byte): Client signature.
			int64_t bClientSig{};
			/// (4 bytes): HID that points to the User Root record. The User Root 
			/// record contains data that is specific to the higher level. 
			HID hidUserRoot{};
			/// (4 bytes): Per-block Fill Level Map. This array consists of eight 4-bit values that indicate the fill level 
			/// for each of the first 8 data blocks (including this header block). If the HN has fewer than 8 data blocks, 
			/// then the values corresponding to the non-existent data blocks MUST be set to zero. The following table explains 
			/// the values indicated by each 4-bit value. 
			std::vector<types::byte_t> rgbFillLevel{};
		};

		/**
		 * @brief = This is the header record used in subsequent data blocks of the HN that do not 
		 * require a new Fill Level Map (see next section). This is only used when multiple data blocks are present. 
		*/
		struct HNPageHDR
		{
			/// (2 bytes): The bytes offset to the HNPAGEMAP record (section 2.3.1.5), with respect to the beginning of the HNPAGEHDR structure.
			int32_t ibHnpm{};
		};

		/**
		 * @brief = Beginning with the eighth data block, a new Fill Level Map is required. 
		 * An HNBITMAPHDR fulfills this requirement. The Fill Level Map in the HNBITMAPHDR 
		 * can map 128 blocks. This means that an HNBITMAPHDR appears at data 
		 * block 8 (the first data block is data block 0) and thereafter every 128 blocks. 
		 * (that is, data block 8, data block 136, data block 264, and so on). 
		*/
		struct HNBitMapHDR
		{
			/// (2 bytes): The byte offset to the HNPAGEMAP record (section 2.3.1.5) relative to the beginning of the HNPAGEHDR structure. 
			uint16_t ibHnpm{};
			/// (64 bytes): Per-block Fill Level Map. This array consists of one hundred and twentyeight (128) 4-bit values 
			/// that indicate the fill level for the next 128 data blocks (including this data block). 
			/// If the HN has fewer than 128 data blocks after this data block, then the values corresponding to the 
			/// non-existent data blocks MUST be set to zero. See rgbFillLevel in section 2.3.1.2 for possible values. 
			std::vector<types::byte_t> rgbFillLevel{64};
		};

		/**
		 * @brief = The HNPAGEMAP is the last item in the variable length data portion of the 
		 * block immediately following the last heap item. There can be anywhere from 0 to 63 bytes 
		 * of padding between the HNPAGEMAP and the block trailer. The beginning of the HNPAGEMAP 
		 * is aligned on a 2-byte boundary so there can be an additional 1 byte of padding between 
		 * the last heap item and the HNPAGEMAP. 
		 * The HNPAGEMAP structure contains the information about the allocations in the page. 
		 * The HNPAGEMAP is located using the ibHnpm field in the HNHDR, HNPAGEHDR and HNBITMAPHDR records. 
		*/
		struct HNPageMap
		{
			/// (2 bytes): Allocation count. This represents the number of items (allocations) in the HN. 
			int32_t cAlloc{};
			/// (2 bytes): Free count. This represents the number of freed items in the HN.
			int32_t cFree{};
			/// (variable): Allocation table. This contains cAlloc + 1 entries. 
			/// Each entry is a WORD value that is the byte offset to the beginning of the allocation. 
			/// An extra entry exists at the cAlloc + 1st position to mark the offset of the next available slot. 
			/// Therefore, the nth allocation starts at offset rgibAlloc[n] (from the beginning of the HN header), 
			/// and its size is calculated as rgibAlloc[n + 1] – rgibAlloc[n] bytes. 
			/// The size of each entry in the PST is 2 bytes.
			std::vector<uint32_t> rgibAlloc{};
		};

		/**
		 * @brief = The BTHHEADER contains the BTH metadata, which instructs the reader how 
		 * to access the other objects of the BTH structure.
		*/
		struct BTHHeader
		{
			/// (1 byte): MUST be bTypeBTH. 
			int16_t bType{};
			/// (1 byte): Size of the BTree Key value, in bytes. This value MUST be set to 2, 4, 8, or 16.
			int16_t cbKey{};
			/// (1 byte): Size of the data value, in bytes. This MUST be greater than zero and less than or equal to 32.
			int16_t cbEnt{};
			/// (1 byte): Index depth. This number indicates how many levels of intermediate indices 
			/// exist in the BTH. Note that this number is zero-based, meaning that a value of zero 
			/// actually means that the BTH has one level of indices. If this value is greater than zero, 
			/// then its value indicates how many intermediate index levels are present. 
			int16_t bIdxLevels{};
			/// (4 bytes): This is the HID that points to the BTH entries for this BTHHEADER. 
			/// The data consists of an array of BTH records. This value is set to zero if the BTH is empty. 
			HID hidRoot{};
		};

		/**
		 * @brief = Index records do not contain actual data, but point to other index records or leaf records. 
		 * The format of the intermediate index record is as follows. The number of index records can be 
		 * determined based on the size of the heap allocation.
		*/
		struct IntermediateBTHRecord
		{
			/// (variable): This is the key of the first record in the next level index record array. 
			/// The size of the key is specified in the cbKey field in the corresponding BTHHEADER 
			/// structure (section 2.3.2.1). The size and contents of the key are specific to the 
			/// higher level structure that implements this BTH. 
			std::vector<types::byte_t> key{};
			/// (4 bytes): HID of the next level index record array. This contains the HID of the heap 
			/// item that contains the next level index record array.
			HID hidNextLevel{};

			static constexpr size_t id() { return 6;}
		};

		/**
		 * @brief = Leaf BTH records contain the actual data associated with each key entry. 
		 * The BTH records are tightly packed (that is, byte-aligned), and each 
		 * record is exactly cbKey + cbEnt bytes in size. The number of data records 
		 * can be determined based on the size of the heap allocation.
		*/
		struct LeafBTHRecord
		{
			/// (variable): This is the key of the record. The size of the key is specified 
			/// in the cbKey field in the corresponding BTHHEADER structure(section 2.3.2.1). 
			/// The size and contents of the key are specific to the higher level 
			/// structure that implements this BTH.
			std::vector<types::byte_t> key{};
			/// (variable) : This contains the actual data associated with the key.The size of the data is
			///	specified in the cbEnt field in the corresponding BTHHEADER structure.The size and contents of
			///	the data are specific to the higher level structure that implements this BTH.
			std::vector<types::byte_t> data{};

			static constexpr size_t id() { return 7; }
		};

		/**
		* @brief
		*/
		struct PCBTHRecord
		{
			/// (2 bytes): Property ID, as specified in [MS-OXCDATA] section 2.9. This is the upper 16 bits of the 
			/// property tag value. This is a manifestation of the BTH record (section 2.3.2.3) 
			/// and constitutes the key of this record. 
			uint32_t wPropId{};

			/// (2 bytes): Property ID, as specified in [MS-OXCDATA] section 2.9. This is the 
			/// upper 16 bits of the property tag value. This is a manifestation of the 
			/// BTH record (section 2.3.2.3) and constitutes the key of this record.
			uint32_t wPropType{};

			/// (4 bytes): Depending on the data size of the property type indicated 
			/// by wPropType and a few other factors, this field represents different values. 
			/// The following table explains the value contained in dwValueHnid based on the 
			/// different scenarios. An HNID that refers to an HID indicates that the item 
			/// is stored in the data block. An HNID that refers
			/// to an NID indicates that the item is stored in the subnode block, and the 
			/// NID is the local NID under
			///	the subnode where the raw data is located.
			std::vector<types::byte_t> dwValueHnid{};
			//HNID dwValueHnid{};

			/*
			* Variable size?	Fixed data size		NID_TYPE(dwValueHnid) == NID_TYPE_HID?	 dwValueHnid
				N					<= 4 bytes				-								 	Data Value
									> 4 bytes				Y								 	HID
				Y					-						Y								 	HID (<= 3580 bytes)
															N								 	NID (subnode, > 3580 bytes)
			*/
			static constexpr size_t id() { return 11; }
		};

		/**
		 * @brief = The Property Context is built directly on top of a BTH. 
		 * The existence of a PC is indicated at the HN level, where bClientSig 
		 * is set to bTypePC. Implementation-wise, the PC is simply a BTH with 
		 * cbKey set to 2 and cbEnt set to 6 (see section 2.3.3.3). 
		 * The following section explains the layout of a PC BTH record. 
		 * 
		 * Each property is stored as an entry in the BTH. Accessing a 
		 * specific property is just a matter of searching the BTH for a 
		 * key that matches the property identifier of the 
		 * desired property, as the following data structure illustrates.
		 * 
		 * Accessing the PC BTHHEADER 
		 * The BTHHEADER structure of a PC is accessed through the hidUserRoot of the HNHDR structure of the containing HN.
		*/

		/**
		 * @brief When a property of type PtypObject is stored in a PC, the dwValueHnid value 
		 * described in section 2.3.3.3 points to a heap allocation that contains a structure 
		 * that defines the size and location of the object data.
		*/
		struct PtypObjectProperty
		{
			/// (4 bytes): The subnode identifier that contains the object data. 
			core::NID nid{};
			/// (4 bytes): The total size of the object.
			int64_t ulSize{};
		};

		/**
		 * @brief The TCOLDESC structure describes a single column in the TC,
		 * which includes metadata about the size of the data associated with this column,
		 * as well as whether a column exists, and how to locate the column data from the Row Matrix.
		*/
		struct TColDesc
		{
			/// (4 bytes): This field specifies that 32-bit tag 
			/// that is associated with the column. 
			uint32_t tag{};
			/// (2 bytes): Data Offset. This field indicates the offset 
			/// from the beginning of the row data (in the Row Matrix) 
			/// where the data for this column can be retrieved. Because 
			/// each data row is laid out the same way in the Row Matrix, 
			/// the Column data for each row can be found at the same offset.
			uint16_t ibData{};
			/// (1 byte): Data size. This field specifies the size of the data 
			/// associated with this column (that is, "width" of the column), 
			/// in bytes per row. However, in the case of variable-sized data, 
			/// this value is set to the size of an HNID instead.
			uint8_t cbData{};
			/// (1 byte): Cell Existence Bitmap Index. This value is the 0-based 
			/// index into the CEB bit that corresponds to this Column. A detailed 
			/// explanation of the mapping mechanism will be discussed in section 2.3.4.4.1. 
			uint8_t iBit{};

			//uint32_t propId() const noexcept { return tag & 0xFFFF0000; }
			uint32_t propId() const noexcept { return (tag & 0xFFFF0000) >> 16; }
			uint32_t propType() const noexcept { return tag & 0xFFFF; }
		};

		/**
		 * @brief TCINFO is the header structure for the TC. The TCINFO is 
		 * accessed using the hidUserRoot field in the HNHDR structure of 
		 * the containing HN. The header contains the column definitions and other relevant data.
		*/
		struct TCInfo
		{
			/// (1 byte): TC signature; MUST be set to bTypeTC.
			types::BType bType{};
			/// (1 byte): Column count. This specifies the number of columns in the TC.
			uint8_t cCols{};
			/// (8 bytes): This is an array of 4 16-bit values that specify the 
			/// offsets of various groups of data in the actual row data. 
			/// The application of this array is specified in section 2.3.4.4, 
			/// which covers the data layout of the Row Matrix. The following 
			/// table lists the meaning of each value: 
			std::vector<uint16_t> rgib{};

			/*
			*	Index	Friendly name	Meaning of rgib[Index] value 
			*	0		TCI_4b				Ending offset of 8- and 4-byte data value group. 
			*	1		TCI_2b				Ending offset of 2-byte data value group. 
			*	2		TCI_1b				Ending offset of 1-byte data value group. 
			*	3		TCI_bm				Ending offset of the Cell Existence Block. 
			*/

			///  (4 bytes): HID to the Row ID BTH. The Row ID BTH contains (RowID, RowIndex) 
			/// value pairs that correspond to each row of the TC. The RowID is a value that is 
			/// associated with the row identified by the RowIndex, whose meaning depends on the higher 
			/// level structure that implements this TC. The RowIndex is the zero-based index to a 
			/// particular row in the Row Matrix. 
			HID hidRowIndex{};
			/// (4 bytes): HNID to the Row Matrix (that is, actual table data). 
			/// This value is set to zero if the TC contains no rows. 
			HNID hnidRows{};
			///  (4 bytes): Deprecated. Implementations SHOULD ignore this value, 
			/// and creators of a new PST MUST set this value to zero. 
			uint32_t hidIndex{}; /// NOT USED
			/// (variable): Array of Column Descriptors. This array contains cCols 
			/// entries of type TCOLDESC structures that define each TC column. 
			/// The entries in this array MUST be sorted by the tag field of TCOLDESC. 
			std::vector<TColDesc> rgTCOLDESC{};

			/// Ending offset of 8- and 4-byte data value group.
			constexpr static size_t TCI_4b = 0;
			/// Ending offset of 2-byte data value group.
			constexpr static size_t TCI_2b = 1;
			/// Ending offset of 1-byte data value group.
			constexpr static size_t TCI_1b = 2;
			/// Ending offset of the Cell Existence Block.
			constexpr static size_t TCI_bm = 3;
		};

		/**
		 * @brief The TCROWID structure is a manifestation of the BTH data record (section 2.3.2.3). 
		 * The size of the TCROWID structure varies depending on the version of the PST. For the Unicode PST, 
		 * each record in the BTH are 8 bytes in size, where cbKey is set to 4 and cEnt is set to 4. 
		 * For an ANSI PST, each record is 6 bytes in size, where cbKey is set to 4 and cEnt is set to 2. 
		 * The following is the binary layout of the TCROWID structure. 
		*/
		struct TCRowID
		{
			/// (4 bytes) : This is  the 32 - bit primary key value that uniquely identifies a row in the Row Matrix.
			uint32_t dwRowID{};
			/// (Unicode: 4 bytes): The 0-based index to the corresponding row 
			/// in the Row Matrix. Note that for ANSI PSTs, the maximum number of rows is 2^16.
			uint32_t dwRowIndex{};
		};

		/**
		 * @brief The following is the organization of a single row of data in the Row Matrix. 
		 * Rows of data are tightlypacked in the Row Matrix, and the size of each data 
		 * row is TCINFO.rgib[TCI_bm] bytes. The following constraints exist for the columns within the structure. 
		 * Columns MUST be sorted:
			1. PidTagLtpRowId MUST be assigned iBit == 0
			2. PidTagLtpRowId MUST be assigned ibData == 0 
			3. PidTagLtpRowVer MUST be assigned iBit == 1 
			4. PidTagLtpRowVer MUST be assigned ibData == 4
			5. For any other columns, iBit can change/be any valid value (other than 0 and 1)
			6. For any other columns, ibData can be any valid value (other than 0 and 4) 
		*/
		class RowData
		{
		public:
			TCInfo tcInfo{};

			/// (4 bytes): The 32-bit value that corresponds to the dwRowID 
			/// value in this row's corresponding TCROWID record. 
			/// Note that this value corresponds to the PidTagLtpRowId property.
			uint32_t dwRowID{};

			/// (variable): 4-byte-aligned Column data. This region contains 
			/// data with a size that is a multiple of 4 bytes. The types of 
			/// data stored in this region are 4-byte and 8-byte values.
			//std::vector<types::byte_t> rgdwData{};

			/// (variable): 2-byte-aligned Column data. This region contains 
			/// data that are 2 bytes in size. 
			//std::vector<types::byte_t> rgwData{};

			/// (variable): Byte-aligned Column data. This region 
			/// contains data that are byte-sized. 
			//std::vector<types::byte_t> rgbData{};

			/// (variable): Cell Existence Block. This array of bits comprises the CEB, 
			/// in which each bit corresponds to a particular Column in the current row. 
			/// The mapping between CEB bits and actual Columns is based on the iBit 
			/// member of each TCOLDESC (section 2.3.4.2), where an iBit value of zero 
			/// maps to the Most Significant Bit (MSB) of the 0th byte of the CEB array (rgCEB[0]). 
			/// Subsequent iBit values map to the next less-significant bit until 
			/// the Least Significant Bit (LSB) is reached, where the subsequent iBit can 
			/// be found in the MSB of the next byte in the CEB array and the process repeats itself. 
			/// Programmatically, the Cell Existence Bit that corresponds to iBit can be extracted 
			/// as follows:  BOOL fCEB = !!(rgCEB[iBit / 8] & (1 << (7 - (iBit % 8)))); 
			/// Space is reserved for a column in the Row Matrix, regardless of the corresponding CEB bit value 
			/// for that column. Specifically, an fCEB bit value of TRUE indicates that the corresponding 
			/// column value in the Row matrix is valid and SHOULD be returned if requested. However, an 
			/// fCEB bit value of false indicates that the corresponding column value in the Row matrix 
			/// is "not set" or "invalid". In this case, the property MUST be "not found" if requested. 
			/// The size of rgCEB is CEIL(TCINFO.cCols / 8) bytes. Extra lower-order bits SHOULD be ignored. 
			/// Creators of a new PST MUST set the extra lower-order bits to zero.
			std::vector<types::byte_t> rgbCEB{};

			/// Eeverything from the start of Row Data to the start of rgbCEB
			std::vector<types::byte_t> data{};
		public:
			RowData(const std::vector<types::byte_t>& rowBytes, const TCInfo& header)
			{
				size_t offset4b = header.rgib.at(TCInfo::TCI_4b);
				size_t offset2b = header.rgib.at(TCInfo::TCI_2b);
				size_t offset1b = header.rgib.at(TCInfo::TCI_1b);
				size_t totalRowSize = header.rgib.at(TCInfo::TCI_bm);

				ASSERT((offset4b <= offset2b), "[ERROR]");
				ASSERT((offset2b <= offset1b), "[ERROR]");
				ASSERT((offset1b < totalRowSize), "[ERROR]");

				tcInfo = header;
				dwRowID = utils::slice(rowBytes, 0, 4, 4, utils::toT_l<uint32_t>);
				// Read everything from the start of Row Data to the start of rgbCEB
				data = utils::slice(rowBytes, 0ull, offset1b, offset1b);
				//row.rgdwData = utils::slice(rowBytes, 4ull, offset4b, offset4b - 4);
				//row.rgwData = utils::slice(rowBytes, offset4b, offset2b, offset2b - offset4b);
				//row.rgbData = utils::slice(rowBytes, offset2b, offset1b, offset2b - offset1b);
				rgbCEB = utils::slice(rowBytes, offset1b, totalRowSize, totalRowSize - offset1b);

				//ASSERT((dwRowID == rowID.dwRowID), "[ERROR]");
				ASSERT((rgbCEB.size() == static_cast<size_t>(std::ceil(static_cast<double>(header.cCols) / 8.0))), "[ERROR]");
			}

		public:

			bool exists(uint8_t iBit) const
			{
				return (rgbCEB[iBit / 8] & (1 << (7 - (iBit % 8))));
			}
		};


		class Record
		{
		public:
			Record(const std::vector<types::byte_t>& data, size_t keySize, size_t dataSize = 0)
				: m_data(data), m_keySize(keySize), m_dataSize(dataSize) {}

			static IntermediateBTHRecord asIntermediateBTHRecord(
				const std::vector<types::byte_t>& bytes,
				size_t keySize
			)
			{
				ASSERT((keySize + 4 == bytes.size()), "[ERROR] Invalid size");
				IntermediateBTHRecord record{};
				record.key = utils::slice(bytes, 0ull, keySize, keySize);
				record.hidNextLevel = HID(utils::slice(
					bytes,
					keySize,
					keySize + 4ull,
					4ull
				));
				return record;
			}

			static LeafBTHRecord asLeafBTHRecord(
				const std::vector<types::byte_t>& bytes,
				size_t keySize,
				size_t dataSize
			)
			{
				ASSERT((bytes.size() == keySize + dataSize), "[ERROR] Invalid size");
				LeafBTHRecord record{};
				record.key = utils::slice(bytes, 0ull, keySize, keySize);
				record.data = utils::slice(bytes, keySize, keySize + dataSize, dataSize);
				return record;
			}

			static PCBTHRecord asPCBTHRecord(
				const std::vector<types::byte_t>& bytes,
				size_t keySize,
				size_t dataSize
			)
			{
				ASSERT((bytes.size() == 8), "[ERROR]");
				ASSERT((keySize == 2), "[ERROR]");
				ASSERT((dataSize == 6), "[ERROR]");
				PCBTHRecord record{};
				record.wPropId = utils::slice(bytes, 0, 2, 2, utils::toT_l<uint32_t>);
				record.wPropType = utils::slice(bytes, 2, 4, 2, utils::toT_l<uint32_t>);
				record.dwValueHnid = utils::slice(bytes, 4, 8, 4);
				//record.dwValueHnid = HNID(utils::slice(bytes, 4, 8, 4));

				ASSERT((utils::isIn(record.wPropType, utils::PROPERTY_TYPE_VALUES)), "[ERROR] Invalid property type");
				return record;
			}

			static TCRowID asTCRowID(
				const std::vector<types::byte_t>& bytes,
				size_t keySize,
				size_t dataSize
			)
			{
				ASSERT((bytes.size() == 8), "[ERROR]");
				ASSERT((keySize == 4), "[ERROR]");
				ASSERT((dataSize == 4), "[ERROR]");
				TCRowID record{};
				record.dwRowID = utils::slice(bytes, 0, 4, 4, utils::toT_l<uint32_t>);
				record.dwRowIndex = utils::slice(bytes, 4, 8, 4, utils::toT_l<uint32_t>);
				return record;
			}

			template<typename RecordType>
			RecordType as() const
			{
				if constexpr (std::is_same_v<RecordType, LeafBTHRecord>)
					return asLeafBTHRecord(m_data, m_keySize, m_dataSize);

				else if constexpr (std::is_same_v<RecordType, IntermediateBTHRecord>)
					return asIntermediateBTHRecord(m_data, m_keySize);

				else if constexpr (std::is_same_v<RecordType, PCBTHRecord>)
					return asPCBTHRecord(m_data, m_keySize, m_dataSize);

				else if constexpr (std::is_same_v<RecordType, TCRowID>)
					return asTCRowID(m_data, m_keySize, m_dataSize);

				else
					ASSERT(false, "[ERROR] Invalid Record Type");
			}
		private:
			size_t m_keySize{};
			size_t m_dataSize{};
			std::vector<types::byte_t> m_data{};
		};

		struct HNBlock
		{
			HNPageMap map{};
			/// won't be present in the first block
			HNPageHDR pheader{}; 
			/// will only be present in 8th block and every 8 + 128 block thereafter
			HNBitMapHDR bmheader{};
			/// this data is not trimmed. It includes everything
			std::vector<types::byte_t> data{};
		};

		/**
		 * @brief The Heap-on-Node defines a standard heap over a node's data stream. 
		 * Taking advantage of the flexible structure of the node, the organization 
		 * of the heap data can take on several forms, depending on how much data 
		 * is stored in the heap. For heaps whose size exceed the amount of data 
		 * that can fit in one data block, the first data block in the HN contains a 
		 * full header record and a trailer record. With the exception of blocks that 
		 * require an HNBITMAPHDR structure, subsequent data blocks only have an abridged 
		 * header and a trailer. This is explained in more detail in the following sections. 
		 * Because the heap is a structure that is defined at a higher layer than the NDB, 
		 * the heap structures are written to the external data sections of data blocks 
		 * and do not use any information from the data block's NDB structure. 
		*/
		class HN 
		{
		public:
			//HN() = default;

			static HN Init(core::NID nid, core::Ref<const ndb::NDB> ndb)
			{
				static_assert(std::is_move_constructible_v<HN>, "HN must be move constructible");
				static_assert(std::is_move_assignable_v<HN>, "HN must be move assignable");
				static_assert(std::is_copy_constructible_v<HN>, "HN must be copy constructible");
				static_assert(std::is_copy_assignable_v<HN>, "HN must be copy assignable");
				ndb::NBTEntry nbt = ndb->get(nid);
				ndb::BBTEntry bbt = ndb->get(nbt.bidData);
				return HN(nid, ndb->InitDataTree(bbt.bref, bbt.cb));
			}

			bool addBlock(const std::vector<types::byte_t>& data, size_t blockIdx)
			{
				ASSERT((m_blocks.size() == blockIdx), "[ERROR] Invalid blockIdx");
				if(blockIdx == 8 || blockIdx % 8 + 128 == 0)
				{
					HNBitMapHDR bmheader = readHNBitMapHDR(data);
					HNPageMap map = readHNPageMap(data, bmheader.ibHnpm);
					HNBlock block{};
					block.bmheader = bmheader;
					block.map = map;
					block.data = data;
					m_blocks.push_back(block);
				}
				else
				{
					HNPageHDR pheader = readHNPageHDR(data);
					HNPageMap map = readHNPageMap(data, pheader.ibHnpm);
					HNBlock block{};
					block.pheader = pheader;
					block.map = map;
					block.data = data;
					m_blocks.push_back(block);
				}
				return true;
			}

			types::BType getBType() const { return utils::toBType(m_hnhdr.bClientSig); }

			HNHDR getHeader() const { return m_hnhdr; }

			uint32_t offset(size_t blockIdx, size_t pageIdx)
			{
				ASSERT((blockIdx < m_blocks.size()), "[ERROR] Invalid blockIdx");
				ASSERT( (pageIdx < m_blocks.at(blockIdx).map.rgibAlloc.size() ), "[ERROR] Invalid pageIdx");
				return m_blocks.at(blockIdx).map.rgibAlloc.at(pageIdx);
			}

			HNBlock at(size_t blockIdx) const { return m_blocks.at(blockIdx); }
			size_t blockSize(HID hid) const { return blockSize(hid.getHIDBlockIndex()); }
			size_t blockSize(size_t blockIdx) const { return at(blockIdx).data.size(); }

			size_t allocSize(size_t blockIdx, size_t pageIdx)
			{
				HNBlock block = at(blockIdx);
				return (size_t)block.map.rgibAlloc.at(pageIdx + 1) - (size_t)block.map.rgibAlloc.at(pageIdx);
			}

			std::vector<types::byte_t> slice(size_t blockIdx, size_t pageIdx, size_t size)
			{
				HNBlock block = at(blockIdx);
				size_t offset = static_cast<size_t>(block.map.rgibAlloc.at(pageIdx));
				return utils::slice(block.data, offset, static_cast<size_t>(offset + size), size);
			}

			std::vector<types::byte_t> alloc(const HID& hid) const
			{
				size_t blockIdx = hid.getHIDBlockIndex();
				size_t pageIdx = hid.getHIDAllocIndex();
				HNBlock block = at(blockIdx);
				size_t start = static_cast<size_t>(block.map.rgibAlloc.at(pageIdx - 1));
				size_t end = static_cast<size_t>(block.map.rgibAlloc.at(pageIdx));
				size_t size = end - start;
				return utils::slice(block.data, start, end, size);
			}

			/**
			 * @brief Slices the block's data at blockIdx into 
			 * allocations specificed by the block's map.
			 * @param blockIdx 
			 * @return 
			*/
			std::vector<std::vector<types::byte_t>> allocs(size_t blockIdx)
			{
				HNBlock block = at(blockIdx);
				ASSERT((block.map.rgibAlloc.size() > 0), "[ERROR] Invalid number of allocations");
				std::vector<std::vector<types::byte_t>> allocs{};
				for (size_t i = 1; i < block.map.rgibAlloc.size(); i++)
				{
					size_t start = static_cast<size_t>(block.map.rgibAlloc.at(i - 1));
					size_t end = static_cast<size_t>(block.map.rgibAlloc.at(i));
					size_t size = end - start;
					allocs.push_back(utils::slice(block.data, start, end, size));
				}
				ASSERT((allocs.size() == block.map.rgibAlloc.size() - 1), "[ERROR] Invalid number of allocations");
				return allocs;
			}

			size_t nblocks() const { return m_blocks.size(); }

			static HNHDR readHNHDR(const std::vector<types::byte_t>& bytes, size_t dataBlockIdx, int64_t nDataBlocks)
			{
				LOG("[INFO] Data Block Size: %i", bytes.size());
				ASSERT((dataBlockIdx == 0), "[ERROR] Only the first data block contains a HNHDR");
				HNHDR hnhdr{};
				hnhdr.ibHnpm = utils::slice(bytes, 0, 2, 2, utils::toT_l<uint32_t>);
				hnhdr.bSig = utils::slice(bytes, 2, 3, 1, utils::toT_l<uint32_t>);
				hnhdr.bClientSig = utils::slice(bytes, 3, 4, 1, utils::toT_l<uint32_t>);
				hnhdr.hidUserRoot = HID(utils::slice(bytes, 4, 8, 4));
				hnhdr.rgbFillLevel = utils::toBits(utils::slice(bytes, 8, 12, 4, utils::toT_l<uint32_t>), 4);

				uint32_t hidType = hnhdr.hidUserRoot.getHIDType();
				ASSERT((hidType == 0), "[ERROR] Invalid HID Type %i", hidType)
					ASSERT(std::cmp_equal(hnhdr.bSig, 0xEC), "[ERROR] Invalid HN signature");
				ASSERT((utils::isIn(hnhdr.bClientSig, utils::BTYPE_VALUES)), "[ERROR] Invalid BType");
				ASSERT((hnhdr.rgbFillLevel.size() == 8), "[ERROR] Invalid Fill Level size must be 8");

				if (nDataBlocks < 8)
				{
					for (int64_t i = nDataBlocks; i < 8; i++)
					{
						ASSERT(((int16_t)hnhdr.rgbFillLevel[i] == (int16_t)types::FillLevel::LEVEL_EMPTY),
							"[ERROR] Fill level must be empty for block at idx [%i]", i);
					}
				}
				return hnhdr;
			}

			static HNPageMap readHNPageMap(const std::vector<types::byte_t>& bytes, int64_t start)
			{
				HNPageMap pg{};
				pg.cAlloc = utils::slice(bytes, start, start + 2ll, 2ll, utils::toT_l<decltype(pg.cAlloc)>);
				pg.cFree = utils::slice(bytes, start + 2ll, start + 4ll, 2ll, utils::toT_l<decltype(pg.cFree)>);

				std::vector<types::byte_t> allocBytes = utils::slice(
					bytes,
					start + 4ll,
					start + 4ll + pg.cAlloc * 2ll + 2ll,
					pg.cAlloc * 2ll + 2ll
				);

				// Each rgibAlloc entry is 2 bytes with cAlloc + 1 entries. 
				ASSERT((allocBytes.size() % 2 == 0), "[ERROR] Size should always be even");
				for (size_t i = 2; i < allocBytes.size() + 1; i += 2)
				{
					pg.rgibAlloc.push_back(utils::slice(allocBytes, i - 2ull, i, 2ull, utils::toT_l<uint32_t>));
				}
				ASSERT((pg.rgibAlloc.size() == pg.cAlloc + 1), "[ERROR] Should be cAlloc + 1 entries");

				for (size_t i = 1; i < pg.rgibAlloc.size(); i++)
				{
					ASSERT((pg.rgibAlloc[i] > pg.rgibAlloc[i - 1]), "[ERROR]");
				}
				ASSERT((pg.rgibAlloc.at(pg.rgibAlloc.size() - 1) == start), "[ERROR]");
				// 12 is the size of the HNHDR Header
				// 4 is the combined size of cAlloc and cFree
				//const uint64_t size = pg.rgibAlloc.back() - pg.rgibAlloc.front();
				//const uint64_t sizeWithHeader = bytes.size() - 12 - 4 - allocBytes.size();
				//const uint64_t sizeWithoutHeader = bytes.size() - 4 - allocBytes.size();
				// There can be anywhere from 0 to 63 bytes of padding between the HNPAGEMAP and the block trailer.
				// The bytes vector doesn't include the bytes for the block trailer but it does include the variable padding
				//ASSERT((size == sizeWithHeader || size == sizeWithoutHeader), "[ERROR] Invalid size");
				return pg;
			}

			static HNPageHDR readHNPageHDR(const std::vector<types::byte_t>& bytes)
			{
				// HNPageHDR should be the first 2 bytes in bytes vector
				//ASSERT((bytes.size() == 2), "[ERROR] Invalid size");
				HNPageHDR hdr{};
				hdr.ibHnpm = utils::slice(bytes, 0, 2, 2, utils::toT_l<int32_t>);
				return hdr;
			}

			static HNBitMapHDR readHNBitMapHDR(const std::vector<types::byte_t>& bytes)
			{
				ASSERT((bytes.size() == 66), "[ERROR] Invalid size");
				HNBitMapHDR hdr{};
				hdr.ibHnpm = utils::slice(bytes, 0, 2, 2, utils::toT_l<uint16_t>);

				std::vector<types::byte_t> fillBytes = utils::slice(bytes, 2, 66, 64);
				for (size_t i = 0; i < fillBytes.size(); i++)
				{
					uint8_t byte = fillBytes[i];
					hdr.rgbFillLevel.push_back(byte & 0x0F); // least significant 4 bits first
					hdr.rgbFillLevel.push_back(byte & 0xF0); // most significant 4 bits second
				}
				ASSERT((hdr.rgbFillLevel.size() == 128), "[ERROR]");
				return hdr;
			}

			private:
				HN(core::NID nid, ndb::DataTree&& dTree)
					: m_nid(nid), m_dataTree(dTree)
				{
					// The first call to blocks fetches all of them
					const std::vector<ndb::DataBlock>& dBlocks = m_dataTree.blocks();
					const ndb::DataBlock firstDBlock = dBlocks.at(0);

					m_hnhdr = readHNHDR(firstDBlock.data, 0, dBlocks.size());

					HNBlock block{};
					block.map = readHNPageMap(firstDBlock.data, m_hnhdr.ibHnpm);
					block.data = firstDBlock.data;
					m_blocks.push_back(block);

					for (size_t i = 1; i < dBlocks.size(); i++)
					{
						addBlock(dBlocks.at(i).data, i);
					}
				}

			private:
				core::NID m_nid;
				HNHDR m_hnhdr;
				ndb::DataTree m_dataTree;
				std::vector<HNBlock> m_blocks;
		};

		class BTreeHeap
		{
		public:
			//BTreeHeap() = default;
			BTreeHeap(core::Ref<const HN> hn, HID bthHeaderHID) : m_hn(hn) 
			{ 
				static_assert(std::is_move_constructible_v<BTreeHeap>, "BTreeHeap must be move constructible");
				static_assert(std::is_move_assignable_v<BTreeHeap>, "BTreeHeap must be move assignable");
				static_assert(std::is_copy_constructible_v<BTreeHeap>, "BTreeHeap must be copy constructible");
				static_assert(std::is_copy_assignable_v<BTreeHeap>, "BTreeHeap must be copy assignable");
				_init(bthHeaderHID);
			}

			static BTHHeader readBTHHeader(const std::vector<types::byte_t>& bytes)
			{
				ASSERT((bytes.size() == 8), "[ERROR]");
				BTHHeader header{};
				header.bType = utils::slice(bytes, 0, 1, 1, utils::toT_l<int16_t>);
				header.cbKey = utils::slice(bytes, 1, 2, 1, utils::toT_l<int16_t>);
				header.cbEnt = utils::slice(bytes, 2, 3, 1, utils::toT_l<int16_t>);
				header.bIdxLevels = utils::slice(bytes, 3, 4, 1, utils::toT_l<int16_t>);
				/// hidRoot can be equal to zero if the BTH is empty
				header.hidRoot = HID(utils::slice(bytes, 4, 8, 4));

				ASSERT(((types::BType)header.bType == types::BType::BTH), "[ERROR] Invalid BTree on Heap");
				ASSERT((utils::isIn(header.cbKey, { 2, 4, 8, 16 })), "[ERROR]");
				ASSERT((header.cbEnt > 0 && header.cbEnt <= 32), "[ERROR]");

				if (header.hidRoot.getHIDRaw() > 0)
				{
					ASSERT( (header.hidRoot.getHIDAllocIndex() > 0), "[ERROR] Invalid Allocation Index");
				}
				return header;
			}

			static std::vector<Record> readBTHRecords(
				const std::vector<types::byte_t>& bytes,
				size_t keySize,
				//size_t bIdxLevels,
				size_t dataSize = 0)
			{
				size_t rsize = keySize + dataSize;
				size_t nRecords = bytes.size() / rsize;
				ASSERT( (bytes.size() % rsize == 0), "[ERROR] nRecords must be a mutiple of keySize + dataSize");
				std::vector<Record> records{};
				records.reserve(nRecords);
				for (size_t i = 0; i < nRecords; i++)
				{
					size_t start = i * rsize;
					size_t end = start + rsize;
					ASSERT((end - start == rsize), "[ERROR] Invalid size");
					records.push_back(Record(
							utils::slice(bytes, start, end, rsize),
							keySize,
							dataSize
						)
					);
				}
				ASSERT((records.size() == nRecords), "[ERROR] Invalid size");
				return records;
			}

			bool empty() const
			{
				return m_header.hidRoot.getHIDRaw() == 0;
			}

			HID hidRoot() const
			{
				return m_header.hidRoot;
			}

			size_t keySize() const
			{
				return m_header.cbKey;
			}
			size_t dataSize() const
			{
				return m_header.cbEnt;
			}

			size_t nrecords() const
			{
				return m_records.size();
			}

			std::vector<Record> records() const
			{
				return m_records;
			}

		private:
			void _init(HID bthHeaderHID)
			{
				std::vector<types::byte_t> headerBytes = m_hn->alloc(bthHeaderHID);
				m_header = readBTHHeader(headerBytes);

				if (m_header.hidRoot.getHIDRaw() > 0) // If hidRoot is zero, the BTH is empty
				{
					m_records = readBTHRecords(
						m_hn->alloc(m_header.hidRoot),
						m_header.cbKey,
						//m_header.bIdxLevels,
						m_header.cbEnt
					);
				}

			}
		private:
			core::Ref<const HN> m_hn;
			BTHHeader m_header;
			std::vector<Record> m_records;
		};

		/**
		 * @brief When the MV property contains variable-size elements,
		 * such as PtypBinary, PtypString, or PtypString8), the data layout
		 * is more complex. The following is the data format of a multi-valued
		 * property with variable-size base type.
		*/
		struct PTMultiValue
		{
			/// (4 bytes): Number of data items in the array.
			uint32_t ulCount{};
			/// (variable): An array of ULONG values that represent offsets to the start of each
			/// data item for the MV array.Offsets are relative to the beginning of the MV property data record.
		    ///	The length of the Nth data item is calculated as : rgulDataOffsets[N + 1] – rgulDataOffsets[N],
			///	with the exception of the last item, in which the total size of the MV property data record is used
			///	instead of rgulDataOffsets[N + 1].
			/// Contains ulCount + 1 offsets because I add data.size() as the last offset
			std::vector<uint32_t> rgulDataOffsets{};
			/// (variable): A byte-aligned array of data items. Individual items are delineated using
			/// the rgulDataOffsets values.
			std::vector<types::byte_t> rgDataItems{};

			static PTMultiValue readPTMV(const std::vector<types::byte_t>& data)
			{
				PTMultiValue mv{};
				mv.ulCount = utils::slice(data, 0, 4, 4, utils::toT_l<uint32_t>);
				
				size_t offsetSize = mv.ulCount * sizeof(uint32_t);
				std::vector<types::byte_t> offsetBytes = utils::slice(data, 4ull, 4ull + offsetSize, offsetSize);
				for(uint32_t i = 0; i < mv.ulCount; i += sizeof(uint32_t))
				{
					uint32_t offset = utils::slice(offsetBytes, static_cast<size_t>(i), i + sizeof(uint32_t), sizeof(uint32_t), utils::toT_l<uint32_t>);
					mv.rgulDataOffsets.push_back(offset);
				}
				mv.rgulDataOffsets.push_back(utils::cast<uint32_t>(data.size()));

				/// Contains ulCount + 1 offsets because I add data.size() as the last offset
				ASSERT(std::cmp_equal(mv.ulCount + 1, mv.rgulDataOffsets.size()), "[ERROR] Invalid size");
				ASSERT(std::cmp_greater(data.size(), 4ull + offsetSize), "[ERROR] Invalid size");

				size_t dataSize = data.size() - (4ull + offsetSize);
				mv.rgDataItems = utils::slice(
					data, 
					4ull + offsetSize,
					data.size(),
					dataSize
				);
				ASSERT(std::cmp_equal(mv.ulCount, mv.rgDataItems.size()), "[ERROR] Invalid size");
				return mv;
			}
		};

		struct PTBinary 
		{
			uint32_t id{};
			std::vector<types::byte_t> data{};
		};

		struct PTString
		{
			uint32_t id{};
			std::string data{};
		};

		class Property
		{
		public:
			uint32_t id{};
			types::PropertyType propType{ types::PropertyType::Null };
			utils::PTInfo info{};
			std::vector<types::byte_t> data{};

		public:
			PTBinary asPTBinary() const
			{
				ASSERT(!info.isMv, "[ERROR] Property is not a PTBinary");
				ASSERT(!info.isFixed, "[ERROR] Property is not a PTBinary");
				ASSERT(std::cmp_equal(info.singleEntrySize, 0ull), "[ERROR] Property is not a PTBinary");
				PTBinary bin{};
				bin.id = id;
				bin.data = data;
				return bin;
			}

			PTString asPTString() const
			{

				ASSERT(!info.isMv, "[ERROR] Property is not a PTString");
				ASSERT(!info.isFixed, "[ERROR] Property is not a PTString");
				ASSERT(std::cmp_equal(info.singleEntrySize, 2ull), "[ERROR] Property is not a PTString");
				ASSERT(std::cmp_equal(data.size() % 2ull, 0ull), "[ERROR] Property is not a PTString");
				ASSERT(std::cmp_greater(data.size(), 0ull), "[ERROR] Property is not a PTString");

				std::vector<uint8_t> characters{};
				for(uint32_t i = 0; i < data.size() / 2ull; ++i)
				{
					uint32_t start = utils::cast<uint32_t>(i * info.singleEntrySize);
					uint32_t end = utils::cast<uint32_t>(start + info.singleEntrySize);
					// TODO: For now this just takes the most significant byte and converts it to utf8
					types::utf16le_t character = utils::slice(data, start, end, end - start, utils::toT_l<types::utf16le_t>);
					characters.push_back(utils::cast<uint8_t>(character & 0xFF));
				}
				PTString str{};
				str.id = id;
				str.data = std::string(characters.begin(), characters.end());
				return str;
			}

			template<typename PropType>
			PropType as() const
			{
				if constexpr (std::is_same_v<PropType, PTBinary>)
				{
					return asPTBinary();
				}
				else if constexpr (std::is_same_v<PropType, PTString>)
				{
					return asPTString();
				}
				else
				{
					ASSERT(false, "[ERROR] Invalid Property Type");
					return PropType{};
				}
			}	
		};

		class PropertyContext
		{
		public:
			using PropertyID_t = uint32_t;
		public:
			//PropertyContext() = default;

			static PropertyContext Init(core::NID nid, core::Ref<const ndb::NDB> ndb)
			{
				static_assert(std::is_move_constructible_v<PropertyContext>, "PropertyContext must be move constructible");
				static_assert(std::is_move_assignable_v<PropertyContext>, "PropertyContext must be move assignable");
				static_assert(std::is_copy_constructible_v<PropertyContext>, "PropertyContext must be copy constructible");
				static_assert(std::is_copy_assignable_v<PropertyContext>, "PropertyContext must be copy assignable");
				return PropertyContext(nid, ndb, HN::Init(nid, ndb));
			}

			auto begin()
			{
				return m_properties.begin();
			}

			auto end()
			{
				return m_properties.end();
			}

			template<typename ConvertTo = Property, typename PropIDType>
			const ConvertTo& at(PropIDType propID) const
			{
				if (std::is_same_v<ConvertTo, Property>)
					return m_properties.at(_convert(propID));
				else
					return m_properties.at(_convert(propID)).as<ConvertTo>();
			}

			template<typename PropIDType>
			bool exists(PropIDType propID) const
			{
				return m_properties.count(_convert(propID)) > 0;
			}

			template<typename PropIDType>
			bool is(PropIDType propID, types::PropertyType propType) const
			{
				return m_properties.at(_convert(propID)).propType == propType;
			}

			template<typename PropIDType>
			bool valid(PropIDType propID, types::PropertyType propType) const
			{
				return exists(propID) && is(propID, propType);
			}

			void readProperties()
			{
				for (const auto& record : m_records)
				{
					
					utils::PTInfo info = utils::PropertyTypeInfo(record.wPropType);

					Property prop{};
					prop.id = record.wPropId;
					prop.propType = utils::PropertyType(record.wPropType);
					prop.info = info;

					if (info.isFixed && info.singleEntrySize <= 4) // Data Value
					{
						prop.data = record.dwValueHnid;
					}
															// check if HID 
					else if ( (info.isFixed && info.singleEntrySize > 4) || (record.dwValueHnid[0] & 0x1F) == 0) // HID
					{
						HID id = HID(record.dwValueHnid);
						prop.data = m_hn.alloc(id);
					}
					else // NID
					{
						ASSERT(false, "[ERROR] NID not implemented yet");
					}

					ASSERT((m_properties.count(prop.id) == 0), "[ERROR] Duplicate property found");
					m_properties[prop.id] = prop;
				}
				ASSERT((m_properties.size() == m_records.size()), "[ERROR] Invalid size");
			}
	
		private:

			PropertyContext(core::NID nid, core::Ref<const ndb::NDB> ndb, HN&& hn)
				: m_nid(nid), m_ndb(ndb), m_hn(hn), m_bth(core::Ref<const HN>(m_hn), m_hn.getHeader().hidUserRoot)
			{
				_init();
			}

			void _init()
			{
				ASSERT((m_hn.getBType() == types::BType::PC), "[ERROR] Invalid BType");
				ASSERT((m_bth.keySize() == 2), "[ERROR]");
				ASSERT((m_bth.dataSize() == 6), "[ERROR]");
				for(const auto& record : m_bth.records())
				{
					m_records.push_back(record.as<PCBTHRecord>());
				}
				readProperties();
			}

			template<typename PropIDType>
			PropertyID_t _convert(PropIDType id) const
			{
				if constexpr (std::is_same_v<PropIDType, PropertyID_t>)
					return id;

				else if constexpr (std::is_same_v<PropIDType, types::PidTagType>)
					return static_cast<PropertyID_t>(id);

				else
					ASSERT(false, "[ERROR] Invalid PropIDType");
			}

		private:
			std::vector<PCBTHRecord> m_records{};
			std::map<PropertyID_t, Property> m_properties{};
			core::NID m_nid;
			core::Ref<const ndb::NDB> m_ndb;
			HN m_hn;
			BTreeHeap m_bth; /// m_bth has to be initialized after m_hn
			
		};

		class RowBlock
		{

		public:
			RowBlock(const std::vector<types::byte_t>& blockBytes, const TCInfo& header, uint64_t rowsPerBlock)
			{
				size_t singleRowSize = header.rgib.at(TCInfo::TCI_bm);
				for (size_t i = 0; i < blockBytes.size() / singleRowSize; ++i)
				{
					size_t start = i * singleRowSize;
					size_t end = start + singleRowSize;
					m_rows.emplace_back(
						utils::slice(blockBytes, start, end, singleRowSize),
						header
					);
				}
				ASSERT((m_rows.size() == rowsPerBlock), "[ERROR] Invalid number of rows");
			}

			const RowData& at(size_t rowIdx) const
			{
				return m_rows.at(rowIdx);
			}

		private:
			std::vector<RowData> m_rows{};
		};

		class TableContext
		{
		public:

			static TableContext Init(core::NID nid, core::Ref<const ndb::NDB> ndb)
			{
				static_assert(std::is_move_constructible_v<TableContext>, "TableContext must be move constructible");
				static_assert(std::is_move_assignable_v<TableContext>, "TableContext must be move assignable");
				static_assert(std::is_copy_constructible_v<TableContext>, "TableContext must be copy constructible");
				static_assert(std::is_copy_assignable_v<TableContext>, "TableContext must be copy assignable");
				ndb::NBTEntry nbt = ndb->get(nid);
				return TableContext(nid, ndb, HN::Init(nid, ndb), nbt);
			}

			std::vector<TCRowID> rowIDs() const
			{
				return m_rowIDs;
			}

			bool hasCol(types::PidTagType propID, types::PropertyType propType) const
			{
				const auto propID_ = static_cast<uint32_t>(propID);
				const auto propType_ = static_cast<uint32_t>(propType);
				for(const auto& col : m_header.rgTCOLDESC)
				{
					const uint32_t colTag = col.tag;
					const uint32_t colPropId = col.propId();
					const uint32_t colPropType = col.propType();
					if (colPropId == propID_)
						if(colPropType == propType_)
							return true;
				}
				return false;
			}

			TColDesc findCol(types::PidTagType propID) const
			{
                const auto propID_ = static_cast<uint32_t>(propID);
                for (const auto &col : m_header.rgTCOLDESC) {
                        const uint32_t colTag = col.tag;
                        const uint32_t colPropId = col.propId();
                        const uint32_t colPropType = col.propType();
                        if (colPropId == propID_)
								return col;
                }
                ASSERT(false, "Unable to find column");
                return TColDesc{};
			}

			[[nodiscard]] const RowData& at(TCRowID rowID) const
			{
				const size_t blockIdx = rowID.dwRowIndex / m_rowsPerBlock;
				const size_t rowIdx = rowID.dwRowIndex % m_rowsPerBlock;
				return m_rowBlocks.at(blockIdx).at(rowIdx);
			}

			Property at(TCRowID rowID, size_t colIdx) const
			{
				const RowData& rowData = at(rowID);
				const TCInfo tcInfo = rowData.tcInfo;
				const TColDesc& colDesc = tcInfo.rgTCOLDESC.at(colIdx);

				ASSERT((rowData.dwRowID == rowID.dwRowID), "[ERROR]");

				if (rowData.exists(colDesc.iBit))
				{
					// Property Type is stored in the lower 16 bits of tag
					types::PropertyType propType = utils::PropertyType(colDesc.propType());
					utils::PTInfo ptInfo = utils::PropertyTypeInfo(propType);

					Property prop{};
					// The upper 16 bits of tag is the PropID
					prop.id = colDesc.propId();
					prop.propType = propType;
					prop.info = ptInfo;

					if (ptInfo.isMv || !ptInfo.isFixed || ptInfo.singleEntrySize > 8ull) // Data stored in Heap(HID) or subnode(NID)
					{
						ASSERT((HNID::size() == static_cast<size_t>(colDesc.cbData)), "[ERROR]");
						// An HNID is a 4 byte value so therefore should be within TCI_4b range.
						ASSERT((colDesc.ibData < tcInfo.rgib.at(TCInfo::TCI_4b)), "[ERROR]");
						HNID hnid(
							utils::slice(
								rowData.data,
								static_cast<uint32_t>(colDesc.ibData),
								colDesc.ibData + static_cast<uint32_t>(colDesc.cbData),
								static_cast<uint32_t>(colDesc.cbData)
							)
						);

						if (hnid.isHID()) // HNID is an HID which means the data is stored in the HN
						{
							prop.data = m_hn.alloc(hnid.as<HID>());
						}

						else // HNID is an NID which means that the data is stored in a subnode
						{
							ASSERT(false, "[ERROR] Not Implemented");
						}
					}

					else if (!ptInfo.isMv && ptInfo.isFixed && ptInfo.singleEntrySize <= 8ull) // Data stored in RowData
					{
						// An HNID is a 4 byte value so therefore should be within TCI_4b range.
						ASSERT((colDesc.ibData < tcInfo.rgib.at(TCInfo::TCI_4b)), "[ERROR]");
						prop.data = utils::slice(
											rowData.data,
											colDesc.ibData,
											static_cast<uint16_t>(colDesc.cbData),
											static_cast<uint16_t>(colDesc.cbData)
										);
					}

					else
					{
						ASSERT(false, "[ERROR] Invalid Column");
					}
					return prop;
				}
				//ASSERT(false, "[ERROR] Not handling non-existent columns");
				return Property{};
			}

			static TCInfo readTCInfo(const std::vector<types::byte_t>& bytes)
			{
				TCInfo info{};
				info.bType = utils::toBType(utils::slice(bytes, 0, 1, 1, utils::toT_l<uint8_t>));
				info.cCols = utils::slice(bytes, 1, 2, 1, utils::toT_l<uint8_t>);
				std::vector<types::byte_t> rgibBytes = utils::slice(bytes, 2, 10, 8);
				size_t singlergibSize = 2ull;
				for (size_t i = 0; i < 4ull; i++)
				{
					size_t start = i * singlergibSize;
					size_t end = start + singlergibSize;
					info.rgib.push_back(utils::slice(rgibBytes, start, end, singlergibSize, utils::toT_l<uint16_t>));
				}
				info.hidRowIndex = HID(utils::slice(bytes, 10, 14, 4));
				info.hnidRows = HNID(utils::slice(bytes, 14, 18, 4)); 
				/// hidIndex is deprecated. Should be set to 0
				info.hidIndex = utils::slice(bytes, 18, 22, 4, utils::toT_l<uint32_t>);
				info.rgTCOLDESC = readTColDesc(utils::slice(bytes, 22ull, bytes.size(), bytes.size() - 22ull));

				ASSERT((info.bType == types::BType::TC), "[ERROR] Invalid BType");
				ASSERT((info.cCols == info.rgTCOLDESC.size()), "[ERROR] Invalid size");
				return info;
			}

			static std::vector<TColDesc> readTColDesc(const std::vector<types::byte_t>& bytes)
			{
				size_t singleTColDescSize = 8;
				ASSERT((bytes.size() % singleTColDescSize == 0), "[ERROR] Invalid size");
				std::vector<TColDesc> cols{};

				for (size_t i = 0; i < bytes.size() / singleTColDescSize; ++i)
				{
					size_t start = i * singleTColDescSize;
					size_t end = start + singleTColDescSize;
					std::vector<types::byte_t> colBytes = utils::slice(bytes, start, end, singleTColDescSize);
					TColDesc col{};
					col.tag = utils::slice(colBytes, 0, 4, 4, utils::toT_l<uint32_t>);
					col.ibData = utils::slice(colBytes, 4, 6, 2, utils::toT_l<uint16_t>);
					col.cbData = utils::slice(colBytes, 6, 7, 1, utils::toT_l<uint8_t>);
					col.iBit = utils::slice(colBytes, 7, 8, 1, utils::toT_l<uint8_t>);
					cols.push_back(col);
				}
				/// The entries in this array MUST be sorted by the tag field of TCOLDESC. In ascending order.
				std::sort(cols.begin(), cols.end(), 
					[](const TColDesc& lhs, const TColDesc& rhs) { return lhs.tag < rhs.tag; });
				return cols;
			}

		private:
			TableContext(
				core::NID nid, core::Ref<const ndb::NDB> ndb, HN&& hn, const ndb::NBTEntry& nbt)
				: m_nid(nid), m_ndb(ndb), m_hn(hn)
			{
				ASSERT((m_hn.getBType() == types::BType::TC), "[ERROR]");

				m_header = readTCInfo(m_hn.alloc(m_hn.getHeader().hidUserRoot));
				BTreeHeap bth(m_hn, m_header.hidRowIndex);
				ndb::SubNodeBTree subNodeBTree = m_ndb->InitSubNodeBTree(nbt);
				ASSERT((bth.keySize() == 4), "[ERROR]");
				ASSERT((bth.dataSize() == 4), "[ERROR]");

				if (!bth.empty())
				{
					if (m_header.hnidRows.isHID() && nbt.bidSub == 0) // Row Matrix is in the HN
					{
						auto rowBlockBytes = m_hn.alloc(m_header.hnidRows.as<HID>());
						m_rowsPerBlock = rowBlockBytes.size() / m_header.rgib.at(TCInfo::TCI_bm);
						/// This check only works for HID allocated Row Matrices
						ASSERT((m_rowsPerBlock == bth.nrecords()), "[ERROR] Invalid number of rows per block");

						m_rowBlocks.emplace_back(rowBlockBytes, m_header, m_rowsPerBlock);
					}

					else // Row Matrix must be in a subnode id'ed by a NID
					{
						const auto [rowBlockBytes, found] = subNodeBTree.at(m_header.hnidRows.as<core::NID>());
						ASSERT(found, "[ERROR] Failed to find");
						m_rowsPerBlock = rowBlockBytes.size() / m_header.rgib.at(TCInfo::TCI_bm);
						ASSERT((m_rowsPerBlock == bth.nrecords()), "[ERROR] Invalid number of rows per block");
						m_rowBlocks.emplace_back(rowBlockBytes, m_header, m_rowsPerBlock);
						/// Each block except the last one MUST have a size of 8192 bytes.
						///ASSERT(false, "[ERROR] Not implemented yet");
					}

					for (const auto& record : bth.records())
					{
						m_rowIDs.push_back(record.as<TCRowID>());
					}
				}
				// Testing only
				//const RowData& rowdata = at(m_rowIDs.at(0));
				//Property prop = at(m_rowIDs.at(0), 0);
				//at(m_rowIDs.at(1), 1);
				//at(m_rowIDs.at(1), 2);
				//at(m_rowIDs.at(1), 3);
				//at(m_rowIDs.at(1), 4);
				//at(m_rowIDs.at(1), 5);
			}


		private:
			core::NID m_nid;
			core::Ref<const ndb::NDB> m_ndb;
			uint64_t m_rowsPerBlock{0};
			std::vector<RowBlock> m_rowBlocks;
			std::vector<TCRowID> m_rowIDs;
			TCInfo m_header;
			HN m_hn;
		};

		class LTP
		{
		public:
			LTP(core::Ref<const ndb::NDB> ndb) : m_ndb(ndb) {}
		private:
			core::Ref<const ndb::NDB> m_ndb;
		};
	}
}







#endif // READER_LTP_H