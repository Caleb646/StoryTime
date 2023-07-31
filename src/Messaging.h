#include <iostream>
#include <cassert>
#include <vector>
#include <array>
#include <string>
#include <fstream>
#include <utility>
#include <unordered_map>
#include <optional>
#include <regex>

#include "types.h"
#include "utils.h"
#include "core.h"
#include "NDB.h"
#include "LTP.h"

#ifndef READER_MESSAGING_H
#define READER_MESSAGING_H

namespace reader
{
	class Attachment
	{
	public:
		explicit Attachment(ltp::PropertyContext&& pc)
			: m_pc(std::move(pc)) 
		{
			VerifyAttachmentPropertyContextIsValid_();
			STORYT_INFO("Attachment Mime Type Header [{}]", getMimeType());
		}
		[[nodiscard]] int32_t getSize()
		{
			ltp::Property* prop = m_pc.TryToGetProperty(types::PidTagType::AttachSize, types::PropertyType::Integer32);
			if (prop)
			{
				return prop->asPTInt32();
			}
			STORYT_ASSERT(false, "Failed to getSize of Attachment");
			return -1;
		}
		[[nodiscard]] int32_t getMethod()
		{
			ltp::Property* prop = m_pc.TryToGetProperty(types::PidTagType::AttachMethod, types::PropertyType::Integer32);
			if (prop)
			{
				return prop->asPTInt32();
			}
			STORYT_ASSERT(false, "Failed to getMethod of Attachment");
			return -1;
		}
		[[nodiscard]] std::string getMimeType()
		{
			ltp::Property* prop = m_pc.TryToGetProperty(types::PidTagType::AttachMimeTag, types::PropertyType::String);
			if (prop)
			{
				return prop->asPTString().data;
			}
			STORYT_ASSERT(false, "Failed to getMimeType of Attachment");
			return {};
		}
		[[nodiscard]] std::vector<types::byte_t> getContent()
		{
			/*
			* The actual binary content of an attachment (if any) is stored in PidTagAttachDataBinary. However,
			* if the attachment is itself a message, the data is stored in PidTagAttachDataObject. In this case,
			* the nid value of the PtypObject property structure is a subnode which is a fully formed message
			* with the exception that such attached messages are not located in the NBT and do not have a parent folder.
			*/
			{
				ltp::Property* data = m_pc.TryToGetProperty(types::PidTagType::AttachDataBinaryOrDataObject, types::PropertyType::Binary);
				if (data)
				{
					return data->data;
				}
			}
			{
				ltp::Property* data = m_pc.TryToGetProperty(types::PidTagType::AttachDataBinaryOrDataObject, types::PropertyType::Object);
				if (data)
				{
					STORYT_ASSERT(false,
						"Attachments with AttachDataObject set are nested Message Objects and they are not implemented currently");
					return {};
				}
			}
			STORYT_ASSERT(false, 
				"Attachments content PropType was not valid");
			return {};
		}
	private:
		void VerifyAttachmentPropertyContextIsValid_() const
		{
			STORYT_ASSERT((m_pc.HasPropertyWPidOf(types::PidTagType::AttachSize)),
				"AttachSize must be present on valid Attachment");
			STORYT_ASSERT((m_pc.HasPropertyWPidOf(types::PidTagType::AttachMethod)),
				"AttachMethod must be present on valid Attachment");
		}
	private:
		ltp::PropertyContext m_pc;
	};

