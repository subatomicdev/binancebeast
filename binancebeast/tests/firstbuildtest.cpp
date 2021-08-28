#include <binancebeast/BinanceBeast.h>
#include "testcommon.h"
#include <future>
#include <chrono>
#include <condition_variable>



using namespace bblib;
using namespace bblib_test;
using namespace std::chrono_literals;


int main (int argc, char ** argv)
{
    std::cout << "\n\nFirst Build Test\n\n";

    BinanceBeast bb;

    auto config = ConnectionConfig::MakeTestNetConfig();
    bb.start(config);

    std::condition_variable cvHaveReply;

    std::cout << "\n\nREST: exchangeInfo\n\n";

    bb.sendRestRequest([&cvHaveReply](RestResponse result)
    {
        if (result.hasErrorCode())
            std::cout << "FAIL: " << result.failMessage << "\n";
        else
            std::cout << "OK:\n" << result.json << "\n";

        cvHaveReply.notify_one();

    }, "/fapi/v1/exchangeInfo", RestSign::Unsigned, RestParams{}, RequestType::Get);

    waitReply(cvHaveReply, "exchangeInfo");


    std::cout << "\n\nWebSockets: bookTicker\n\n";
    bb.startWebSocket([](WsResponse result)
    {
        if (result.hasErrorCode())
            std::cout << "ERROR: " << result.failMessage << "\n";
        else
            std::cout << result.json << "\n";
    }, "!bookTicker");

    std::this_thread::sleep_for(8s);

    return 0;
}
