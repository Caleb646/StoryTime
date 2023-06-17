#include <iostream>
#include <stdarg.h>
#include <cassert>
#include <vector>
#include <string>
#include <fstream>

#include "types.h"
#include "utils.h"
#include "core.h"
#include "ndb.h"

#ifndef READER_LTP_H
#define READER_LTP_H

namespace reader {
	namespace ltp {
		/**
		 * @brief = An HID is a 4-byte value that identifies an item allocated from the heap. 
		 * The value is unique only within the heap itself. The following is the structure of an HID. 
		*/
		struct HID
		{
			/// (5 bits) MUST be set to 0 (NID_TYPE_HID) to indicate a valid HID.
			int16_t hidType{};
			/// (11 bits) This is the 1-based index value that identifies an item allocated from the heap node. This value MUST NOT be zero.
			int32_t hidIndex{};
			/// (16 bits) This is the zero-based data block index. This number indicates the zerobased index of the data block in which this heap item resides.
			int32_t hidBlockIndex{};
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
			int64_t hidUserRoot{};
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
			int16_t ibHnpm{};
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
			std::vector<types::byte_t> rgibAlloc{};
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
			int64_t hidRoot{};
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
			int64_t key{};
			/// (4 bytes): HID of the next level index record array. This contains the HID of the heap 
			/// item that contains the next level index record array.
			int64_t hidNextLevel{};
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
			int64_t key{};
			/// (variable): This is the key of the record. The size of the key is 
			/// specified in the cbKey field in the corresponding BTHHEADER structure(section 2.3.2.1). 
			/// The size and contents of the key are specific to the higher level structure that implements this BTH.
			int64_t data{};
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
		struct PropertyContext
		{

		};

		/**
		 * @brief = An HNID is a 32-bit hybrid value that represents either an HID or an NID. 
		 * The determination is made by examining the hidType (or equivalently, nidType) value. 
		 * The HNID refers to an HID if the hidType is NID_TYPE_HID. Otherwise, the HNID refers to an NID. 
		 * An HNID that refers to an HID indicates that the item is stored in the data block. An HNID that 
		 * refers to an NID indicates that the item is stored in the subnode block, and the NID is the local 
		 * NID under the subnode where the raw data is located. 
		*/
		struct HNID
		{
			/// The HNID refers to an HID if the hidType is NID_TYPE_HID. Otherwise, the HNID refers to an NID. 
			core::NID nid{};
			/// The HNID refers to an HID if the hidType is NID_TYPE_HID. Otherwise, the HNID refers to an NID. 
			HID hid{};
		};

		/**
		 * @brief 
		*/
		struct PCBTHRecord
		{
			/// (2 bytes): Property ID, as specified in [MS-OXCDATA] section 2.9. This is the upper 16 bits of the 
			/// property tag value. This is a manifestation of the BTH record (section 2.3.2.3) 
			/// and constitutes the key of this record. 
			int32_t wPropId{};

			/// (2 bytes): Property ID, as specified in [MS-OXCDATA] section 2.9. This is the 
			/// upper 16 bits of the property tag value. This is a manifestation of the 
			/// BTH record (section 2.3.2.3) and constitutes the key of this record.
			int32_t wPropType{};

			/// (4 bytes): Depending on the data size of the property type indicated 
			/// by wPropType and a few other factors, this field represents different values. 
			/// The following table explains the value contained in dwValueHnid based on the 
			/// different scenarios. In the event where the dwValueHnid value contains an 
			/// HID or NID (section 2.3.3.2), the actual data is stored in the corresponding 
			/// heap or subnode entry, respectively. 
			int64_t dwValueHnid{};

			/*
			* Variable size?	Fixed data size		NID_TYPE(dwValueHnid) == NID_TYPE_HID?	 dwValueHnid 
				N					<= 4 bytes				-								 	Data Value 
									> 4 bytes				Y								 	HID 
				Y					-						Y								 	HID (<= 3580 bytes) 
															N								 	NID (subnode, > 3580 bytes) 
			*/
		};