	class AttachmentTable
	{
	public:
		/// (optional) The subnode is an Attachment TABLE
		static constexpr core::NID ATTACH_TC_NID{ 0x671 };
		/// (optional) The subnode is an Attachment OBJECT
		static constexpr core::NID ATTACH_NID{ 0x8025 };
	public:
		static std::optional<AttachmentTable> Init(ndb::SubNodeBTree& messageObjectSubTree)
		{
			std::optional<ltp::TableContext> attachTable = ltp::TableContext::Init(ATTACH_TC_NID, messageObjectSubTree);
			if (attachTable.has_value())
			{
				return std::optional(AttachmentTable(std::move(*attachTable), messageObjectSubTree));
			}
			return std::optional<AttachmentTable>(std::nullopt);
		}
		[[nodiscard]] std::vector<Attachment>* getAttachments()
		{
			return &m_attachments;
		}
	private:
		explicit AttachmentTable(ltp::TableContext&& tc, ndb::SubNodeBTree& messageObjectSubTree)
			: m_tc(std::move(tc))
		{
			VerifyAttachmentTableIsValid_();
			_setupAttachments(messageObjectSubTree);
		}
		void VerifyAttachmentTableIsValid_() const
		{
			STORYT_ASSERT((m_tc.hasColumn(types::PidTagType::AttachSize, types::PropertyType::Integer32)),
				"Failed to find m_tc.hasCol(types::PidTagType::AttachSize, types::PropertyType::Integer32)");
			STORYT_ASSERT((m_tc.hasColumn(types::PidTagType::AttachFileName, types::PropertyType::String)),
				"Failed to find m_tc.hasCol(types::PidTagType::AttachFileName, types::PropertyType::String)");
			STORYT_ASSERT((m_tc.hasColumn(types::PidTagType::AttachMethod, types::PropertyType::Integer32)),
				"Failed to find m_tc.hasCol(types::PidTagType::AttachMethod, types::PropertyType::Integer32)");
			STORYT_ASSERT((m_tc.hasColumn(types::PidTagType::RenderingPosition, types::PropertyType::Integer32)),
				"Failed to find m_tc.hasCol(types::PidTagType::RenderingPosition, types::PropertyType::Integer32)");
			STORYT_ASSERT((m_tc.hasColumn(types::PidTagType::LtpRowId, types::PropertyType::Integer32)),
				"Failed to find m_tc.hasCol(types::PidTagType::LtpRowId, types::PropertyType::Integer32)");
			STORYT_ASSERT((m_tc.hasColumn(types::PidTagType::LtpRowVer, types::PropertyType::Integer32)),
				"Failed to find m_tc.hasCol(types::PidTagType::LtpRowVer, types::PropertyType::Integer32)");
		}
		void _setupAttachments(ndb::SubNodeBTree& messageObjectSubTree)
		{
			m_attachments.reserve(m_tc.nRows());
			for (const auto& rowID : m_tc.getRowIDs())
			{
				const core::NID attachNID = core::NID(rowID.dwRowID);
				ndb::DataTree* datatree = messageObjectSubTree.getDataTree(attachNID);
				ndb::SubNodeBTree* childSubTree = messageObjectSubTree.getNestedSubNodeTree(attachNID);
				ltp::PropertyContext pc = ltp::PropertyContext::Init(attachNID, datatree, childSubTree);
				m_attachments.push_back(Attachment(std::move(pc)));
			}
			STORYT_ASSERT((m_tc.nRows() == m_attachments.size()), 
				"The number of rows in the row index and attachments must be equal");
		}
	private:
		ltp::TableContext m_tc;
		std::vector<Attachment> m_attachments;
	};

	class MessageObject
	{
	public:
		static MessageObject Init(core::NID nid, core::Ref<const ndb::NDB> ndb)
		{
			STORYT_ASSERT((nid.getNIDType() == types::NIDType::NORMAL_MESSAGE), 
				"Invalid NIDType for Messsage Object [{}]", static_cast<uint32_t>(nid.getNIDType()));
			const std::optional<ndb::NBTEntry> nbt = ndb->get(nid);
			STORYT_ASSERT(nbt.has_value(), 
				"Failed to find NBTEntry for NID [{}]", nid.getNIDRaw());

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
				//STORYT_INFO(pc.getProperty(types::PidTagTypeCombo::MessageSubject.pid).asPTString().data.c_str());
				std::optional<ltp::TableContext> recip = ltp::TableContext::Init(RECIPIENT_TC_NID, messageSubNodeTree);
				std::optional<AttachmentTable> attachmentTable = AttachmentTable::Init(messageSubNodeTree);

				return MessageObject(
					nid,
					std::move(pc),
					std::move(recip),
					std::move(attachmentTable)
				);
			}
			else
			{
				STORYT_ERROR("Failed to construct message because NBTEntry could not be found with NID [{}]", nid.getNIDRaw());
				return MessageObject(nid);
			}
		}

