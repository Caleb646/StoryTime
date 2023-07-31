# StoryTime: Microsoft Outlook PST File Reader
StoryTime is an open-source C++ header only library that provides a powerful library for reading Microsoft Outlook PST files, including messages and attachments. This library is designed to offer developers an easy-to-use interface for extracting valuable information from PST files, which are used by Microsoft Outlook to store email messages, contacts, calendar data, and other items.
# Features
* Efficiently reads Microsoft Outlook PST files.
* Extracts email messages, including metadata (subject, sender, recipients, timestamp, etc.).
* Retrieves message attachments with support for various file formats.
* Allows seamless integration into your C++ projects.
# Installation
## Copying Include Directory
Simply copy the include folder and add it to your include path.
## Using CMake
```CMake
  include(FetchContent)
  FetchContent_Declare(
    storyt
    GIT_REPOSITORY url
    GIT_TAG tag
  )
  FetchContent_MakeAvailable(storyt)
```
# Usage
```C++
#include <vector>
#include <iostream>
#include "pst_reader.h"

int main()
{
    // Initilize the PSTReader
    storyt::PSTReader reader("Path to PST File");
    // Pulls just the folders into memory not any messages.
    reader.read();
    // Returns the folder labeled "Inbox"
    storyt::Folder* folder = reader.getFolder(std::string("Inbox"));
    
    std::cout << folder->nMessages() << "\n";
    // Reads 50 messages at a time from the Inbox Folder
    const size_t batchSize = 50;
    for (size_t i = 0; i < folder->nMessages(); i += batchSize)
    {
        std::vector<storyt::MessageObject> messages = folder->getNMessages(i, i+batchSize);
        // Each message has several fields that can be accessed using the methods shown below.
        for (auto& msg : messages)
        {
            std::string subject = msg.getSubject();
            std::string sender = msg.getSender();
            std::string body = msg.getBody();
            std::vector<std::string> recipients = msg.getRecipients();
        }
    }
    return 0;
}
```
