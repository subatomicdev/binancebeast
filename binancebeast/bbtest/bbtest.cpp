#include "BinanceBeast.h"
#include <future>


using namespace bblib;


void onExchangeInfo (RestResult&& result)
{
    std::cout << BB_FUNCTION_ENTER << "\n";

    std::cout << result.json.as_object()["rateLimits"].as_array()[0].as_object()["interval"] << "\n";
}


int main (int argc, char ** argv)
{
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
    config.keys.api = "e40fd4783309eed8285e5f00d60e19aa712ce0ecb4d449f015f8702ab1794abf";
    config.keys.secret = "6c3d765d9223d2cdf6fe7a16340721d58689e26d10e6a22903dd76e1d01969f0";

    BinanceBeast bb;

    bb.start(config);

    //bb.ping();
    bb.exchangeInfo(onExchangeInfo);

    cmdFut.wait();

    return 0;
}