		[[nodiscard]] bool hasAttachments() const
		{
			return m_attachmentTable.has_value();
		}

		[[nodiscard]] std::vector<Attachment>* getAttachments()
		{
			if (hasAttachments())
			{
				return m_attachmentTable->getAttachments();
			}
			STORYT_WARN("Message Object with NID [{}] does not have any attachments", m_nid.getNIDRaw());
			return nullptr;
		}

		[[nodiscard]] std::string getSender()
		{
			ltp::Property* prop = TryToGetProperty_(types::PidTagTypeCombo::SenderEmailAddress);
			if (prop)
			{
				return prop->asPTString().data;
			}
			STORYT_ASSERT(false, "Failed to getFrom for Message with NID [{}]", m_nid.getNIDRaw());
			return {};
		}

		[[nodiscard]] std::vector<std::string> getRecipients()
		{
			std::vector<std::string> emailAddresses{};
			if (m_recip.has_value())
			{
				m_recip->loadRowMatrix(); // Make sure recip Row Matrix is loaded
				emailAddresses.reserve(m_recip->nRows());
				auto emailInfo = types::PidTagTypeCombo::RecipEmailAddress;
				for (const auto& rowID : m_recip->getRowIDs())
				{
					ltp::RowEntry* rowEntry = m_recip->getSingleRowAndLoadColumn(rowID, static_cast<types::PidTagType>(emailInfo.pid));
					if (rowEntry != nullptr)
					{
						emailAddresses.push_back(utils::UTF16BytesToString(rowEntry->data));
					}
				}
				return emailAddresses;
			}
			STORYT_ERROR("Failed to getRecipients for Message with NID [{}]", m_nid.getNIDRaw());
			return emailAddresses;
		}

		[[nodiscard]] std::string getSubject()
		{
			ltp::Property* prop = TryToGetProperty_(types::PidTagTypeCombo::MessageSubject);
			if (prop)
			{
				return prop->asPTString().data;
			}
			STORYT_ASSERT(false, "Failed to getSubject for Message with NID [{}]", m_nid.getNIDRaw());
			return {};
		}

		[[nodiscard]] std::string getBody()
		{
			{
				ltp::Property* prop = TryToGetProperty_(types::PidTagTypeCombo::MessageBody);
				if (prop)
				{
					return prop->asPTString().data;
				}
			}
			{
				ltp::Property* prop = TryToGetProperty_(types::PidTagTypeCombo::BodyHtml);
				if (prop)
				{
					return prop->asPTString().data;
				}
			}
			STORYT_ASSERT(false, "Failed to getBody for Message with NID [{}] and Subject of [{}]", m_nid.getNIDRaw(), getSubject());
			return {};
		}

	private:
		explicit MessageObject(core::NID nid) : m_nid(nid)
		{
			_init();
		}

		explicit MessageObject(
			core::NID nid, 
			std::optional<ltp::PropertyContext> pc,
			std::optional<ltp::TableContext> recip,
			std::optional<AttachmentTable> attachmentTable
			)
			: 
			m_nid(nid), 
			m_pc(std::move(pc)), 
			m_recip(std::move(recip)), 
			m_attachmentTable(std::move(attachmentTable)) 
		{
			_init();
		}

		ltp::Property* TryToGetProperty_(types::PidTagTypeCombo::Info info)
		{
			if (m_pc.has_value())
			{
				if (m_pc->HasPropertyWPidAndPtypeOf(info))
				{
					return m_pc->TryToGetProperty(info);
				}
			}
			STORYT_ERROR("Failed to TryToGetProperty_ for Message with NID [{}]", m_nid.getNIDRaw());
			return nullptr;
		}

		void _init()
		{
			VerifyMessagePropertyContextIsValid_();
			VerfiyMessageRecipientTableContextIsValid_();
		}

