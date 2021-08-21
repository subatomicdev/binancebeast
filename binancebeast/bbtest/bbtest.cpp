#include "BinanceBeast.h"
#include <future>

int main (int argc, char ** argv)
{
    using namespace bblib;

    auto cmdFut = std::async(std::launch::async, []
    {
        
        bool done = false;
        while (!done)
        {
            std::cout << ">\n";
            std::string s;
            std::getline(std::cin, s);
            done =  (s == "stop" || s == "exit");
        }
    });

    auto config = ConnectionConfig::MakeTestNetConfig();

    BinanceBeast bb;

    bb.start(config);

    bb.ping();
    bb.exchangeInfo();

    cmdFut.wait();

    return 0;
}