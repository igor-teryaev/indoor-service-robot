#include <iostream>
#include <string>

#include "stm32_client_runner.h"

int main(
    int argc,
    char* argv[])
{
    if (argc != 2)
    {
        std::cerr
            << "Usage: stm32_link_client <serial-device>"
            << std::endl;

        return 1;
    }

    Stm32ClientRunner runner{
        std::string(argv[1])
    };

    return runner.run();
}