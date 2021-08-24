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
        std::cout << result.json.as_object() << "\n";

        std::cout << "success = " << !bblib_test::hasError(BB_FUNCTION, result);

        cvHaveReply.notify_one();
    });

    waitReply(cvHaveReply, "exchangeInfo");


    return 0;
}