		void VerifyMessagePropertyContextIsValid_() const
		{
			if (m_pc.has_value())
			{
				STORYT_ASSERT((m_pc->HasPropertyWPidAndPtypeOf(types::PidTagType::MessageClassW, types::PropertyType::String)),
					"Failed: pc.is(types::PidTagType::MessageClassW, types::PropertyType::String)");
				STORYT_ASSERT((m_pc->HasPropertyWPidAndPtypeOf(types::PidTagType::MessageFlags, types::PropertyType::Integer32)),
					"Failed: pc.is(types::PidTagType::MessageFlags, types::PropertyType::Integer32)");
				STORYT_ASSERT((m_pc->HasPropertyWPidAndPtypeOf(types::PidTagType::MessageSize, types::PropertyType::Integer32)),
					"Failed: pc.is(types::PidTagType::MessageSize, types::PropertyType::Integer32)");
				STORYT_ASSERT((m_pc->HasPropertyWPidAndPtypeOf(types::PidTagType::CreationTime, types::PropertyType::Time)),
					"Failed: pc.is(types::PidTagType::CreationTime, types::PropertyType::Time)");
				STORYT_ASSERT((m_pc->HasPropertyWPidAndPtypeOf(types::PidTagType::LastModificationTime, types::PropertyType::Time)),
					"Failed: pc.is(types::PidTagType::LastModificationTime, types::PropertyType::Time)");
				STORYT_ASSERT((m_pc->HasPropertyWPidAndPtypeOf(types::PidTagType::SearchKey, types::PropertyType::Binary)),
					"Failed: pc.is(types::PidTagType::SearchKey, types::PropertyType::Binary)");
			}
			else
			{
				STORYT_ASSERT(false, "m_pc has not been initialized for Message with NID [{}]", m_nid.getNIDRaw());
			}
		}

		void VerfiyMessageRecipientTableContextIsValid_() const
		{
			if (m_recip.has_value())
			{
				STORYT_ASSERT((m_recip->hasColumn(types::PidTagType::RecipientType, types::PropertyType::Integer32)),
					"Failed: recip->hasCol(types::PidTagType::RecipientType, types::PropertyType::Integer32)");
				STORYT_ASSERT((m_recip->hasColumn(types::PidTagType::Responsibility, types::PropertyType::Boolean)),
					"Failed: recip->hasCol(types::PidTagType::Responsibility, types::PropertyType::Boolean)");
				STORYT_ASSERT((m_recip->hasColumn(types::PidTagType::RecordKey, types::PropertyType::Binary)),
					"Failed: recip->hasCol(types::PidTagType::RecordKey, types::PropertyType::Binary)");
				STORYT_ASSERT((m_recip->hasColumn(types::PidTagType::ObjectType, types::PropertyType::Integer32)),
					"Failed: recip->hasCol(types::PidTagType::ObjectType, types::PropertyType::Integer32)");
				STORYT_ASSERT((m_recip->hasColumn(types::PidTagType::EntryId, types::PropertyType::Binary)),
					"Failed: recip->hasCol(types::PidTagType::EntryId, types::PropertyType::Binary)");
				STORYT_ASSERT((m_recip->hasColumn(types::PidTagType::DisplayName, types::PropertyType::String)),
					"Failed: recip->hasCol(types::PidTagType::DisplayName, types::PropertyType::String)");
				STORYT_ASSERT((m_recip->hasColumn(types::PidTagType::AddressType, types::PropertyType::String)),
					"Failed: recip->hasCol(types::PidTagType::AddressType, types::PropertyType::String)");
				STORYT_ASSERT((m_recip->hasColumn(types::PidTagType::EmailAddress, types::PropertyType::String)),
					"Failed: recip->hasCol(types::PidTagType::EmailAddress, types::PropertyType::String)");
				STORYT_ASSERT((m_recip->hasColumn(types::PidTagType::SearchKey, types::PropertyType::Binary)),
					"Failed: recip->hasCol(types::PidTagType::SearchKey, types::PropertyType::Binary)");
				STORYT_ASSERT((m_recip->hasColumn(types::PidTagType::DisplayType, types::PropertyType::Integer32)),
					"Failed: recip->hasCol(types::PidTagType::DisplayType, types::PropertyType::Integer32)");
				STORYT_ASSERT((m_recip->hasColumn(types::PidTagType::SevenBitDisplayName, types::PropertyType::String)),
					"Failed: recip->hasCol(types::PidTagType::SevenBitDisplayName, types::PropertyType::String)");
				STORYT_ASSERT((m_recip->hasColumn(types::PidTagType::SendRichInfo, types::PropertyType::Boolean)),
					"Failed: recip->hasCol(types::PidTagType::SendRichInfo, types::PropertyType::Boolean)");
				STORYT_ASSERT((m_recip->hasColumn(types::PidTagType::LtpRowId, types::PropertyType::Integer32)),
					"Failed: recip->hasCol(types::PidTagType::LtpRowId, types::PropertyType::Integer32)");
				STORYT_ASSERT((m_recip->hasColumn(types::PidTagType::LtpRowVer, types::PropertyType::Integer32)),
					"Failed: recip->hasCol(types::PidTagType::LtpRowVer, types::PropertyType::Integer32)");

				const ltp::TColDesc ltpRowIdCol = m_recip->getColumn(types::PidTagType::LtpRowId);
				const ltp::TColDesc ltpRowVerCol = m_recip->getColumn(types::PidTagType::LtpRowVer);
				STORYT_ASSERT((ltpRowIdCol.iBit == 0 && ltpRowIdCol.ibData == 0 && ltpRowIdCol.cbData == 4),
					"Failed: ltpRowIdCol.iBit == 0 && ltpRowIdCol.ibData == 0 && ltpRowIdCol.cbData == 4");
				STORYT_ASSERT((ltpRowVerCol.iBit == 1 && ltpRowVerCol.ibData == 4 && ltpRowVerCol.cbData == 4),
					"Failed: ltpRowVerCol.iBit == 1 && ltpRowVerCol.ibData == 4 && ltpRowVerCol.cbData == 4");
			}
			else
			{
				//STORYT_ASSERT(false, "m_recip is NOT setup for Message with NID [{}]", m_nid.getNIDRaw());
				STORYT_ERROR("m_recip is NOT setup for Message with NID [{}]", m_nid.getNIDRaw());
			}	
		}

