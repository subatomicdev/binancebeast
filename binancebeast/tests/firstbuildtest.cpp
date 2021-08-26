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

    bb.sendRestRequest([&cvHaveReply](RestResult result)
    {
        if (result.hasErrorCode())
            std::cout << "FAIL: " << result.failMessage << "\n";
        else
            std::cout << "OK:\n" << result.json << "\n";

        cvHaveReply.notify_one();

    }, "/fapi/v1/exchangeInfo", RestSign::Unsigned);

    waitReply(cvHaveReply, "exchangeInfo");


    return 0;
}
