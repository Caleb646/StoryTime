#include <iostream>
#include "reader.h"

int main()
{
    //reader::PSTReader reader("C:\\Users\\caleb\\Coding_Projects\\CPP Projects\\PST File Reader\\data\\Test.pst");
    storyt::PSTReader reader("C:\\Users\\caleb\\Documents\\Outlook Files\\Outlook.pst");
    reader.read();

    storyt::Folder* folder = reader.getFolder(std::string("Inbox"));
    
    std::cout << folder->nMessages() << "\n";

    const size_t batchSize = 50;
    for (size_t i = 0; i < folder->nMessages(); i += batchSize)
    {
        std::vector<storyt::MessageObject> messages = folder->getNMessages(i, i+batchSize);
        for (auto& msg : messages)
        {
            std::string subject = msg.getSubject();
            std::string sender = msg.getSender();
            std::string body = msg.getBody();
            std::vector<std::string> recipients = msg.getRecipients();
        }
    }

    //std::vector<reader::MessageObject> messages = folder->getNMessages(0, 50);
    //for (auto& msg : messages)
    //{
    //    STORYT_INFO("Subject: {}\nFrom: {}\nHas Attachments: {}\n", msg.getSubject(), msg.getSender(), msg.hasAttachments());

    //    for (auto& recip : msg.getRecipients())
    //    {
    //        std::cout << recip << "\n";
    //    }
    //    std::cout << "\n";
    //}
    return 0;
}