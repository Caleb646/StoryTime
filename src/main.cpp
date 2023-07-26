#include <iostream>
#include "reader.h"

int main()
{
    reader::PSTReader reader("C:\\Users\\caleb\\Coding_Projects\\CPP Projects\\PST File Reader\\data\\Test.pst");
    //reader::PSTReader reader("C:\\Users\\caleb\\Documents\\Outlook Files\\Outlook.pst");
    reader.read();

    reader::msg::Folder* folder = reader.getFolder(std::string("Inbox"));
    return 0;
}