	private:
		core::NID m_nid;
		std::optional<ltp::PropertyContext> m_pc;
		std::optional<ltp::TableContext> m_recip;
		std::optional<AttachmentTable> m_attachmentTable;
		/// (required) The subnode is a Message Recipient Table
		static constexpr core::NID RECIPIENT_TC_NID{ 0x692 };		
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
			STORYT_ASSERT((nbtentries.size() >= 4), "A Folder must be composed of at least 4 Parts");
			STORYT_ASSERT(nbtentries.contains(types::NIDType::NORMAL_FOLDER), "A Folder must have a NORMAL_FOLDER");
			STORYT_ASSERT(nbtentries.contains(types::NIDType::HIERARCHY_TABLE), "A Folder must have a HIERARCHY_TABLE");
			STORYT_ASSERT(nbtentries.contains(types::NIDType::CONTENTS_TABLE), "A Folder must have a CONTENTS_TABLE");
			STORYT_ASSERT(nbtentries.contains(types::NIDType::ASSOC_CONTENTS_TABLE), "A Folder must have a ASSOC_CONTENTS_TABLE");

			ltp::PropertyContext normal = ltp::PropertyContext::Init(nbtentries.at(types::NIDType::NORMAL_FOLDER).nid, ndb);
			ltp::TableContext hier = ltp::TableContext::Init(nbtentries.at(types::NIDType::HIERARCHY_TABLE).nid, ndb);
			ltp::TableContext contents = ltp::TableContext::Init(nbtentries.at(types::NIDType::CONTENTS_TABLE).nid, ndb);
			ltp::TableContext assoc = ltp::TableContext::Init(nbtentries.at(types::NIDType::ASSOC_CONTENTS_TABLE).nid, ndb);
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

		[[nodiscard]] std::vector<MessageObject> getNMessages(size_t start, size_t end) const
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

		[[nodiscard]] const std::string& getName()
		{
			return m_folderName;
		}

