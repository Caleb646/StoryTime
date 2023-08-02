#include <vector>
#include <array>
#include <string>
#include <iostream>


#include <storyt/pst/pst_reader.h>
#include <storyt/pdf/compression.h>


int main()
{
    //reader::PSTReader reader("C:\\Users\\caleb\\Coding_Projects\\CPP Projects\\PST File Reader\\data\\Test.pst");
    //storyt::PSTReader reader("C:\\Users\\caleb\\Documents\\Outlook Files\\Outlook.pst");
    //reader.read();

    //storyt::Folder* folder = reader.getFolder(std::string("Inbox"));
    //std::array<Bytef, 32> compressed = storyt::_internal::deflate();
    //std::string com(compressed.begin(), compressed.end());
    //std::cout << com << "\n";

    //std::array<Bytef, 32> data = storyt::_internal::inflate(compressed);
    std::string data("Compressed");
    std::string str = storyt::_internal::compressString(data);
    std::cout << str << "\n";

    //std::array<Bytef, 19> data = { 0x78, 0x01, 0x63, 0x62, 0x80, 0x00, 0x66, 0x20, 0xc5, 0x08, 0xc4, 0x20, 0x1a, 0x04, 0x00, 0x00, 0x9c, 0x00, 0x0a };
    //std::string str(data.begin(), data.end());
    std::string ret = storyt::_internal::decompressString(str);
    std::cout << ret << "\n";


    //std::array<Bytef, 25> tocompress = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    //    0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    
    //std::cout << folder->nMessages() << "\n";

    //const size_t batchSize = 50;
    //for (size_t i = 0; i < folder->nMessages(); i += batchSize)
    //{
    //    std::vector<storyt::MessageObject> messages = folder->getNMessages(i, i+batchSize);
    //    for (auto& msg : messages)
    //    {
    //        std::string subject = msg.getSubject();
    //        std::string sender = msg.getSender();
    //        std::string body = msg.getBody();
    //        std::vector<std::string> recipients = msg.getRecipients();
    //    }
    //}
    return 0;
}