		/**
		 * @brief When the MV property contains variable-size elements, 
		 * such as PtypBinary, PtypString, or PtypString8), the data layout 
		 * is more complex. The following is the data format of a multi-valued 
		 * property with variable-size base type. 
		*/
		struct VariableSizeMVProperty
		{
			/// (4 bytes): Number of data items in the array. 
			int64_t ulCount{};
			/// (variable): An array of ULONG values that represent offsets to 
			/// the start of each data item for the MV array. Offsets are relative 
			/// to the beginning of the MV property data record. The length of the 
			/// Nth data item is calculated as: rgulDataOffsets[N+1] – rgulDataOffsets[N], 
			/// with the exception of the last item, in which the total size of the 
			/// MV property data record is used instead of rgulDataOffsets[N+1].
			std::vector<types::byte_t> rgulDataOffsets{};
			/// (variable): A byte-aligned array of data items. Individual items are delineated 
			/// using the rgulDataOffsets values. 
			std::vector<types::byte_t> rgDataItems{};
		};

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
		 * @brief TCINFO is the header structure for the TC. The TCINFO is 
		 * accessed using the hidUserRoot field in the HNHDR structure of 
		 * the containing HN. The header contains the column definitions and other relevant data.
		*/
		struct TCInfo
		{
			/// (1 byte): TC signature; MUST be set to bTypeTC.
			int16_t bType{};
			/// (1 byte): Column count. This specifies the number of columns in the TC.
			int16_t cCols{};
			/// (8 bytes): This is an array of 4 16-bit values that specify the 
			/// offsets of various groups of data in the actual row data. 
			/// The application of this array is specified in section 2.3.4.4, 
			/// which covers the data layout of the Row Matrix. The following 
			/// table lists the meaning of each value: 
			int64_t rgib{};

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
			int64_t hidRowIndex{};
			/// (4 bytes): HNID to the Row Matrix (that is, actual table data). 
			/// This value is set to zero if the TC contains no rows. 
			int64_t hnidRows{};
			///  (4 bytes): Deprecated. Implementations SHOULD ignore this value, 
			/// and creators of a new PST MUST set this value to zero. 
			int64_t hidIndex{};
			/// (variable): Array of Column Descriptors. This array contains cCols 
			/// entries of type TCOLDESC structures that define each TC column. 
			/// The entries in this array MUST be sorted by the tag field of TCOLDESC. 
			std::vector<types::byte_t> rgTCOLDESC{};
		};

		/**
		 * @brief The TCOLDESC structure describes a single column in the TC, 
		 * which includes metadata about the size of the data associated with this column, 
		 * as well as whether a column exists, and how to locate the column data from the Row Matrix.
		*/
		struct TCOLDESC
		{
			/// (4 bytes): This field specifies that 32-bit tag 
			/// that is associated with the column. 
			int64_t tag{};
			/// (2 bytes): Data Offset. This field indicates the offset 
			/// from the beginning of the row data (in the Row Matrix) 
			/// where the data for this column can be retrieved. Because 
			/// each data row is laid out the same way in the Row Matrix, 
			/// the Column data for each row can be found at the same offset.
			int32_t ibData{};
			/// (1 byte): Data size. This field specifies the size of the data 
			/// associated with this column (that is, "width" of the column), 
			/// in bytes per row. However, in the case of variable-sized data, 
			/// this value is set to the size of an HNID instead.
			int32_t cbData{};
			/// (1 byte): Cell Existence Bitmap Index. This value is the 0-based 
			/// index into the CEB bit that corresponds to this Column. A detailed 
			/// explanation of the mapping mechanism will be discussed in section 2.3.4.4.1. 
			int16_t iBit{};
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
			int64_t dwRowID{};
			/// (Unicode: 4 bytes): The 0-based index to the corresponding row 
			/// in the Row Matrix. Note that for ANSI PSTs, the maximum number of rows is 2^16.
			int64_t dwRowIndex{};
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
		struct RowData
		{
			/// (4 bytes): The 32-bit value that corresponds to the dwRowID 
			/// value in this row's corresponding TCROWID record. 
			/// Note that this value corresponds to the PidTagLtpRowId property.
			int64_t dwRowID{};
			/// (variable): 4-byte-aligned Column data. This region contains 
			/// data with a size that is a multiple of 4 bytes. The types of 
			/// data stored in this region are 4-byte and 8-byte values.
			std::vector<types::byte_t> rgdwData{};
			/// (variable): 2-byte-aligned Column data. This region contains 
			/// data that are 2 bytes in size. 
			std::vector<types::byte_t> rgwData{};
			/// (variable): Byte-aligned Column data. This region 
			/// contains data that are byte-sized. 
			std::vector<types::byte_t> rgbData{};
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
			enum class ConfigType
			{
				SINGLE_BLOCK,
				XBLOCK,
				XXBLOCK
			};
		};

