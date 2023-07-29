#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <fstream>
#include <utility>
#include <algorithm>
#include <unordered_map>

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
			static constexpr size_t SizeNBytes = 4;
		public:
			HID() = default;
			explicit HID(const std::vector<types::byte_t>& data)
				: m_hid(utils::slice(data, 0, 4, 4, utils::toT_l<uint32_t>))
			{
				STORYT_ASSERT((utils::getNIDType(getHIDType()) == types::NIDType::HID),
					"Invalid HID Type");
			}
			/// @brief (5 bits) MUST be set to 0 (NID_TYPE_HID) to indicate a valid HID.
			[[nodiscard]] uint32_t getHIDType() const
			{ 
				return m_hid & 0x1FU;
			}
			/// @brief (11 bits) This is the 1-based index value that identifies 
			/// an item allocated from the heap node. This value MUST NOT be zero.
			[[nodiscard]] uint32_t getHIDAllocIndex() const
			{ 
				const uint32_t index = (m_hid >> 5U) & 0x7FFU;
				STORYT_ASSERT((index != 0), "Invalid HID Index");
				return index;
			}
			/// @brief (16 bits) This is the zero-based data block index. 
			/// This number indicates the zerobased index of the data block in which this heap item resides.
			[[nodiscard]] uint32_t getHIDBlockIndex() const
			{ 
				return (m_hid >> 16U) & 0xFFFFU;
			}

			[[nodiscard]] uint32_t getHIDRaw() const
			{
				return m_hid;
			}
			static constexpr size_t id() { return 10; }

		private:
			uint32_t m_hid;
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
			explicit HNID(const std::vector<types::byte_t>& data)
				: m_data(data)
			{
				STORYT_ASSERT((m_data.size() == HNID::sizeNBytes), "m_data.size() [{}] == HNID::sizeNBytes [4]", m_data.size());
			}

			[[nodiscard]] bool isHID() const
			{
				return (m_data[0] & 0x1FU) == 0;
			}

			template<typename IDType>
			[[nodiscard]] IDType as() const
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
				else
				{
					STORYT_ASSERT(false, "Invalid IDType");
					return IDType{};
				}
			}
			constexpr static size_t sizeNBytes = 4;
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
			uint16_t ibHnpm{};
			/// (1 byte): Block signature; MUST be set to 0xEC to indicate an HN. 
			uint8_t bSig{};
			/// (1 byte): Client signature.
			uint8_t bClientSig{};
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
			uint16_t ibHnpm{};
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
			std::vector<types::byte_t> rgbFillLevel;
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
			uint16_t cAlloc{};
			/// (2 bytes): Free count. This represents the number of freed items in the HN.
			uint16_t cFree{};
			/// (variable): Allocation table. This contains cAlloc + 1 entries. 
			/// Each entry is a WORD value that is the byte offset to the beginning of the allocation. 
			/// An extra entry exists at the cAlloc + 1st position to mark the offset of the next available slot. 
			/// Therefore, the nth allocation starts at offset rgibAlloc[n] (from the beginning of the HN header), 
			/// and its size is calculated as rgibAlloc[n + 1] – rgibAlloc[n] bytes. 
			/// The size of each entry in the PST is 2 bytes.
			std::vector<uint16_t> rgibAlloc{};
		};

		/**
		 * @brief = The BTHHEADER contains the BTH metadata, which instructs the reader how 
		 * to access the other objects of the BTH structure.
		*/
		struct BTHHeader
		{
			/// (1 byte): MUST be bTypeBTH. 
			uint8_t bType{};
			/// (1 byte): Size of the BTree Key value, in bytes. This value MUST be set to 2, 4, 8, or 16.
			uint8_t cbKey{};
			/// (1 byte): Size of the data value, in bytes. This MUST be greater than zero and less than or equal to 32.
			uint8_t cbEnt{};
			/// (1 byte): Index depth. This number indicates how many levels of intermediate indices 
			/// exist in the BTH. Note that this number is zero-based, meaning that a value of zero 
			/// actually means that the BTH has one level of indices. If this value is greater than zero, 
			/// then its value indicates how many intermediate index levels are present. 
			uint8_t bIdxLevels{};
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
			uint64_t key{};
			/// (4 bytes): HID of the next level index record array. This contains the HID of the heap 
			/// item that contains the next level index record array.
			HID hidNextLevel{};

			IntermediateBTHRecord(const std::vector<types::byte_t>& bytes, size_t keySize, size_t dataSize)
			{
				STORYT_ASSERT((keySize <= sizeof(uint64_t)), "keySize is not <= sizeof(uint64_t)");
				STORYT_ASSERT((dataSize == HID::SizeNBytes), "dataSize != HID::SizeNBytes");
				STORYT_ASSERT((bytes.size() == keySize + dataSize), "bytes.size() != keySize + dataSize");
				utils::ByteView view(bytes);
				key = view.read<uint64_t>(keySize);
				hidNextLevel = view.entry<HID>(dataSize);
			}

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
			uint32_t ulSize{};
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
			[[nodiscard]] uint32_t getPID() const noexcept { return (tag & 0xFFFF0000U) >> 16U; }
			[[nodiscard]] uint32_t getPType() const noexcept { return tag & 0xFFFFU; }
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
				STORYT_ASSERT((keySize + 4 == bytes.size()), "keySize + 4 != bytes.size()");
				return IntermediateBTHRecord(bytes, keySize, HID::SizeNBytes);
			}

			static LeafBTHRecord asLeafBTHRecord(
				const std::vector<types::byte_t>& bytes,
				size_t keySize,
				size_t dataSize
			)
			{
				STORYT_ASSERT((bytes.size() == keySize + dataSize), "bytes.size() != keySize + dataSize");
				utils::ByteView view(bytes);
				LeafBTHRecord record{};
				record.key = view.read(keySize); // utils::slice(bytes, 0ull, keySize, keySize);
				record.data = view.read(dataSize);// utils::slice(bytes, keySize, keySize + dataSize, dataSize);
				return record;
			}

			static PCBTHRecord asPCBTHRecord(
				const std::vector<types::byte_t>& bytes,
				size_t keySize,
				size_t dataSize
			)
			{
				STORYT_ASSERT((bytes.size() == 8), "bytes.size() != 8");
				STORYT_ASSERT((keySize == 2), "keySize != 2");
				STORYT_ASSERT((dataSize == 6), "dataSize != 6");
				utils::ByteView view(bytes);
				PCBTHRecord record{};
				record.wPropId = view.read<uint16_t>(2);
				record.wPropType = view.read<uint16_t>(2);
				record.dwValueHnid = view.read(4); 

				STORYT_ASSERT((utils::isIn(record.wPropType, utils::PROPERTY_TYPE_VALUES)), "Invalid property type");
				return record;
			}

			static TCRowID asTCRowID(
				const std::vector<types::byte_t>& bytes,
				size_t keySize,
				size_t dataSize
			)
			{
				STORYT_ASSERT((bytes.size() == 8), "bytes.size() != 8");
				STORYT_ASSERT((keySize == 4), "keySize != 4");
				STORYT_ASSERT((dataSize == 4), "dataSize != 4");
				utils::ByteView view(bytes);
				TCRowID record{};
				record.dwRowID = view.read<uint32_t>(4);
				record.dwRowIndex = view.read<uint32_t>(4);
				return record;
			}

			template<typename RecordType>
			[[nodiscard]] RecordType as() const
			{
				if constexpr (std::is_same_v<RecordType, LeafBTHRecord>)
				{
					return asLeafBTHRecord(m_data, m_keySize, m_dataSize);
				}
				else if constexpr (std::is_same_v<RecordType, IntermediateBTHRecord>)
				{
					return asIntermediateBTHRecord(m_data, m_keySize);
				}
				else if constexpr (std::is_same_v<RecordType, PCBTHRecord>)
				{
					return asPCBTHRecord(m_data, m_keySize, m_dataSize);
				}
				else if constexpr (std::is_same_v<RecordType, TCRowID>)
				{
					return asTCRowID(m_data, m_keySize, m_dataSize);
				}
				else
				{
					STORYT_ASSERT(false, "Invalid Record Type");
				}	
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
			static HN Init(core::NID nid, core::Ref<const ndb::NDB> ndb)
			{
				const auto nbt = ndb->get(nid);
				const auto bbt = ndb->get(nbt.value().bidData);
				return HN(nid, ndb->InitDataTree(bbt.value().bref, bbt.value().cb));
			}

			static HN Init(core::NID nid, ndb::DataTree&& dtree)
			{
				return HN(nid, std::move(dtree));
			}

			bool addBlock(const std::vector<types::byte_t>& data, size_t blockIdx)
			{
				STORYT_ASSERT((m_blocks.size() == blockIdx), "m_blocks.size() != blockIdx");
				if(blockIdx == 8 || blockIdx % 8 + 128 == 0)
				{
					const HNBitMapHDR bmheader = readHNBitMapHDR(data);
					const HNPageMap map = readHNPageMap(data, bmheader.ibHnpm);
					HNBlock block{};
					block.bmheader = bmheader;
					block.map = map;
					block.data = data;
					m_blocks.push_back(block);
				}
				else
				{
					const HNPageHDR pheader = readHNPageHDR(data);
					const HNPageMap map = readHNPageMap(data, pheader.ibHnpm);
					HNBlock block{};
					block.pheader = pheader;
					block.map = map;
					block.data = data;
					m_blocks.push_back(block);
				}
				return true;
			}

			[[nodiscard]] types::BType getBType() const { return utils::toBType(m_hnhdr.bClientSig); }
			[[nodiscard]] HNHDR getHeader() const { return m_hnhdr; }
			[[nodiscard]] const HNBlock& at(size_t blockIdx) const { return m_blocks.at(blockIdx); }
			[[nodiscard]] std::vector<types::byte_t> getAllocation(const HID& hid) const
			{
				const size_t blockIdx = hid.getHIDBlockIndex();
				const size_t pageIdx = hid.getHIDAllocIndex();
				const HNBlock& block = at(blockIdx);
				const size_t start = static_cast<size_t>(block.map.rgibAlloc.at(pageIdx - 1));
				const size_t end = static_cast<size_t>(block.map.rgibAlloc.at(pageIdx));
				const size_t size = end - start;
				return utils::slice(block.data, start, end, size);
			}

			[[nodiscard]] size_t nblocks() const { return m_blocks.size(); }

			static HNHDR readHNHDR(const std::vector<types::byte_t>& bytes, size_t dataBlockIdx, size_t nDataBlocks)
			{
				STORYT_ASSERT((dataBlockIdx == 0), "Only the first data block contains a HNHDR");
				utils::ByteView view(bytes);
				HNHDR hnhdr{};
				hnhdr.ibHnpm = view.read<uint16_t>(2);
				hnhdr.bSig = view.read<uint8_t>(1); 
				hnhdr.bClientSig = view.read<uint8_t>(1); 
				hnhdr.hidUserRoot = view.entry<HID>(4); 
				hnhdr.rgbFillLevel = view.split(4); //utils::toBits(utils::slice(bytes, 8, 12, 4, utils::toT_l<uint32_t>), 4);

				const uint32_t hidType = hnhdr.hidUserRoot.getHIDType();
				STORYT_ASSERT((hidType == 0), "Invalid HID Type [{}]", hidType);
				STORYT_ASSERT((hnhdr.bSig == 0xECU), "Invalid HN signature");
				STORYT_ASSERT(utils::isIn(hnhdr.bClientSig, utils::BTYPE_VALUES), "Invalid BType");
				STORYT_ASSERT((hnhdr.rgbFillLevel.size() == 8U), "Invalid Fill Level size must be 8");

				if (nDataBlocks < 8)
				{
					for (size_t i = nDataBlocks; i < 8; i++)
					{
						STORYT_ASSERT((static_cast<uint32_t>(hnhdr.rgbFillLevel.at(i)) == static_cast<uint32_t>(types::FillLevel::LEVEL_EMPTY)),
							"Fill level must be empty for block at idx [{}]", i);
					}
				}
				return hnhdr;
			}

			static HNPageMap readHNPageMap(const std::vector<types::byte_t>& bytes, size_t start)
			{
				utils::ByteView view(bytes, start);
				HNPageMap pg{};
				pg.cAlloc = view.read<uint16_t>(2);
				pg.cFree = view.read<uint16_t>(2); 
				pg.rgibAlloc = view.read<uint16_t>(pg.cAlloc + 1, 2);
				STORYT_ASSERT((pg.rgibAlloc.size() == pg.cAlloc + 1), "Should be cAlloc + 1 entries");

				for (size_t i = 1; i < pg.rgibAlloc.size(); i++)
				{
					STORYT_ASSERT((pg.rgibAlloc[i] > pg.rgibAlloc[i - 1]), "pg.rgibAlloc[i] is not > pg.rgibAlloc[i - 1]");
				}
				const uint16_t lastAlloc = pg.rgibAlloc.at(pg.rgibAlloc.size() - 1);

				// The beginning of the HNPAGEMAP is aligned on a 2-byte
				// boundary so there can be an additional 1 byte of padding between 
				// the last heap item and the HNPAGEMAP.
				STORYT_ERRORIF((lastAlloc != start && lastAlloc + 1 != start),
					"The last entry is rgibAlloc in the HNPageMap != the start of the HNPageMap. Last Alloc: [{}] Start: [{}]", lastAlloc, start);
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
				utils::ByteView view(bytes);
				HNPageHDR hdr{};
				hdr.ibHnpm = view.read<uint16_t>(2);
				return hdr;
			}

			static HNBitMapHDR readHNBitMapHDR(const std::vector<types::byte_t>& bytes)
			{
				utils::ByteView view(bytes);
				HNBitMapHDR hdr{};
				hdr.ibHnpm = view.read<uint16_t>(2);
				hdr.rgbFillLevel = view.split(64);
				STORYT_ASSERT((hdr.rgbFillLevel.size() == 128), "hdr.rgbFillLevel.size() != 128");
				return hdr;
			}

			private:
				HN(core::NID nid, ndb::DataTree&& dTree)
					: m_nid(nid), m_dataTree(std::move(dTree))
				{
					static_assert(std::is_move_constructible_v<HN>, "HN must be move constructible");
					static_assert(std::is_move_assignable_v<HN>, "HN must be move assignable");
					static_assert(std::is_copy_constructible_v<HN>, "HN must be copy constructible");
					static_assert(std::is_copy_assignable_v<HN>, "HN must be copy assignable");

					size_t idx = 0;	
					for (ndb::DataBlock& dataBlock : m_dataTree)
					{
						if (idx == 0)
						{
							m_hnhdr = readHNHDR(dataBlock.data, 0, m_dataTree.nDataBlocks());
							HNBlock block{};
							block.map = readHNPageMap(dataBlock.data, m_hnhdr.ibHnpm);
							block.data = std::move(dataBlock.data);
							m_blocks.push_back(block);
						}
						else
						{
							addBlock(dataBlock.data, idx);
						}
						++idx;
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
			BTreeHeap(const HN& hn, HID bthHeaderHID)
			{ 
				static_assert(std::is_move_constructible_v<BTreeHeap>, "BTreeHeap must be move constructible");
				static_assert(std::is_move_assignable_v<BTreeHeap>, "BTreeHeap must be move assignable");
				static_assert(std::is_copy_constructible_v<BTreeHeap>, "BTreeHeap must be copy constructible");
				static_assert(std::is_copy_assignable_v<BTreeHeap>, "BTreeHeap must be copy assignable");
				const std::vector<types::byte_t> headerBytes = hn.getAllocation(bthHeaderHID);
				m_header = readBTHHeader(headerBytes);

				if (m_header.hidRoot.getHIDRaw() > 0) // If hidRoot is zero, the BTH is empty
				{
					m_records = readBTHRecords(
						hn,
						hn.getAllocation(m_header.hidRoot),
						m_header.cbKey,
						m_header.bIdxLevels,
						m_header.cbEnt
					);
				}
			}

			static BTHHeader readBTHHeader(const std::vector<types::byte_t>& bytes)
			{
				STORYT_ASSERT((bytes.size() == 8), "readBTHHeader");
				utils::ByteView view{bytes};
				BTHHeader header{};
				header.bType = view.read<uint8_t>(1); 
				header.cbKey = view.read<uint8_t>(1); 
				header.cbEnt = view.read<uint8_t>(1);  
				header.bIdxLevels = view.read<uint8_t>(1);
				/// hidRoot can be equal to zero if the BTH is empty
				header.hidRoot = view.entry<HID>(4); 

				STORYT_ASSERT((utils::toBType(header.bType) == types::BType::BTH), "utils::toBType(header.bType) != types::BType::BTH");
				STORYT_ASSERT((utils::isIn(header.cbKey, { 2, 4, 8, 16 })), "header.cbKey is NOT in {2, 4, 8, 16}");
				STORYT_ASSERT((header.cbEnt > 0 && header.cbEnt <= 32), "readBTHHeader");

				if (header.hidRoot.getHIDRaw() > 0)
				{
					STORYT_ASSERT((header.hidRoot.getHIDAllocIndex() > 0), "Invalid Allocation Index");
				}
				return header;
			}

			std::vector<Record> readBTHRecords(
				const HN& hn,
				const std::vector<types::byte_t>& bytes,
				size_t keySize,
				size_t bIdxLevels,
				size_t dataSize = 0)
			{
				const size_t rsize = keySize + dataSize;
				const size_t nRecords = bytes.size() / rsize;
				STORYT_ASSERT((bytes.size() % rsize == 0), "nRecords must be a mutiple of keySize + dataSize");

				if (bIdxLevels > 0)
				{
					STORYT_ASSERT((dataSize == HID::SizeNBytes), "readBTHRecords");
					utils::ByteView view(bytes);
					std::vector<IntermediateBTHRecord> inters = view.entries<IntermediateBTHRecord>(nRecords, rsize, keySize, dataSize);
					std::vector<types::byte_t> totalBytes{};
					for (const auto& inter : inters)
					{
						std::vector<types::byte_t> alloc = hn.getAllocation(inter.hidNextLevel);
						totalBytes.insert(totalBytes.end(), alloc.begin(), alloc.end());
					}
					return readBTHRecords(hn, totalBytes, keySize, bIdxLevels - 1, dataSize);
				}

				STORYT_ASSERT((bIdxLevels == 0), "bIdxLevels != 0 [{}]", bIdxLevels);
				utils::ByteView view(bytes);
				std::vector<Record> records = view.entries<Record>(nRecords, rsize, keySize, dataSize);
				STORYT_ASSERT((records.size() == nRecords), "Invalid size");
				return records;
			}

			[[nodiscard]] bool empty() const
			{
				return m_header.hidRoot.getHIDRaw() == 0;
			}

			[[nodiscard]] HID getHIDRoot() const
			{
				return m_header.hidRoot;
			}

			[[nodiscard]] size_t getKeySize() const
			{
				return m_header.cbKey;
			}
			[[nodiscard]] size_t getDataSize() const
			{
				return m_header.cbEnt;
			}

			[[nodiscard]] size_t nrecords() const
			{
				return m_records.size();
			}

			[[nodiscard]] const std::vector<Record>& records() const
			{
				return m_records;
			}
		private:
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
				
				const size_t offsetSize = mv.ulCount * sizeof(uint32_t);
				std::vector<types::byte_t> offsetBytes = utils::slice(data, 4ull, 4ull + offsetSize, offsetSize);
				for(uint32_t i = 0; i < mv.ulCount; i += sizeof(uint32_t))
				{
					uint32_t offset = utils::slice(offsetBytes, static_cast<size_t>(i), i + sizeof(uint32_t), sizeof(uint32_t), utils::toT_l<uint32_t>);
					mv.rgulDataOffsets.push_back(offset);
				}
				mv.rgulDataOffsets.push_back(utils::cast<uint32_t>(data.size()));

				/// Contains ulCount + 1 offsets because I add data.size() as the last offset
				STORYT_ASSERT(std::cmp_equal(mv.ulCount + 1, mv.rgulDataOffsets.size()), "Invalid size");
				STORYT_ASSERT(std::cmp_greater(data.size(), 4ULL + offsetSize), "Invalid size");

				size_t dataSize = data.size() - (4ULL + offsetSize);
				mv.rgDataItems = utils::slice(
					data, 
					4ull + offsetSize,
					data.size(),
					dataSize
				);
				STORYT_ASSERT(std::cmp_equal(mv.ulCount, mv.rgDataItems.size()), "Invalid size");
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
			bool isLoaded{ false };

		public:
			[[nodiscard]] PTBinary asPTBinary() const
			{
				STORYT_ASSERT(!info.isMv, "Property is not a PTBinary");
				STORYT_ASSERT(!info.isFixed, "Property is not a PTBinary");
				STORYT_ASSERT(std::cmp_equal(info.singleEntrySize, 0ULL), "Property is not a PTBinary");
				PTBinary bin{};
				bin.id = id;
				bin.data = data;
				return bin;
			}

			[[nodiscard]] PTString asPTString() const
			{
				STORYT_ASSERT(!info.isMv, "Property is not a PTString");
				STORYT_ASSERT(!info.isFixed, "Property is not a PTString");
				STORYT_ASSERT((info.singleEntrySize == 2ULL), "Property is not a PTString");
				STORYT_ASSERT((data.size() % 2ULL == 0ULL), "Property is not a PTString");
				STORYT_ASSERT((data.size() != 0ULL), "Property is not a PTString");
				PTString str{};
				str.id = id;
				str.data = utils::UTF16BytesToString(data);
				return str;
			}

			[[nodiscard]] int32_t asPTInt32() const
			{
				utils::ByteView view(data);
				return view.read<int32_t>(4U);
			}

			template<typename PropType>
			[[nodiscard]] PropType as() const
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
					STORYT_ASSERT(false, "Invalid Property Type");
					return PropType{};
				}
			}

			[[nodiscard]] bool DataIsInHNID() const
			{
				return info.isFixed && info.singleEntrySize <= 4;
			}

			[[nodiscard]] bool DataIsInHeap() const
			{
				STORYT_ASSERT((data.size() == 4), "Data should be an HNID");
				return (info.isFixed && info.singleEntrySize > 4U) || (data[0] & 0x1FU) == 0U;
			}

			[[nodiscard]] bool DataIsInSubNodeTree() const
			{
				return !DataIsInHNID() && !DataIsInHeap();
			}
		};

		class PropertyContext
		{
		public:
			using PropertyID_t = uint32_t;
		public:
			static PropertyContext Init(
				core::NID nid,
				ndb::DataTree* datatree,
				const ndb::SubNodeBTree* const subTree
			)
			{
				STORYT_ASSERT((datatree != nullptr), "Cannot create a PropertyContext with a nullptr DataTree");
				if (subTree != nullptr)
				{
					return PropertyContext(nid, HN::Init(nid, std::move(*datatree)), *subTree);
				}
				return PropertyContext(nid, HN::Init(nid, std::move(*datatree)));
			}
			static PropertyContext Init(
				core::NID nid, 
				core::Ref<const ndb::NDB> ndb, 
				const ndb::SubNodeBTree& subTree
			)
			{
				return PropertyContext(nid, HN::Init(nid, ndb), subTree);
			}

			static PropertyContext Init(
				core::NID nid,
				core::Ref<const ndb::NDB> ndb
			)
			{
				const auto nbt = ndb->get(nid);
				STORYT_ERRORIF(!nbt.has_value(), "Failed to find NBTEntry for Property Context with NID [{}]", nid.getNIDRaw());
				return PropertyContext(nid, HN::Init(nid, ndb), ndb->InitSubNodeBTree(nbt->bidSub));
			}
			[[nodiscard]] Property* TryToGetProperty(uint32_t pid, types::PropertyType propType)
			{
				_loadProperty(pid); // attempt to load property
				if (HasPropertyWPidAndPtypeOf(pid, propType))
				{
					return &m_properties.at(pid);
				}
				return nullptr;
			}
			[[nodiscard]] Property* TryToGetProperty(types::PidTagTypeCombo::Info info)
			{
				return TryToGetProperty(info.pid, info.type);
			}
			[[nodiscard]] Property* TryToGetProperty(types::PidTagType pid, types::PropertyType propType)
			{
				return TryToGetProperty(static_cast<uint32_t>(pid), propType);
			}
			[[nodiscard]] bool HasPropertyWPidOf(uint32_t pid) const
			{
				return m_properties.contains(pid);
			}
			[[nodiscard]] bool HasPropertyWPidOf(types::PidTagType pid) const
			{
				return HasPropertyWPidOf(static_cast<uint32_t>(pid));
			}
			[[nodiscard]] bool HasPropertyWPidAndPtypeOf(uint32_t pid, types::PropertyType propType) const
			{
				if (m_properties.contains(pid))
				{
					const Property& prop = m_properties.at(pid);
					if (prop.propType == propType)
					{
						return true;
					}
				}
				return false;
			}
			[[nodiscard]] bool HasPropertyWPidAndPtypeOf(types::PidTagType pid, types::PropertyType propType) const
			{
				return HasPropertyWPidAndPtypeOf(static_cast<uint32_t>(pid), propType);
			}
			[[nodiscard]] bool HasPropertyWPidAndPtypeOf(types::PidTagTypeCombo::Info info) const
			{
				return HasPropertyWPidAndPtypeOf(info.pid, info.type);
			}
	
		private:
			explicit PropertyContext(core::NID nid, HN&& hn)
				: m_nid(nid), m_hn(std::move(hn)), m_bth(m_hn, m_hn.getHeader().hidUserRoot) 
			{
				_init();
			}

			explicit PropertyContext(
				core::NID nid, 
				HN&& hn, 
				const ndb::SubNodeBTree& subTree
			)
				: 
				m_nid(nid), 
				m_hn(hn), 
				m_bth(m_hn, m_hn.getHeader().hidUserRoot),
				// The SubNodeTree can afford to be copied 
				// because at this point nothing has been read from the file.
				// But it can't be std::move because other TCs or PCs may depend on it.
				m_subtree(subTree)
			{
				_init();
			}
			void _init()
			{
				VerifyTableContextIsValid_();
				_loadMetaProps();
			}
			void VerifyTableContextIsValid_() const
			{
				STORYT_ASSERT((m_hn.getBType() == types::BType::PC), "m_hn.getBType() != types::BType::PC");
				STORYT_ASSERT((m_bth.getKeySize() == 2), "m_bth.getKeySize() != 2");
				STORYT_ASSERT((m_bth.getDataSize() == 6), "m_bth.getDataSize() != 6");
			}
			void _loadProperty(uint32_t propID)
			{
				Property& prop = m_properties.at(propID);
				if (prop.isLoaded || prop.DataIsInHNID()) // Data Value or Data for prop has already been loaded
				{
					return;
				}
				else if (prop.DataIsInHeap()) // HID
				{
					const HID id = HID(prop.data);
					prop.data = m_hn.getAllocation(id);
				}
				else if (prop.DataIsInSubNodeTree()) // NID
				{
					if (m_subtree.has_value())
					{
						const core::NID nid(prop.data);
						ndb::DataTree* dataPtr = m_subtree->getDataTree(nid);
						STORYT_ASSERT((dataPtr != nullptr), "Failed to find DataTree for Property [{}]", prop.id);
						prop.data = dataPtr->combineDataBlocks();
					}
					else
					{
						STORYT_ERROR("Attempted to get a DataTree from an unintialized SubNodeTree");
					}	
				}
				else
				{
					STORYT_ASSERT(false, "Invalid Property");
				}
				prop.isLoaded = true;
			}

			void _loadMetaProps()
			{
				for (const auto& bthRecord : m_bth.records())
				{
					const auto record = bthRecord.as<PCBTHRecord>();
					const utils::PTInfo info = utils::PropertyTypeInfo(record.wPropType);
					Property prop{};
					prop.id = record.wPropId;
					prop.propType = utils::PropertyType(record.wPropType);
					prop.info = info;
					prop.data = record.dwValueHnid;

					STORYT_ASSERT((!m_properties.contains(prop.id)), "m_properties.contains(prop.id)");
					m_properties[prop.id] = prop;
				}
				STORYT_ASSERT((m_properties.size() == m_bth.nrecords()), "m_properties.size() != m_bth.nrecords()");
			}

		private:
			std::unordered_map<PropertyID_t, Property> m_properties{};
			core::NID m_nid;
			std::optional<ndb::SubNodeBTree> m_subtree;
			HN m_hn;
			BTreeHeap m_bth; /// m_bth has to be initialized after m_hn	
		};

		struct RowEntry
		{
			std::vector<types::byte_t> data;
			types::PropertyType propType;
			uint32_t propID;
			bool isLoaded{ false };
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
		class SingleRow
		{
		public:

			explicit SingleRow(const std::vector<types::byte_t>& rowBytes, const TCInfo& header)
				: m_tcInfo(header)
			{
				const size_t offset4b = header.rgib.at(TCInfo::TCI_4b);
				const size_t offset2b = header.rgib.at(TCInfo::TCI_2b);
				const size_t offset1b = header.rgib.at(TCInfo::TCI_1b);
				const size_t totalRowSize = header.rgib.at(TCInfo::TCI_bm);

				STORYT_ASSERT((offset4b <= offset2b), "offset4b is not <= offset2b");
				STORYT_ASSERT((offset2b <= offset1b), "offset2b is not <= offset1b");
				STORYT_ASSERT((offset1b < totalRowSize), "offset1b is not < totalRowSize");

				utils::ByteView view(rowBytes);
				m_dwRowID = view.read<uint32_t>(4);
				// Read everything from the start of Row Data to the start of rgbCEB
				m_data = view.setStart(0).read(offset1b);
				m_rgbCEB = view.read(totalRowSize - offset1b);
				_setupRowEntries();
				STORYT_ASSERT((m_rgbCEB.size() == static_cast<size_t>(std::ceil(static_cast<double>(header.cCols) / 8.0))), "RowData Constructor");
			}
			[[nodiscard]] size_t getRowID() const
			{
				return m_dwRowID;
			}
			[[nodiscard]] size_t nColumns() const
			{
				return m_tcInfo.cCols;
			}
			[[nodiscard]] bool isColumnPresent(uint8_t iBit) const
			{
				return (m_rgbCEB[iBit / 8U] & (1U << (7U - (iBit % 8U))));
			}
			[[nodiscard]] bool hasRowEntry(uint32_t pid) const
			{
				return  m_rowEntries.contains(pid);
			}
			[[nodiscard]] bool hasRowEntry(types::PidTagType pid) const
			{
				return hasRowEntry(static_cast<uint32_t>(pid));
			}
			[[nodiscard]] RowEntry* TryToGetRowEntry(uint32_t pid)
			{
				if (m_rowEntries.contains(pid))
				{
					RowEntry* entry = &m_rowEntries.at(pid);
					if (entry->isLoaded)
					{
						return entry;
					}
					else
					{
						STORYT_ASSERT(false, "Trying to get a Row Entry that is not loaded PID [{}]", pid);
					}
				}
				STORYT_ASSERT(false, "Failed to find RowEntry with PID [{}]", pid);
				return nullptr;
			}
			[[nodiscard]] RowEntry* TryToGetRowEntry(types::PidTagType& pid)
			{
				return TryToGetRowEntry(static_cast<uint32_t>(pid));
			}

			[[nodiscard]] RowEntry* loadRowEntry(const HN& hn, std::optional<ndb::SubNodeBTree>& subtree, const TColDesc& colInfo)
			{
				if (m_rowEntries.contains(colInfo.getPID()))
				{
					RowEntry* entry = &m_rowEntries.at(colInfo.getPID());
					if (entry->isLoaded) // If the entry has already been loaded then just return it.
					{
						return entry;
					}
					else if (isColumnPresent(colInfo.iBit))
					{
						utils::ByteView view(m_data);
						entry->propID = colInfo.getPID();
						entry->propType = utils::PropertyType(colInfo.getPType());
						const utils::PTInfo ptInfo = utils::PropertyTypeInfo(colInfo.getPType());
						// Read data where the column starts (ibData) to where it ends (cbData)
						const std::vector<types::byte_t> data = view.setStart(colInfo.ibData).read(colInfo.cbData);
						if (DataIsStoredInline(ptInfo)) // Data is stored inline
						{
							entry->data = data;
						}
						else if (DataIsStoredInHN(data)) // Data is stored in HN and Indexed using HID
						{
							entry->data = hn.getAllocation(HID(data));
						}
						else if (DataIsStoredInSubNodeTree(ptInfo, data) && subtree.has_value())
						{
							const core::NID nid = core::NID(data);
							ndb::DataTree* datatree = subtree->getDataTree(nid);
							if (datatree != nullptr)
							{
								entry->data = datatree->combineDataBlocks();
							}
							else
							{
								STORYT_ASSERT(false,
									"Failed to find data tree in subnode tree using NID [{}] for SingleRow", nid.getNIDRaw());
							}
						}
						entry->isLoaded = true;
						return entry;
					}
				}
				STORYT_ASSERT(false, "Failed to find RowEntry with Column PID [{}]", colInfo.getPID());
				return nullptr;
			}

			[[nodiscard]] SingleRow& loadEntireRow(const HN& hn, std::optional<ndb::SubNodeBTree>& subtree)
			{
				for (const auto& col : m_tcInfo.rgTCOLDESC)
				{
					loadRowEntry(hn, subtree, col);
				}
				return *this;
			}
			[[nodiscard]] bool DataIsStoredInline(const utils::PTInfo& ptInfo) const
			{
				return ptInfo.isFixed && ptInfo.singleEntrySize <= 8ULL;
			}

			[[nodiscard]] bool DataIsStoredInHN(const std::vector<types::byte_t>& data) const
			{
				return (data[0] & 0x1FU) == 0U;
			}

			[[nodiscard]] bool DataIsStoredInSubNodeTree(const utils::PTInfo& ptInfo, const std::vector<types::byte_t>& data) const
			{
				return !DataIsStoredInline(ptInfo) && !DataIsStoredInHN(data);
			}

		private:
			void _setupRowEntries()
			{
				// There will be a maximum number of RowEntries for a SingleRow as there
				// are columns in the Row Matrix. So resize the m_rowEntries vector to match
				// and fill it with empty RowEntries
				m_rowEntries.reserve(nColumns());
				for (const auto& col : m_tcInfo.rgTCOLDESC)
				{
					if (!m_rowEntries.contains(col.getPID()))
					{
						m_rowEntries[col.getPID()] = RowEntry{};
					}
					else
					{
						STORYT_ASSERT(false, "Found duplicate PID when constructing RowEntries for SingleRow");
					}
				}
			}
		private:
			TCInfo m_tcInfo{};
			/// (4 bytes): The 32-bit value that corresponds to the dwRowID 
			/// value in this row's corresponding TCROWID record. 
			/// Note that this value corresponds to the PidTagLtpRowId property.
			uint32_t m_dwRowID{};
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
			std::vector<types::byte_t> m_rgbCEB{};
			/// Everything from the start of Row Data to the start of rgbCEB
			/// Can contain the actual data is the data is less than 8 bytes
			/// otherwise the data is in the single row.
			std::vector<types::byte_t> m_data{};
			/// RowEntries will either be loaded or need to be loaded.
			/// If loaded they will contain all the byte information for that particular
			/// row entry. If not loaded they will need be loaded using an HN and SubNodeTree.
			/// The key is the PIDTag for the column.
			std::unordered_map<uint32_t, RowEntry> m_rowEntries{};
		};

		class RowBlock
		{

		public:
			RowBlock(const std::vector<types::byte_t>& blockBytes, const TCInfo& header, size_t rowsPerBlock)
			{
				const size_t singleRowSize = header.rgib.at(TCInfo::TCI_bm);
				utils::ByteView view(blockBytes);
				m_rows = view.entries<SingleRow>(blockBytes.size() / singleRowSize, singleRowSize, header);
				STORYT_ASSERT((m_rows.size() == rowsPerBlock), "m_rows.size() != rowsPerBlock");
			}
			[[nodiscard]] SingleRow& getSingleRow(size_t rowIdx)
			{
				return m_rows.at(rowIdx);
			}

		private:
			std::vector<SingleRow> m_rows{};
		};

		class TableContext
		{
		public:

			static std::optional<TableContext> Init(
				core::NID dataTreeNID,
				ndb::SubNodeBTree& parentSubNodeTree
			)
			{
				/*
				* The DataTree for this TC should ALWAYS be in the parentSubNodeTree.
				* 
				* Whereas the SubNodeBTree for the TC, if present, will be a Nested SubNodeBTree inside the Parent SubNodeBtree.
				* The id for the Nested SubNodeBTree is the same id for the DataTree.
				*/
				ndb::DataTree* dataTree = parentSubNodeTree.getDataTree(dataTreeNID);
				ndb::SubNodeBTree* nestedSubNodeTree = parentSubNodeTree.getNestedSubNodeTree(dataTreeNID);
				if (dataTree == nullptr) // A TableContext can NOT be setup without a Data Tree
				{
					// Only log missed DataTree NIDs that are NOT the attachment and attachment table nids
					// because these NIDs are not required to have a DataTree
					STORYT_WARNIF((dataTreeNID != core::NID(0x671) && dataTreeNID != core::NID(0x8025)),
						"Data Tree was NOT found for NID [{}]", dataTreeNID.getNIDRaw());
					return std::nullopt;
				}
				else if (nestedSubNodeTree == nullptr)
				{
					STORYT_WARN("Nested SubNodeBTree was not found");
					return TableContext(HN::Init(dataTreeNID, std::move(*dataTree)), std::nullopt);
				}
				return TableContext(HN::Init(dataTreeNID, std::move(*dataTree)), std::move(*nestedSubNodeTree));
			}

			static TableContext Init(core::NID nid, core::Ref<const ndb::NDB> ndb)
			{			
				const auto nbt = ndb->get(nid);
				STORYT_ASSERT(nbt.has_value(), "Failed to Init TableContext with NID of [{}]", nid.getNIDRaw());
				return TableContext(HN::Init(nid, ndb), ndb->InitSubNodeBTree(nbt->bidSub));
			}
			void loadRowMatrix()
			{
				if (!m_bth.empty() && !m_rowMatrixIsLoaded)
				{
					if (m_header.hnidRows.isHID()) // Row Matrix is in the HN
					{
						_loadRowMatrixFromHN();
					}

					else // Row Matrix must be in a subnode id'ed by a NID
					{
						_loadRowMatrixFromSubNodeTree();
					}
					STORYT_WARNIF((m_rowIDs.size() < m_rowsPerBlock),
						"The number of row IDs [{}] is less than the number of rows per block [{}]",
						m_rowIDs.size(), m_rowsPerBlock);
				}
				m_rowMatrixIsLoaded = true;
			}
			[[nodiscard]] size_t getSizeOfSingleRow() const
			{
				return m_header.rgib.at(TCInfo::TCI_bm);
			}

			[[nodiscard]] size_t nRows() const
			{
				return m_rowIDs.size();
			}

			[[nodiscard]] const std::vector<TCRowID>& getRowIDs() const
			{
				return m_rowIDs;
			}

			[[nodiscard]] const std::vector<TColDesc>& getColumns() const
			{
				return m_header.rgTCOLDESC;
			}

			[[nodiscard]] bool hasColumn(types::PidTagType pid, types::PropertyType ptype) const
			{
				const auto upid = static_cast<uint32_t>(pid);
				const auto uptype = static_cast<uint32_t>(ptype);
				auto match = [upid, uptype](const TColDesc& desc) {
					return desc.getPID() == upid && desc.getPType() == uptype;
				};
				return std::ranges::any_of(m_header.rgTCOLDESC.cbegin(), m_header.rgTCOLDESC.cend(), match);
			}

			[[nodiscard]] TColDesc getColumn(types::PidTagType propID) const
			{
				const auto propID_ = static_cast<uint32_t>(propID);
				for (const auto& col : m_header.rgTCOLDESC)
				{
					const uint32_t colPropId = col.getPID();
					if (colPropId == propID_)
					{
						return col;
					}
				}
				STORYT_ASSERT(false, "Failed to find column");
				return TColDesc{};
			}
			[[nodiscard]] RowEntry* getSingleRowAndLoadColumn(TCRowID rowID, types::PidTagType pid)
			{
				loadRowMatrix();
				return getSingleRowRaw(rowID).loadRowEntry(m_hn, m_subtree, getColumn(pid));
			}

			[[nodiscard]] SingleRow& getSingleRowAndLoadEntireRow(TCRowID rowID)
			{
				loadRowMatrix(); 
				return getSingleRowRaw(rowID).loadEntireRow(m_hn, m_subtree);
			}

			[[nodiscard]] SingleRow& getSingleRowRaw(TCRowID rowID) // A Raw Single Row will be returned
			{
				const size_t blockIdx = rowID.dwRowIndex / m_rowsPerBlock;
				const size_t rowIdx = rowID.dwRowIndex % m_rowsPerBlock;
				return m_rowBlocks.at(blockIdx).getSingleRow(rowIdx);
			}

			static TCInfo readTCInfo(const std::vector<types::byte_t>& bytes)
			{
				utils::ByteView view(bytes);
				TCInfo info{};
				info.bType = utils::toBType(view.read<uint8_t>(1));
				info.cCols = view.read<uint8_t>(1); 
				info.rgib = view.read<uint16_t>(4, 2);
				info.hidRowIndex = view.entry<HID>(4);
				info.hnidRows = view.entry<HNID>(4);
				/// hidIndex is deprecated. Should be set to 0
				info.hidIndex = view.read<uint32_t>(4);
				info.rgTCOLDESC = readTColDesc(view.read(bytes.size() - 22ULL));

				STORYT_ASSERT((info.bType == types::BType::TC), "Invalid BType");
				STORYT_ASSERT((info.cCols == info.rgTCOLDESC.size()), "info.cCols != info.rgTCOLDESC.size()");
				STORYT_ASSERT((info.hidIndex == 0), "info.hidIndex != 0");
				for (size_t i = 1; i < info.rgib.size(); ++i)
				{
					STORYT_ASSERT((info.rgib.at(i) >= info.rgib.at(i - 1)), "info.rgib.at(i) is not >= info.rgib.at(i - 1)");
				}
				return info;
			}

			static std::vector<TColDesc> readTColDesc(const std::vector<types::byte_t>& bytes)
			{
				const size_t singleTColDescSize = 8;
				STORYT_ASSERT((bytes.size() % singleTColDescSize == 0), "bytes.size() mod singleTColDescSize != 0");
				std::vector<TColDesc> cols;

				for (size_t i = 0; i < bytes.size() / singleTColDescSize; ++i)
				{
					const size_t start = i * singleTColDescSize;
					const size_t end = start + singleTColDescSize;
					const std::vector<types::byte_t> colBytes = utils::slice(bytes, start, end, singleTColDescSize);
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
			explicit TableContext(
				HN&& hn, 
				std::optional<ndb::SubNodeBTree> subtree
			)
				: 
				m_hn(std::move(hn)), 
				m_header(readTCInfo(m_hn.getAllocation(m_hn.getHeader().hidUserRoot))), 
				m_bth(m_hn, m_header.hidRowIndex), 
				m_subtree(std::move(subtree))
			{
				VerifyTableContextIsValid_();
				_loadRowIndexFromBTH();
			}
			void VerifyTableContextIsValid_() const
			{
				STORYT_ASSERT((m_hn.getBType() == types::BType::TC), "m_hn.getBType() != types::BType::TC");
				STORYT_ASSERT((m_bth.getKeySize() == 4), "m_bth.getKeySize() != 4");
				STORYT_ASSERT((m_bth.getDataSize() == 4), "m_bth.getDataSize() != 4");
			}
			void _loadRowIndexFromBTH()
			{
				if (!m_bth.empty())
				{
					for (const auto& record : m_bth.records())
					{
						m_rowIDs.push_back(record.as<TCRowID>());
					}
				}
				else
				{
					STORYT_WARN("Trying to load RowIndex but the BTH is empty");
				}
			}

			void _loadRowMatrixFromHN()
			{
				auto rowBlockBytes = m_hn.getAllocation(m_header.hnidRows.as<HID>());
				m_rowsPerBlock = rowBlockBytes.size() / getSizeOfSingleRow();
				/// This check only works for HID allocated Row Matrices
				STORYT_ASSERT((m_rowsPerBlock == m_bth.nrecords()), "Invalid number of rows per block");
				m_rowBlocks.emplace_back(rowBlockBytes, m_header, m_rowsPerBlock);
			}

			void _loadRowMatrixFromSubNodeTree() 
			{
				STORYT_ASSERT((m_subtree.has_value()), "!m_subtree.has_value()");
				if (m_subtree.has_value())
				{
					ndb::DataTree* datatree = m_subtree->getDataTree(m_header.hnidRows.as<core::NID>());
					m_rowsPerBlock = datatree->sizeOfDataBlockData(0) / getSizeOfSingleRow();
					for (size_t i = 0; i < datatree->nDataBlocks(); i++)
					{
						if (i < datatree->nDataBlocks() - 1)
						{
							/// Each block except the last one MUST have a size of 8192 bytes.
							/// TODO its possible this check will fail because of variable padding
							STORYT_ASSERT((datatree->sizeOfDataBlockData(i) + 16 == 8192), "datatree->sizeOfDataBlockData(i) + 16 != 8192");
						}

						/// last block will potentially have less rows because its allowed
						/// to have fewer bytes
						if (i == datatree->nDataBlocks() - 1)
						{
							m_rowBlocks.emplace_back(
								datatree->at(i).data,
								m_header,
								datatree->sizeOfDataBlockData(i) / m_header.rgib.at(TCInfo::TCI_bm)
							);
							continue;
						}
						m_rowBlocks.emplace_back(datatree->at(i).data, m_header, m_rowsPerBlock);
					}
				}
				else
				{
					STORYT_ASSERT(false, "Failed to Setup RowMatrix for TableContext because SubTree was not initialized");
				}
			}

		private:
			bool m_rowMatrixIsLoaded{false};
			std::optional<ndb::SubNodeBTree> m_subtree;
			uint64_t m_rowsPerBlock{0};
			std::vector<RowBlock> m_rowBlocks;
			std::vector<TCRowID> m_rowIDs;
			HN m_hn;
			TCInfo m_header; // TCInfo has to come after HN			
			BTreeHeap m_bth; // BTH has to come after HN and m_header
		};

		class LTP
		{
		public:
			explicit LTP(core::Ref<const ndb::NDB> ndb) : m_ndb(ndb) {}
		private:
			core::Ref<const ndb::NDB> m_ndb;
		};
	}
}







#endif // READER_LTP_H