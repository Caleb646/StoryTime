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
#include "NDB.h"
#include "LTP.h"

#ifndef READER_MESSAGING_H
#define READER_MESSAGING_H

namespace reader
{
	namespace msg
	{
		class MessageObject
		{
		public:
			static MessageObject Init(core::NID nid, core::Ref<const ndb::NDB> ndb)
			{
				const ndb::NBTEntry nbt = ndb->get(nid);
				const ndb::BBTEntry bbtData = ndb->get(nbt.bidData);
				const ndb::BBTEntry bbtSub = ndb->get(nbt.bidSub);
				const ndb::DataTree dataTree = ndb->InitDataTree(bbtData.bref, bbtData.cb);
				return MessageObject();
			}
		};
	
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
			static Folder Init(core::NID nid, core::Ref<const ndb::NDB> ndb)
			{
				std::map<types::NIDType, ndb::NBTEntry> nbtentries = ndb->all(nid);
				ASSERT((nbtentries.size() == 4), "[ERROR] A Folder must be composed of 4 Parts");
				ASSERT((nbtentries.count(types::NIDType::NORMAL_FOLDER) == 1), "[ERROR] A Folder must have a NORMAL_FOLDER");
				ASSERT((nbtentries.count(types::NIDType::HIERARCHY_TABLE) == 1), "[ERROR] A Folder must have a HIERARCHY_TABLE");
				ASSERT((nbtentries.count(types::NIDType::CONTENTS_TABLE) == 1), "[ERROR] A Folder must have a CONTENTS_TABLE");
				ASSERT((nbtentries.count(types::NIDType::ASSOC_CONTENTS_TABLE) == 1), "[ERROR] A Folder must have a ASSOC_CONTENTS_TABLE");

				ltp::PropertyContext normal = ltp::PropertyContext::Init(nbtentries.at(types::NIDType::NORMAL_FOLDER).nid, ndb);
				ltp::TableContext hier = ltp::TableContext::Init(nbtentries.at(types::NIDType::HIERARCHY_TABLE).nid, ndb);
				//ltp::TableContext hier = ltp::TableContext::Init(core::NID(0x60D), ndb);
				ltp::TableContext contents = ltp::TableContext::Init(nbtentries.at(types::NIDType::CONTENTS_TABLE).nid, ndb);
				ltp::TableContext assoc = ltp::TableContext::Init(nbtentries.at(types::NIDType::ASSOC_CONTENTS_TABLE).nid, ndb);


				ASSERT((normal.valid(types::PidTagType::DisplayName, types::PropertyType::String)), "[ERROR]");
				ASSERT((normal.valid(types::PidTagType::ContentCount, types::PropertyType::Integer32)), "[ERROR]");
				ASSERT((normal.valid(types::PidTagType::ContentUnreadCount, types::PropertyType::Integer32)), "[ERROR]");
				ASSERT((normal.valid(types::PidTagType::Subfolders, types::PropertyType::Boolean)), "[ERROR]");

				{

					// TODO: this assert doesnt pass. The PidTagType is found but it has the PropType of PTBinary not PTInt32
					//ASSERT((hier.hasCol(types::PidTagType::ReplItemid, types::PropertyType::Integer32)), "[ERROR]");
					ASSERT((hier.hasCol(types::PidTagType::ReplChangenum, types::PropertyType::Integer64)), "[ERROR]");
					ASSERT((hier.hasCol(types::PidTagType::ReplVersionHistory, types::PropertyType::Binary)), "[ERROR]");
					ASSERT((hier.hasCol(types::PidTagType::ReplFlags, types::PropertyType::Integer32)), "[ERROR]");
					ASSERT((hier.hasCol(types::PidTagType::DisplayName, types::PropertyType::String)), "[ERROR]");
					ASSERT((hier.hasCol(types::PidTagType::ContentCount, types::PropertyType::Integer32)), "[ERROR]");
					ASSERT((hier.hasCol(types::PidTagType::ContentUnreadCount, types::PropertyType::Integer32)), "[ERROR]");
					ASSERT((hier.hasCol(types::PidTagType::Subfolders, types::PropertyType::Boolean)), "[ERROR]");
					// TODO: this assert doesnt pass. Pid Tag matches but it has PropType of PTstring
					//ASSERT((hier.hasCol(types::PidTagType::ContainerClass, types::PropertyType::Binary)), "[ERROR]");
					ASSERT((hier.hasCol(types::PidTagType::PstHiddenCount, types::PropertyType::Integer32)), "[ERROR]");
					ASSERT((hier.hasCol(types::PidTagType::PstHiddenUnread, types::PropertyType::Integer32)), "[ERROR]");
					ASSERT((hier.hasCol(types::PidTagType::LtpRowId, types::PropertyType::Integer32)), "[ERROR]");
					ASSERT((hier.hasCol(types::PidTagType::LtpRowVer, types::PropertyType::Integer32)), "[ERROR]");

					const ltp::TColDesc ltpRowIdCol = hier.findCol(types::PidTagType::LtpRowId);
					const ltp::TColDesc ltpRowVerCol = hier.findCol(types::PidTagType::LtpRowVer);
					ASSERT((ltpRowIdCol.iBit == 0 && ltpRowIdCol.ibData == 0 && ltpRowIdCol.cbData == 4), "[ERROR]");
					ASSERT((ltpRowVerCol.iBit == 1 && ltpRowVerCol.ibData == 4 && ltpRowVerCol.cbData == 4), "[ERROR]");
				}

				{
					ASSERT((contents.hasCol(types::PidTagType::Importance, types::PropertyType::Integer32)), "[ERROR]");
					ASSERT((contents.hasCol(types::PidTagType::ClientSubmitTime, types::PropertyType::Time)), "[ERROR]");
					ASSERT((contents.hasCol(types::PidTagType::SentRepresentingNameW, types::PropertyType::String)), "[ERROR]");
					ASSERT((contents.hasCol(types::PidTagType::MessageToMe, types::PropertyType::Boolean)), "[ERROR]");
					ASSERT((contents.hasCol(types::PidTagType::MessageCcMe, types::PropertyType::Boolean)), "[ERROR]");
					ASSERT((contents.hasCol(types::PidTagType::ConversationTopicW, types::PropertyType::String)), "[ERROR]");
					ASSERT((contents.hasCol(types::PidTagType::ConversationIndex, types::PropertyType::Binary)), "[ERROR]");

					ASSERT((contents.hasCol(types::PidTagType::DisplayCcW, types::PropertyType::String)), "[ERROR]");
					ASSERT((contents.hasCol(types::PidTagType::DisplayToW, types::PropertyType::String)), "[ERROR]");
					ASSERT((contents.hasCol(types::PidTagType::MessageDeliveryTime, types::PropertyType::Time)), "[ERROR]");
					ASSERT((contents.hasCol(types::PidTagType::MessageFlags, types::PropertyType::Integer32)), "[ERROR]");
					ASSERT((contents.hasCol(types::PidTagType::MessageSize, types::PropertyType::Integer32)), "[ERROR]");
					ASSERT((contents.hasCol(types::PidTagType::MessageStatus, types::PropertyType::Integer32)), "[ERROR]");

					//ASSERT((contents.hasCol(types::PidTagType::ReplItemid, types::PropertyType::Integer32)), "[ERROR]");
					ASSERT((contents.hasCol(types::PidTagType::ReplChangenum, types::PropertyType::Integer64)), "[ERROR]");
					ASSERT((contents.hasCol(types::PidTagType::ReplVersionHistory, types::PropertyType::Binary)), "[ERROR]");
					ASSERT((contents.hasCol(types::PidTagType::ReplFlags, types::PropertyType::Integer32)), "[ERROR]");
					ASSERT((contents.hasCol(types::PidTagType::ReplCopiedfromVersionhistory, types::PropertyType::Binary)), "[ERROR]");
					ASSERT((contents.hasCol(types::PidTagType::ReplCopiedfromItemid, types::PropertyType::Binary)), "[ERROR]");
					ASSERT((contents.hasCol(types::PidTagType::ItemTemporaryFlags, types::PropertyType::Integer32)), "[ERROR]");

					const ltp::TColDesc ltpRowIdCol = hier.findCol(types::PidTagType::LtpRowId);
					const ltp::TColDesc ltpRowVerCol = hier.findCol(types::PidTagType::LtpRowVer);
					ASSERT((ltpRowIdCol.iBit == 0 && ltpRowIdCol.ibData == 0 && ltpRowIdCol.cbData == 4), "[ERROR]");
					ASSERT((ltpRowVerCol.iBit == 1 && ltpRowVerCol.ibData == 4 && ltpRowVerCol.cbData == 4), "[ERROR]");
				}
				return Folder(nid, ndb, std::move(normal), std::move(hier), std::move(contents), std::move(assoc));
			}

