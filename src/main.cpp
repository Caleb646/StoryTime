#include <iostream>
#include "reader.h"

int main()
{
    reader::PSTReader reader("C:\\Users\\caleb\\Coding_Projects\\CPP Projects\\PST File Reader\\data\\Test.pst");
    //reader::PSTReader reader("C:\\Users\\caleb\\Documents\\Outlook Files\\Outlook.pst");
    reader.read();

    reader::Folder* folder = reader.getFolder(std::string("Inbox"));
    std::vector<reader::MessageObject> messages = folder->getNMessages(0, 50);

    for (auto& msg : messages)
    {
        STORYT_INFO("Subject: {}\nFrom: {}\nHas Attachments: {}\n", msg.getSubject(), msg.getSender(), msg.hasAttachments());

        for (auto& recip : msg.getRecipients())
        {
            std::cout << recip << "\n";
        }
        std::cout << "\n";
    }
    return 0;
}