		template<typename FolderID>
		[[nodiscard]] Folder* getFolder(const FolderID& folderID)
		{
			if constexpr (std::is_same_v<FolderID, core::NID>) // Match folder on NID
			{
				if (m_nid == folderID)
				{
					return this;
				}
			}
			else if constexpr (std::is_same_v<FolderID, std::string>) // Match folder on Folder Name
			{
				if (getName().find(folderID) != std::string::npos)
				{
					return this;
				}
			}
			else if constexpr (std::is_same_v<FolderID, std::regex>) // Match folder on Folder Name Regex
			{
				const std::string& folderName = getName();
				if (std::regex_search(folderName, std::regex(folderID)))
				{
					return this;
				}
			}
			else
			{
				STORYT_ASSERT(false, "Invalid FolderID Type");
			}
			
			for (Folder& folder : m_subfolders)
			{
				Folder* f = folder.getFolder(folderID);
				if (f != nullptr)
				{
					return f;
				}
			}
			return nullptr;
		}

		[[nodiscard]] core::NID getNID() const
		{
			return m_nid;
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
			VerifyFolderNormalPropertContextIsValid_();
			VerifyFolderHierarchyTableContextIsValid_();
			VerifyFolderContentsTableContextIsValid_();
			_setupFolderName();
			_setupSubFolders();
			//_setupMessages();
		}

		void VerifyFolderNormalPropertContextIsValid_() const
		{
			STORYT_ASSERT((m_normal.HasPropertyWPidAndPtypeOf(types::PidTagType::DisplayName, types::PropertyType::String)),
				"Failed: normal.valid(types::PidTagType::DisplayName, types::PropertyType::String)");
			STORYT_ASSERT((m_normal.HasPropertyWPidAndPtypeOf(types::PidTagType::ContentCount, types::PropertyType::Integer32)),
				"Failed: normal.valid(types::PidTagType::ContentCount, types::PropertyType::Integer32)");
			STORYT_ASSERT((m_normal.HasPropertyWPidAndPtypeOf(types::PidTagType::ContentUnreadCount, types::PropertyType::Integer32)),
				"Failed: normal.valid(types::PidTagType::ContentUnreadCount, types::PropertyType::Integer32)");
			STORYT_ASSERT((m_normal.HasPropertyWPidAndPtypeOf(types::PidTagType::Subfolders, types::PropertyType::Boolean)),
				"Failed: normal.valid(types::PidTagType::Subfolders, types::PropertyType::Boolean)");
		}

		void VerifyFolderHierarchyTableContextIsValid_() const
		{
			//TODO: this assert doesnt pass. The PidTagType is found but it has the PropType of PTBinary not PTInt32
				//ASSERT((hier.hasCol(types::PidTagType::ReplItemid, types::PropertyType::Integer32)), "[ERROR]");
			STORYT_ASSERT((m_hier.hasColumn(types::PidTagType::ReplChangenum, types::PropertyType::Integer64)), "[ERROR]");
			STORYT_ASSERT((m_hier.hasColumn(types::PidTagType::ReplVersionHistory, types::PropertyType::Binary)), "[ERROR]");
			STORYT_ASSERT((m_hier.hasColumn(types::PidTagType::ReplFlags, types::PropertyType::Integer32)), "[ERROR]");
			STORYT_ASSERT((m_hier.hasColumn(types::PidTagType::DisplayName, types::PropertyType::String)), "[ERROR]");
			STORYT_ASSERT((m_hier.hasColumn(types::PidTagType::ContentCount, types::PropertyType::Integer32)), "[ERROR]");
			STORYT_ASSERT((m_hier.hasColumn(types::PidTagType::ContentUnreadCount, types::PropertyType::Integer32)), "[ERROR]");
			STORYT_ASSERT((m_hier.hasColumn(types::PidTagType::Subfolders, types::PropertyType::Boolean)), "[ERROR]");
			// TODO: this assert doesnt pass. Pid Tag matches but it has PropType of PTstring
			//ASSERT((hier.hasColumn(types::PidTagType::ContainerClass, types::PropertyType::Binary)), "[ERROR]");
			STORYT_ASSERT((m_hier.hasColumn(types::PidTagType::PstHiddenCount, types::PropertyType::Integer32)), "[ERROR]");
			STORYT_ASSERT((m_hier.hasColumn(types::PidTagType::PstHiddenUnread, types::PropertyType::Integer32)), "[ERROR]");
			STORYT_ASSERT((m_hier.hasColumn(types::PidTagType::LtpRowId, types::PropertyType::Integer32)), "[ERROR]");
			STORYT_ASSERT((m_hier.hasColumn(types::PidTagType::LtpRowVer, types::PropertyType::Integer32)), "[ERROR]");

			const ltp::TColDesc ltpRowIdCol = m_hier.getColumn(types::PidTagType::LtpRowId);
			const ltp::TColDesc ltpRowVerCol = m_hier.getColumn(types::PidTagType::LtpRowVer);
			STORYT_ASSERT((ltpRowIdCol.iBit == 0 && ltpRowIdCol.ibData == 0 && ltpRowIdCol.cbData == 4), "[ERROR]");
			STORYT_ASSERT((ltpRowVerCol.iBit == 1 && ltpRowVerCol.ibData == 4 && ltpRowVerCol.cbData == 4), "[ERROR]");
		}

