#include <iostream>
#include <stdarg.h>
#include <cassert>
#include <vector>
#include <array>
#include <string>
#include <fstream>
#include <utility>
#include <map>

#include "types.h"
#include "utils.h"
#include "core.h"
#include "ndb.h"
#include "ltp.h"

#ifndef READER_MESSAGING_H
#define READER_MESSAGING_H

namespace reader
{
	namespace msg
	{

	
		/**
		 * @brief The Folder object is a composite entity that is represented using four LTP constructs. Each Folder 
		 * object consists of one PC, which contains the properties directly associated with the Folder object, 
		 * and three TCs for information about the contents, hierarchy and other associated information of 
		 * the Folder object. Some Folder objects MAY have additional nodes that pertain to Search, 
		 * which is discussed in section. 
		 * 
		 * @Note For identification purposes, the nidIndex portion for each of the NIDs is the same to indicate
		 * that these nodes collectively make up a Folder object. However, each of the 4 NIDs has a different
		 * nidType value to differentiate their respective function.
		*/
		class Folder
		{
		public:
			Folder(core::NID nid, ndb::NDB& ndb)
				: m_nid(nid), m_ndb(ndb)
			{
				std::map<types::NIDType, ndb::NBTEntry> nbtentries = m_ndb.all(m_nid);
				ASSERT((nbtentries.size() == 4), "[ERROR] A Folder must be composed of 4 Parts");
				ASSERT((nbtentries.count(types::NIDType::NORMAL_FOLDER) == 1), "[ERROR] A Folder must have a NORMAL_FOLDER");
				ASSERT((nbtentries.count(types::NIDType::HIERARCHY_TABLE) == 1), "[ERROR] A Folder must have a HIERARCHY_TABLE");
				ASSERT((nbtentries.count(types::NIDType::CONTENTS_TABLE) == 1), "[ERROR] A Folder must have a CONTENTS_TABLE");
				ASSERT((nbtentries.count(types::NIDType::ASSOC_CONTENTS_TABLE) == 1), "[ERROR] A Folder must have a ASSOC_CONTENTS_TABLE");

				ltp::TableContext hier(nbtentries.at(types::NIDType::HIERARCHY_TABLE).nid, m_ndb);
			}


		private:
			core::NID m_nid;
			ndb::NDB& m_ndb;
		};

		struct EntryID
		{
			/// (4 bytes): Flags; each of these bytes MUST be initialized to zero.
			uint32_t rgbFlags{};
			/// (16 bytes): The provider UID of this PST, which is the value of the PidTagRecordKey property
			/// in the message store.If this property does not exist, the PST client MAY generate a new unique
			///	ID, or reject the PST as invalid.
			std::vector<types::byte_t> uuid{};
			/// (4 bytes): This is the corresponding NID of the underlying node that represents the object.
			core::NID nid{};

			EntryID(const std::vector <types::byte_t >& data, const std::vector<types::byte_t>& pidTagRecordKey)
			{
				ASSERT((data.size() == 24ull), "EntryID data size is not 24 bytes");
				rgbFlags = utils::slice(data, 0, 4, 4, utils::toT_l<uint32_t>);
				uuid = utils::slice(data, 4, 20, 16);
				nid = core::readNID(utils::slice(data, 20, 24, 4));
				ASSERT((uuid == pidTagRecordKey), "EntryID uuid does not match pidTagRecordKey");
			}
		};

		class MessageStore
		{
		public:
			MessageStore(core::NID nid, ndb::NDB& ndb)
				: m_nid(nid), m_ndb(ndb)
			{
				/*
				* Property identifier		Property type		Friendly name				Description
				*	0x0FF9					PtypBinary				PidTagRecordKey				Record key. This is the Provider UID of this PST.
				*	0x3001					PtypString				PidTagDisplayName			Display name of PST
				*	0x35E0					PtypBinary				PidTagIpmSubTreeEntryId		EntryID of the Root Mailbox Folder object
				*	0x35E3					PtypBinary				PidTagIpmWastebasketEntryId EntryID of the Deleted Items Folder object
				*	0x35E7					PtypBinary				PidTagFinderEntryId			EntryID of the search Folder object
				*/

				_init();
			}

