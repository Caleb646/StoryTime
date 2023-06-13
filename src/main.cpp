#include <iostream>
#include "reader.h"

int main()
{
    //PSTReader reader{ "" };
    //std::cout << "Hello" << std::endl;

    //reader::log("[TEST] %0.2f %0.2f %0.2f", 1.0f, 1.0f, 1.0f);
    reader::PSTReader reader("C:\\Users\\caleb\\Coding_Projects\\CPP Projects\\PST File Reader\\data\\Test.pst");
    reader.read();
    return 0;
}