		class LTP
		{
		public:
			LTP(ndb::NDB& ndb) : m_ndb(ndb)
			{
				_init();
			}
		public:
			void _init()
			{

				auto nbtentry = m_ndb.getNID(core::NID_MESSAGE_STORE);
				auto bbtentry = m_ndb.getBID(nbtentry.bidData);
				//LOG("[INFO] %i", utils::ms::ComputeSig(bbtentry.bref.ib, bbtentry.bref.bid.getBidRaw()));
				ndb::DataTree dataTree = m_ndb.readDataTree(bbtentry.bref, bbtentry.cb);
				_readHN(dataTree);
			}

			void _readHN(ndb::DataTree& tree)
			{
				ASSERT(tree.isValid(), "[ERROR] Invalid Data Tree");

				HNHDR hnHdr = LTP::readHNHDR(tree.dataBlocks.at(0).data, 0, tree.dataBlocks.size());
				for (size_t i = 1; i < tree.dataBlocks.size(); i++)
				{
					const ndb::DataBlock& block = tree.dataBlocks.at(i);
				}
			}

			static HNHDR readHNHDR(const std::vector<types::byte_t>& bytes, size_t dataBlockIdx, int64_t nDataBlocks)
			{
				LOG("[INFO] Data Block Size: %i", bytes.size());
				ASSERT((dataBlockIdx == 0), "[ERROR] Only the first data block contains a HNHDR");
				HNHDR hnhdr{};
				hnhdr.ibHnpm = utils::slice(bytes, 0, 2, 2, utils::toT_l<decltype(hnhdr.ibHnpm)>);
				hnhdr.bSig = utils::slice(bytes, 2, 3, 1, utils::toT_l<decltype(hnhdr.bSig)>);
				hnhdr.bClientSig = utils::slice(bytes, 3, 4, 1, utils::toT_l<decltype(hnhdr.bClientSig)>);
				hnhdr.hidUserRoot = utils::slice(bytes, 4, 8, 4, utils::toT_l<decltype(hnhdr.hidUserRoot)>);
				hnhdr.rgbFillLevel = utils::toBits(utils::slice(bytes, 8, 12, 4, utils::toT_l<int32_t>), 4);

				uint32_t hidType = hnhdr.hidUserRoot & 0x1F;
				ASSERT((hidType == 0), "[ERROR] Invalid HID Type %i", hidType)
				ASSERT((hnhdr.bSig == static_cast<decltype(hnhdr.bSig)>(0xEC) ), "[ERROR] Invalid HN signature");
				ASSERT( (utils::isIn(hnhdr.bClientSig, utils::BTYPE_VALUES) ), "[ERROR] Invalid BType");
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

			HNPageMap _readHNPageMap(const std::vector<types::byte_t>& bytes, int64_t startPosOfHNPageMap)
			{
				HNPageMap pg{};
				pg.cAlloc = utils::slice(bytes, startPosOfHNPageMap, startPosOfHNPageMap + (int64_t)2, (int64_t)2, utils::toT_l<decltype(pg.cAlloc)>);
			}

		private:
			ndb::NDB& m_ndb;
		};
	}
}







#endif // READER_LTP_H