		public:
			template<typename PropType>
			PropType as(types::PidTagType pt)
			{
				if constexpr (std::is_same_v<PropType, EntryID>)
				{
					return EntryID(
						m_properties.at(pt).asPTBinary().data,
						m_properties.at(types::PidTagType::RecordKey).asPTBinary().data
					);
				}
				else
				{
					return m_properties.at(pt).as<PropType>();
				}
			}

			bool has(types::PidTagType pt)
			{
				return m_properties.count(pt) > 0;
			}

		private:
			void _init()
			{
				m_nbtentry = m_ndb.get(m_nid);
				ndb::BBTEntry bbtentry = m_ndb.get(m_nbtentry.bidData);
				ndb::DataTree dataTree = m_ndb.readDataTree(bbtentry.bref, bbtentry.cb);
				ltp::HN hn(dataTree, m_nbtentry);
				ltp::PropertyContext pc(m_ndb, hn);

				for (const ltp::Property& prop : pc)
				{
					types::PidTagType pt = utils::PidTagType(prop.id);
					if (pt == types::PidTagType::Unknown)
					{
						LOG("[WARN] Unknown property found in MessageStore in hex: [%X]", prop.id);
						continue;
					}
					ASSERT((m_properties.count(pt) == 0), "[ERROR] Duplicate property found in MessageStore");
					m_properties[pt] = prop;
				}
				ASSERT(_hasMinimumRequiredProperties() == true, "[ERROR] MessageStore does not have minimum required properties");
				LOG("[INFO] Message Store Display Name: [%s]", 
					as<ltp::PTString>(types::PidTagType::DisplayName).data.c_str());
			}

			bool _hasMinimumRequiredProperties()
			{
				uint8_t flags{ 0 };
				for (const auto& [pt, prop] : m_properties)
				{
					if (pt == types::PidTagType::RecordKey) // PtypBinary -- PidTagRecordKey -- Record key. This is the Provider UID of this PST.
					{
						ASSERT((prop.propType == types::PropertyType::Binary), "PropType is not PtypBinary");
						flags |= 1;
					}
					else if (pt == types::PidTagType::DisplayName) // PtypString -- PidTagDisplayName -- Display name of PST
					{
						ASSERT((prop.propType == types::PropertyType::String), "PropType is not String");
						flags |= 2;
					}
					else if (pt == types::PidTagType::IpmSubTreeEntryId) // PtypBinary -- PidTagIpmSubTreeEntryId -- EntryID of the Root Mailbox Folder object
					{
						ASSERT((prop.propType == types::PropertyType::Binary), "PropType is not PtypBinary");
						flags |= 4;
					}
					else if (pt == types::PidTagType::IpmWastebasketEntryId) // PtypBinary -- PidTagIpmWastebasketEntryId -- EntryID of the Deleted Items Folder object
					{
						ASSERT((prop.propType == types::PropertyType::Binary), "PropType is not PtypBinary");
						flags |= 8;
					}
					else if (pt == types::PidTagType::FinderEntryId) // PtypBinary -- PidTagFinderEntryId -- EntryID of the search Folder object
					{
						ASSERT((prop.propType == types::PropertyType::Binary), "PropType is not PtypBinary");
						flags |= 16;
					}
				}
				return ((flags & 0x1F) == 0x1F);
			}
				

		private:
			core::NID& m_nid;
			ndb::NBTEntry m_nbtentry;
			ndb::NDB& m_ndb;
			std::map<types::PidTagType, ltp::Property> m_properties;
		};


		class Messaging
		{
		public:
			Messaging(ltp::LTP& ltp, ndb::NDB& ndb)
				: m_ltp(ltp), m_ndb(ndb)
			{
				MessageStore store(core::NID_MESSAGE_STORE, ndb);
				Folder rootFolder(store.as<EntryID>(types::PidTagType::IpmSubTreeEntryId).nid, ndb);
			}
		private:
			ltp::LTP& m_ltp;
			ndb::NDB& m_ndb;
		};
	} // namespace msg
}



#endif // READER_MESSAGING_H