		void VerifyFolderContentsTableContextIsValid_() const
		{
			STORYT_ASSERT((m_contents.hasColumn(types::PidTagType::Importance, types::PropertyType::Integer32)), "[ERROR]");
			STORYT_ASSERT((m_contents.hasColumn(types::PidTagType::ClientSubmitTime, types::PropertyType::Time)), "[ERROR]");
			STORYT_ASSERT((m_contents.hasColumn(types::PidTagType::SentRepresentingNameW, types::PropertyType::String)), "[ERROR]");
			STORYT_ASSERT((m_contents.hasColumn(types::PidTagType::MessageToMe, types::PropertyType::Boolean)), "[ERROR]");
			STORYT_ASSERT((m_contents.hasColumn(types::PidTagType::MessageCcMe, types::PropertyType::Boolean)), "[ERROR]");
			STORYT_ASSERT((m_contents.hasColumn(types::PidTagType::ConversationTopicW, types::PropertyType::String)), "[ERROR]");
			STORYT_ASSERT((m_contents.hasColumn(types::PidTagType::ConversationIndex, types::PropertyType::Binary)), "[ERROR]");

			STORYT_ASSERT((m_contents.hasColumn(types::PidTagType::DisplayCcW, types::PropertyType::String)), "[ERROR]");
			STORYT_ASSERT((m_contents.hasColumn(types::PidTagType::DisplayToW, types::PropertyType::String)), "[ERROR]");
			STORYT_ASSERT((m_contents.hasColumn(types::PidTagType::MessageDeliveryTime, types::PropertyType::Time)), "[ERROR]");
			STORYT_ASSERT((m_contents.hasColumn(types::PidTagType::MessageFlags, types::PropertyType::Integer32)), "[ERROR]");
			STORYT_ASSERT((m_contents.hasColumn(types::PidTagType::MessageSize, types::PropertyType::Integer32)), "[ERROR]");
			STORYT_ASSERT((m_contents.hasColumn(types::PidTagType::MessageStatus, types::PropertyType::Integer32)), "[ERROR]");

			//ASSERT((contents.hasCol(types::PidTagType::ReplItemid, types::PropertyType::Integer32)), "[ERROR]");
			STORYT_ASSERT((m_contents.hasColumn(types::PidTagType::ReplChangenum, types::PropertyType::Integer64)), "[ERROR]");
			STORYT_ASSERT((m_contents.hasColumn(types::PidTagType::ReplVersionHistory, types::PropertyType::Binary)), "[ERROR]");
			STORYT_ASSERT((m_contents.hasColumn(types::PidTagType::ReplFlags, types::PropertyType::Integer32)), "[ERROR]");
			STORYT_ASSERT((m_contents.hasColumn(types::PidTagType::ReplCopiedfromVersionhistory, types::PropertyType::Binary)), "[ERROR]");
			STORYT_ASSERT((m_contents.hasColumn(types::PidTagType::ReplCopiedfromItemid, types::PropertyType::Binary)), "[ERROR]");
			STORYT_ASSERT((m_contents.hasColumn(types::PidTagType::ItemTemporaryFlags, types::PropertyType::Integer32)), "[ERROR]");

			const ltp::TColDesc ltpRowIdCol = m_contents.getColumn(types::PidTagType::LtpRowId);
			const ltp::TColDesc ltpRowVerCol = m_contents.getColumn(types::PidTagType::LtpRowVer);
			STORYT_ASSERT((ltpRowIdCol.iBit == 0 && ltpRowIdCol.ibData == 0 && ltpRowIdCol.cbData == 4), "[ERROR]");
			STORYT_ASSERT((ltpRowVerCol.iBit == 1 && ltpRowVerCol.ibData == 4 && ltpRowVerCol.cbData == 4), "[ERROR]");
		}