			std::vector<MessageObject> messages()
			{
				/*
				* The RowIndex (section 2.3.4.3) of the contents table TC provides an efficient mechanism to locate
				*	the Message object PC node of every Message object in the Folder object. The dwRowIndex field
				*	represents the 0-based Message object row in the Row Matrix, whereas the dwRowID value
				*	represents the NID of the Message object node that corresponds to the row specified by RowIndex.
				*	For example, if a TCROWID is "{ dwRowID=0x200024, dwRowIndex=3 }", the NID that
				*	corresponds to the fourth (first being 0th) Message object row in the Row Matrix is 0x200024.
				*/
			}

		private:
			Folder(
				core::NID nid, 
				core::Ref<const ndb::NDB> ndb,
				ltp::PropertyContext&& normal,
				ltp::TableContext&& hier,
				ltp::TableContext&& contents,
				ltp::TableContext&& assoc
				)
				: m_nid(nid), m_ndb(ndb),  m_normal(normal), 
				  m_hier(hier), m_contents(contents), 
				  m_assoc(assoc)
			{
				_setupSubFolders();
			}

			void _setupSubFolders()
			{
				/*
				* The RowIndex (section 2.3.4.3) of the hierarchy table TC provides a mechanism for efficiently
				* locating immediate sub-Folder objects. The dwRowIndex field represents the 0-based sub-Folder
				* object row in the Row Matrix, whereas the dwRowID value represents the NID of the sub-Folder
				* object node that corresponds to the row specified by RowIndex. For example, if a TCROWID is: "{
				* dwRowID=0x8022, dwRowIndex=3 }", the sub-Folder object NID that corresponds to the fourth
				* (first being 0th) sub-Folder object row in the Row Matrix is 0x8022.
				*/

				std::vector<ltp::TCRowID> rowIDs = m_hier.rowIDs();
				for (const auto& rowid : rowIDs)
				{
					core::NID rowNID(rowid.dwRowID);
					m_subfolders.push_back(Folder::Init(rowNID, m_ndb));
				}
			}

