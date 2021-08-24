#include <binancebeast/BinanceBeast.h>
#include "testcommon.h"
#include <future>
#include <chrono>
#include <condition_variable>



using namespace bblib;
using namespace bblib_test;

int main (int argc, char ** argv)
{
    std::cout << "\n\nFirst Build Test\n\n";

    BinanceBeast bb;

    auto config = ConnectionConfig::MakeTestNetConfig();
    bb.start(config);


    std::condition_variable cvHaveReply;

    bb.exchangeInfo([&cvHaveReply](RestResult result)
    {
        if (!bblib_test::handleError(BB_FUNCTION, result))
        {
            std::cout << result.json.as_object() << "\n";
        }

        cvHaveReply.notify_one();
    });

    waitReply(cvHaveReply, "exchangeInfo");


    return 0;
}
