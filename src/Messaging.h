#include <iostream>
#include <stdarg.h>
#include <cassert>
#include <vector>
#include <array>
#include <string>
#include <fstream>
#include <utility>
#include <map>
#include <unordered_map>
#include <optional>

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
				STORYT_ASSERT((nid.getNIDType() == types::NIDType::NORMAL_MESSAGE));
				const std::optional<ndb::NBTEntry> nbt = ndb->get(nid);
				STORYT_ASSERT(nbt.has_value());

				if (nbt.has_value())
				{
					// This SubNodeBTree must not be stored in the MessageObject. It simply allows the PC and TC 
					// objects to set themselves up
					ndb::SubNodeBTree messageSubNodeTree = ndb->InitSubNodeBTree(nbt->bidSub);
					/*
					* nbt.bidData is the location of the dataTree for the PC
					* nbt.bidSub is the location of the SubNodeTree for the Message Object.
					*	This SubNodeTree will be shared amongst the PC, Recip TC, Attach Table TC, and Attach TC
					*/
					ltp::PropertyContext pc = ltp::PropertyContext::Init(nbt->nid, ndb, messageSubNodeTree);
					std::optional<ltp::TableContext> recip = ltp::TableContext::Init(RECIPIENT_TC_NID, messageSubNodeTree);
					std::optional<ltp::TableContext> attachment = ltp::TableContext::Init(ATTACH_NID, messageSubNodeTree);
					std::optional<ltp::TableContext> attachmentTable = ltp::TableContext::Init(ATTACH_TC_NID, messageSubNodeTree);

					LOG(pc.getProperty(types::PidTagTypeCombo::MessageSubject.pid).asPTString().data.c_str());

					STORYT_ASSERT((pc.is(types::PidTagType::MessageClassW, types::PropertyType::String)), "[ERROR]");
					STORYT_ASSERT((pc.is(types::PidTagType::MessageFlags, types::PropertyType::Integer32)), "[ERROR]");
					STORYT_ASSERT((pc.is(types::PidTagType::MessageSize, types::PropertyType::Integer32)), "[ERROR]");
					//ASSERT((pc.is(types::PidTagType::MessageStatus, types::PropertyType::Integer32)), "[ERROR]");
					STORYT_ASSERT((pc.is(types::PidTagType::CreationTime, types::PropertyType::Time)), "[ERROR]");
					STORYT_ASSERT((pc.is(types::PidTagType::LastModificationTime, types::PropertyType::Time)), "[ERROR]");
					STORYT_ASSERT((pc.is(types::PidTagType::SearchKey, types::PropertyType::Binary)), "[ERROR]");

					if (recip.has_value())
					{
						STORYT_ASSERT((recip->hasCol(types::PidTagType::RecipientType, types::PropertyType::Integer32)), "[ERROR]");
						STORYT_ASSERT((recip->hasCol(types::PidTagType::Responsibility, types::PropertyType::Boolean)), "[ERROR]");
						STORYT_ASSERT((recip->hasCol(types::PidTagType::RecordKey, types::PropertyType::Binary)), "[ERROR]");
						STORYT_ASSERT((recip->hasCol(types::PidTagType::ObjectType, types::PropertyType::Integer32)), "[ERROR]");
						STORYT_ASSERT((recip->hasCol(types::PidTagType::EntryId, types::PropertyType::Binary)), "[ERROR]");
						STORYT_ASSERT((recip->hasCol(types::PidTagType::DisplayName, types::PropertyType::String)), "[ERROR]");
						STORYT_ASSERT((recip->hasCol(types::PidTagType::AddressType, types::PropertyType::String)), "[ERROR]");
						STORYT_ASSERT((recip->hasCol(types::PidTagType::EmailAddress, types::PropertyType::String)), "[ERROR]");
						STORYT_ASSERT((recip->hasCol(types::PidTagType::SearchKey, types::PropertyType::Binary)), "[ERROR]");
						STORYT_ASSERT((recip->hasCol(types::PidTagType::DisplayType, types::PropertyType::Integer32)), "[ERROR]");

						STORYT_ASSERT((recip->hasCol(types::PidTagType::SevenBitDisplayName, types::PropertyType::String)), "[ERROR]");
						STORYT_ASSERT((recip->hasCol(types::PidTagType::SendRichInfo, types::PropertyType::Boolean)), "[ERROR]");
						STORYT_ASSERT((recip->hasCol(types::PidTagType::LtpRowId, types::PropertyType::Integer32)), "[ERROR]");
						STORYT_ASSERT((recip->hasCol(types::PidTagType::LtpRowVer, types::PropertyType::Integer32)), "[ERROR]");

						const ltp::TColDesc ltpRowIdCol = recip->getCol(types::PidTagType::LtpRowId);
						const ltp::TColDesc ltpRowVerCol = recip->getCol(types::PidTagType::LtpRowVer);
						STORYT_ASSERT((ltpRowIdCol.iBit == 0 && ltpRowIdCol.ibData == 0 && ltpRowIdCol.cbData == 4), "[ERROR]");
						STORYT_ASSERT((ltpRowVerCol.iBit == 1 && ltpRowVerCol.ibData == 4 && ltpRowVerCol.cbData == 4), "[ERROR]");
					}
					return MessageObject(
						nid,
						ndb,
						std::move(pc),
						std::move(recip),
						std::move(attachment),
						std::move(attachmentTable)
					);
				}
				else
				{
					STORYT_ERROR("Failed to construct message because NBTEntry could not be found with NID [{}]", nid.getNIDRaw());
					return MessageObject(nid, ndb);
				}
			}

		private:
			MessageObject(
				core::NID nid,
				core::Ref<const ndb::NDB> ndb
			)
				: m_nid(nid), m_ndb(ndb) {}

			MessageObject(
				core::NID nid, 
				core::Ref<const ndb::NDB> ndb, 
				std::optional<ltp::PropertyContext> pc,
				std::optional<ltp::TableContext> recip,
				std::optional<ltp::TableContext> attachment,
				std::optional<ltp::TableContext> attachmentTable
				)
				: m_nid(nid), m_ndb(ndb), m_pc(std::move(pc)), m_recip(std::move(recip)),
				m_attachment(std::move(attachment)), m_attachmentTable(std::move(attachmentTable))
			{

			}
		private:
			core::NID m_nid;
			core::Ref<const ndb::NDB> m_ndb;
			std::optional<ndb::SubNodeBTree> m_subtree;
			std::optional<ltp::PropertyContext> m_pc;
			std::optional<ltp::TableContext> m_recip;
			std::optional<ltp::TableContext> m_attachment;
			std::optional<ltp::TableContext> m_attachmentTable;

			/// (required) The subnode is a Message Recipient Table
			static constexpr core::NID RECIPIENT_TC_NID{ 0x692 };		
			/// (optional) The subnode is an Attachment TABLE
			static constexpr core::NID ATTACH_TC_NID{ 0x671 };
			/// (optional) The subnode is an Attachment OBJECT
			static constexpr core::NID ATTACH_NID{ 0x8025 };
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
				std::unordered_map<types::NIDType, ndb::NBTEntry> nbtentries = ndb->all(nid);
				STORYT_ASSERT((nbtentries.size() == 4), "[ERROR] A Folder must be composed of 4 Parts");
				STORYT_ASSERT((nbtentries.count(types::NIDType::NORMAL_FOLDER) == 1), "[ERROR] A Folder must have a NORMAL_FOLDER");
				STORYT_ASSERT((nbtentries.count(types::NIDType::HIERARCHY_TABLE) == 1), "[ERROR] A Folder must have a HIERARCHY_TABLE");
				STORYT_ASSERT((nbtentries.count(types::NIDType::CONTENTS_TABLE) == 1), "[ERROR] A Folder must have a CONTENTS_TABLE");
				STORYT_ASSERT((nbtentries.count(types::NIDType::ASSOC_CONTENTS_TABLE) == 1), "[ERROR] A Folder must have a ASSOC_CONTENTS_TABLE");

				ltp::PropertyContext normal = ltp::PropertyContext::Init(nbtentries.at(types::NIDType::NORMAL_FOLDER).nid, ndb);
				ltp::TableContext hier = ltp::TableContext::Init(nbtentries.at(types::NIDType::HIERARCHY_TABLE).nid, ndb);
				ltp::TableContext contents = ltp::TableContext::Init(nbtentries.at(types::NIDType::CONTENTS_TABLE).nid, ndb);
				ltp::TableContext assoc = ltp::TableContext::Init(nbtentries.at(types::NIDType::ASSOC_CONTENTS_TABLE).nid, ndb);


				STORYT_ASSERT((normal.valid(types::PidTagType::DisplayName, types::PropertyType::String)), "[ERROR]");
				STORYT_ASSERT((normal.valid(types::PidTagType::ContentCount, types::PropertyType::Integer32)), "[ERROR]");
				STORYT_ASSERT((normal.valid(types::PidTagType::ContentUnreadCount, types::PropertyType::Integer32)), "[ERROR]");
				STORYT_ASSERT((normal.valid(types::PidTagType::Subfolders, types::PropertyType::Boolean)), "[ERROR]");
				LOG("[INFO] Folder Name: [%s]",
					normal.getProperty<ltp::PTString>(types::PidTagType::DisplayName).data.c_str());

				{

					 //TODO: this assert doesnt pass. The PidTagType is found but it has the PropType of PTBinary not PTInt32
					//ASSERT((hier.hasCol(types::PidTagType::ReplItemid, types::PropertyType::Integer32)), "[ERROR]");
					STORYT_ASSERT((hier.hasCol(types::PidTagType::ReplChangenum, types::PropertyType::Integer64)), "[ERROR]");
					STORYT_ASSERT((hier.hasCol(types::PidTagType::ReplVersionHistory, types::PropertyType::Binary)), "[ERROR]");
					STORYT_ASSERT((hier.hasCol(types::PidTagType::ReplFlags, types::PropertyType::Integer32)), "[ERROR]");
					STORYT_ASSERT((hier.hasCol(types::PidTagType::DisplayName, types::PropertyType::String)), "[ERROR]");
					STORYT_ASSERT((hier.hasCol(types::PidTagType::ContentCount, types::PropertyType::Integer32)), "[ERROR]");
					STORYT_ASSERT((hier.hasCol(types::PidTagType::ContentUnreadCount, types::PropertyType::Integer32)), "[ERROR]");
					STORYT_ASSERT((hier.hasCol(types::PidTagType::Subfolders, types::PropertyType::Boolean)), "[ERROR]");
					// TODO: this assert doesnt pass. Pid Tag matches but it has PropType of PTstring
					//ASSERT((hier.hasCol(types::PidTagType::ContainerClass, types::PropertyType::Binary)), "[ERROR]");
					STORYT_ASSERT((hier.hasCol(types::PidTagType::PstHiddenCount, types::PropertyType::Integer32)), "[ERROR]");
					STORYT_ASSERT((hier.hasCol(types::PidTagType::PstHiddenUnread, types::PropertyType::Integer32)), "[ERROR]");
					STORYT_ASSERT((hier.hasCol(types::PidTagType::LtpRowId, types::PropertyType::Integer32)), "[ERROR]");
					STORYT_ASSERT((hier.hasCol(types::PidTagType::LtpRowVer, types::PropertyType::Integer32)), "[ERROR]");

					const ltp::TColDesc ltpRowIdCol = hier.getCol(types::PidTagType::LtpRowId);
					const ltp::TColDesc ltpRowVerCol = hier.getCol(types::PidTagType::LtpRowVer);
					STORYT_ASSERT((ltpRowIdCol.iBit == 0 && ltpRowIdCol.ibData == 0 && ltpRowIdCol.cbData == 4), "[ERROR]");
					STORYT_ASSERT((ltpRowVerCol.iBit == 1 && ltpRowVerCol.ibData == 4 && ltpRowVerCol.cbData == 4), "[ERROR]");
				}

				{
					STORYT_ASSERT((contents.hasCol(types::PidTagType::Importance, types::PropertyType::Integer32)), "[ERROR]");
					STORYT_ASSERT((contents.hasCol(types::PidTagType::ClientSubmitTime, types::PropertyType::Time)), "[ERROR]");
					STORYT_ASSERT((contents.hasCol(types::PidTagType::SentRepresentingNameW, types::PropertyType::String)), "[ERROR]");
					STORYT_ASSERT((contents.hasCol(types::PidTagType::MessageToMe, types::PropertyType::Boolean)), "[ERROR]");
					STORYT_ASSERT((contents.hasCol(types::PidTagType::MessageCcMe, types::PropertyType::Boolean)), "[ERROR]");
					STORYT_ASSERT((contents.hasCol(types::PidTagType::ConversationTopicW, types::PropertyType::String)), "[ERROR]");
					STORYT_ASSERT((contents.hasCol(types::PidTagType::ConversationIndex, types::PropertyType::Binary)), "[ERROR]");

					STORYT_ASSERT((contents.hasCol(types::PidTagType::DisplayCcW, types::PropertyType::String)), "[ERROR]");
					STORYT_ASSERT((contents.hasCol(types::PidTagType::DisplayToW, types::PropertyType::String)), "[ERROR]");
					STORYT_ASSERT((contents.hasCol(types::PidTagType::MessageDeliveryTime, types::PropertyType::Time)), "[ERROR]");
					STORYT_ASSERT((contents.hasCol(types::PidTagType::MessageFlags, types::PropertyType::Integer32)), "[ERROR]");
					STORYT_ASSERT((contents.hasCol(types::PidTagType::MessageSize, types::PropertyType::Integer32)), "[ERROR]");
					STORYT_ASSERT((contents.hasCol(types::PidTagType::MessageStatus, types::PropertyType::Integer32)), "[ERROR]");

					//ASSERT((contents.hasCol(types::PidTagType::ReplItemid, types::PropertyType::Integer32)), "[ERROR]");
					STORYT_ASSERT((contents.hasCol(types::PidTagType::ReplChangenum, types::PropertyType::Integer64)), "[ERROR]");
					STORYT_ASSERT((contents.hasCol(types::PidTagType::ReplVersionHistory, types::PropertyType::Binary)), "[ERROR]");
					STORYT_ASSERT((contents.hasCol(types::PidTagType::ReplFlags, types::PropertyType::Integer32)), "[ERROR]");
					STORYT_ASSERT((contents.hasCol(types::PidTagType::ReplCopiedfromVersionhistory, types::PropertyType::Binary)), "[ERROR]");
					STORYT_ASSERT((contents.hasCol(types::PidTagType::ReplCopiedfromItemid, types::PropertyType::Binary)), "[ERROR]");
					STORYT_ASSERT((contents.hasCol(types::PidTagType::ItemTemporaryFlags, types::PropertyType::Integer32)), "[ERROR]");

					const ltp::TColDesc ltpRowIdCol = hier.getCol(types::PidTagType::LtpRowId);
					const ltp::TColDesc ltpRowVerCol = hier.getCol(types::PidTagType::LtpRowVer);
					STORYT_ASSERT((ltpRowIdCol.iBit == 0 && ltpRowIdCol.ibData == 0 && ltpRowIdCol.cbData == 4), "[ERROR]");
					STORYT_ASSERT((ltpRowVerCol.iBit == 1 && ltpRowVerCol.ibData == 4 && ltpRowVerCol.cbData == 4), "[ERROR]");
				}
				return Folder(nid, ndb, std::move(normal), std::move(hier), std::move(contents), std::move(assoc));
			}

			[[nodiscard]] const std::vector<Folder>& getSubFolders() const
			{
				return m_subfolders;
			}

			[[nodiscard]] size_t nSubFolders() const
			{
				return m_subfolders.size();
			}

			[[nodiscard]] std::vector<MessageObject> getNMessages(size_t start, size_t end)
			{
				/*
				* The RowIndex (section 2.3.4.3) of the contents table TC provides an efficient mechanism to locate
				*	the Message object PC node of every Message object in the Folder object. The dwRowIndex field
				*	represents the 0-based Message object row in the Row Matrix, whereas the dwRowID value
				*	represents the NID of the Message object node that corresponds to the row specified by RowIndex.
				*	For example, if a TCROWID is "{ dwRowID=0x200024, dwRowIndex=3 }", the NID that
				*	corresponds to the fourth (first being 0th) Message object row in the Row Matrix is 0x200024.
				*/
				std::vector<MessageObject> messages{};
				const size_t actualEnd = std::min(end, nMessages());
				const std::vector<ltp::TCRowID>& rowIDs = m_contents.getRowIDs();
				for (size_t i = start; i < actualEnd; ++i)
				{
					const ltp::TCRowID& rowid = rowIDs.at(i);
					const core::NID rowNID(rowid.dwRowID);
					messages.push_back(MessageObject::Init(rowNID, m_ndb));
				}
				return messages;
			}

			[[nodiscard]] size_t nMessages() const
			{
				// Returns the number of rows in the Row Index which corresponds to the number of messages
				// contained in the folder.
				return m_contents.nRows();
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
				: 
				m_nid(nid), 
				m_ndb(ndb),  
				m_normal(normal), 	  
				m_hier(hier), 
				m_contents(contents), 
				m_assoc(assoc)
			{
				_setupSubFolders();
				_setupMessages();
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
				for (const auto& rowid : m_hier.getRowIDs())
				{
					const core::NID rowNID(rowid.dwRowID);
					m_subfolders.push_back(Folder::Init(rowNID, m_ndb));
				}
			}

			void _setupMessages()
			{
				for (const auto& rowid : m_contents.getRowIDs())
				{
					const core::NID rowNID(rowid.dwRowID);
					m_messages.push_back(MessageObject::Init(rowNID, m_ndb));
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
			std::vector<MessageObject> m_messages{};
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
				STORYT_ASSERT((data.size() == 24ULL), "EntryID data size is not 24 bytes");
				rgbFlags = utils::slice(data, 0, 4, 4, utils::toT_l<uint32_t>);
				uuid = utils::slice(data, 4, 20, 16);
				nid = core::NID(utils::slice(data, 20, 24, 4));
				STORYT_ASSERT((uuid == pidTagRecordKey), "EntryID uuid does not match pidTagRecordKey");
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
						m_pc.getProperty(pt).asPTBinary().data,
						m_pc.getProperty(types::PidTagType::RecordKey).asPTBinary().data
					);
				}
				else
				{
					return m_pc.getProperty(pt).as<PropType>();
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
				STORYT_ASSERT((m_pc.valid(types::PidTagType::RecordKey, types::PropertyType::Binary)), "[ERROR]");
				STORYT_ASSERT((m_pc.valid(types::PidTagType::DisplayName, types::PropertyType::String)), "[ERROR]");
				STORYT_ASSERT((m_pc.valid(types::PidTagType::IpmSubTreeEntryId, types::PropertyType::Binary)), "[ERROR]");
				STORYT_ASSERT((m_pc.valid(types::PidTagType::IpmWastebasketEntryId, types::PropertyType::Binary)), "[ERROR]");
				STORYT_ASSERT((m_pc.valid(types::PidTagType::FinderEntryId, types::PropertyType::Binary)), "[ERROR]");
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
	} // namespace reader::msg
}



#endif // READER_MESSAGING_H