		private:
			core::NID m_nid;
			core::Ref<const ndb::NDB> m_ndb;
			ltp::PropertyContext m_normal;
			ltp::TableContext m_hier;
			ltp::TableContext m_contents;
			ltp::TableContext m_assoc;
			std::vector<Folder> m_subfolders{};
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
			static MessageStore Init(core::NID nid, core::Ref<const ndb::NDB> ndb)
			{
				static_assert(std::is_move_constructible_v<MessageStore>, "Property context must be move constructible");
				static_assert(std::is_move_assignable_v<MessageStore>, "Property context must be move constructible");
				static_assert(std::is_copy_constructible_v<MessageStore>, "Property context must be move constructible");
				static_assert(std::is_copy_assignable_v<MessageStore>, "Property context must be move constructible");
				return MessageStore(nid, ndb, ltp::PropertyContext::Init(nid, ndb));
			}

		public:
			template<typename PropType>
			PropType as(types::PidTagType pt)
			{
				if constexpr (std::is_same_v<PropType, EntryID>)
				{
					return EntryID(
						m_pc.at(pt).asPTBinary().data,
						m_pc.at(types::PidTagType::RecordKey).asPTBinary().data
					);
				}
				else
				{
					return m_pc.at(pt).as<PropType>();
				}
			}

		private:
			MessageStore(core::NID nid, core::Ref<const ndb::NDB> ndb, ltp::PropertyContext&& pc)
				: m_nid(nid), m_ndb(ndb), m_pc(pc)
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

			void _init()
			{
				ASSERT((m_pc.valid(types::PidTagType::RecordKey, types::PropertyType::Binary)), "[ERROR]");
				ASSERT((m_pc.valid(types::PidTagType::DisplayName, types::PropertyType::String)), "[ERROR]");
				ASSERT((m_pc.valid(types::PidTagType::IpmSubTreeEntryId, types::PropertyType::Binary)), "[ERROR]");
				ASSERT((m_pc.valid(types::PidTagType::IpmWastebasketEntryId, types::PropertyType::Binary)), "[ERROR]");
				ASSERT((m_pc.valid(types::PidTagType::FinderEntryId, types::PropertyType::Binary)), "[ERROR]");
				LOG("[INFO] Message Store Display Name: [%s]", 
					as<ltp::PTString>(types::PidTagType::DisplayName).data.c_str());
			}

		private:
			core::NID m_nid;
			core::Ref<const ndb::NDB> m_ndb;
			ltp::PropertyContext m_pc;
		};


		class Messaging
		{
		public:
			Messaging(core::Ref<const ndb::NDB> ndb, core::Ref<const ltp::LTP> ltp)
				: m_ndb(ndb), m_ltp(ltp)
			{
				MessageStore store = MessageStore::Init(core::NID_MESSAGE_STORE, ndb);
				Folder rootFolder = Folder::Init(store.as<EntryID>(types::PidTagType::IpmSubTreeEntryId).nid, ndb);
			}
		private:
			core::Ref<const ltp::LTP> m_ltp;
			core::Ref<const ndb::NDB> m_ndb;
		};
	} // namespace msg
}



#endif // READER_MESSAGING_H