		void _setupFolderName()
		{
			ltp::Property* prop = m_normal.TryToGetProperty(types::PidTagType::DisplayName, types::PropertyType::String);
			if (prop)
			{
				m_folderName = prop->asPTString().data;
			}
			else
			{
				STORYT_ASSERT(false, "Failed to Setup Folder Name with NID of [{}]", m_nid.getNIDRaw());
			}
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
		std::string m_folderName{};
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
		std::optional<EntryID> getEntryID(types::PidTagType pid)
		{
			ltp::Property* entryIDProp = m_pc.TryToGetProperty(pid, types::PropertyType::Binary);
			ltp::Property* entryRecordKeyProp = m_pc.TryToGetProperty(types::PidTagType::RecordKey, types::PropertyType::Binary);
			if (entryIDProp && entryRecordKeyProp)
			{
				return EntryID(
					entryIDProp->asPTBinary().data,
					entryRecordKeyProp->data
				);
			}
			STORYT_ASSERT(false, "Failed to EntryID with PID of [{}]", static_cast<uint32_t>(pid));
			return std::nullopt;
			
		}

	private:
		MessageStore(core::NID nid, core::Ref<const ndb::NDB> ndb, ltp::PropertyContext&& pc)
			: m_nid(nid), m_ndb(ndb), m_pc(pc)
		{
			VerifyMessageStorePropertyContextIsValid_();
	/*		STORYT_INFO("Message Store Display Name: [{}]",
				as<ltp::PTString>(types::PidTagType::DisplayName).data);*/
		}

		void VerifyMessageStorePropertyContextIsValid_() const
		{
			STORYT_ASSERT((m_pc.HasPropertyWPidAndPtypeOf(types::PidTagType::RecordKey, types::PropertyType::Binary)), "[ERROR]");
			STORYT_ASSERT((m_pc.HasPropertyWPidAndPtypeOf(types::PidTagType::DisplayName, types::PropertyType::String)), "[ERROR]");
			STORYT_ASSERT((m_pc.HasPropertyWPidAndPtypeOf(types::PidTagType::IpmSubTreeEntryId, types::PropertyType::Binary)), "[ERROR]");
			STORYT_ASSERT((m_pc.HasPropertyWPidAndPtypeOf(types::PidTagType::IpmWastebasketEntryId, types::PropertyType::Binary)), "[ERROR]");
			STORYT_ASSERT((m_pc.HasPropertyWPidAndPtypeOf(types::PidTagType::FinderEntryId, types::PropertyType::Binary)), "[ERROR]");
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
			: m_ndb(ndb), m_ltp(ltp), m_store(MessageStore::Init(core::NID_MESSAGE_STORE, ndb)),
			m_rootFolder(Folder::Init(m_store.getEntryID(types::PidTagType::IpmSubTreeEntryId)->nid, ndb)) {}
		
		template<typename FolderID>
		[[nodiscard]] Folder* getFolder(const FolderID& folderID)
		{
			// The Root Folder can return a ptr to itself
			return m_rootFolder.getFolder(folderID);
		}

	private:
		core::Ref<const ltp::LTP> m_ltp;
		core::Ref<const ndb::NDB> m_ndb;
		MessageStore m_store; // MessageStore has to be intiliazed before the Root Folder
		Folder m_rootFolder;
	}; 
} // namespace reader

#endif // READER_MESSAGING_H