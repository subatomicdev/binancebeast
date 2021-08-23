#include <binancebeast/BinanceBeast.h>
#include "testcommon.h"
#include <future>
#include <chrono>
#include <condition_variable>



using namespace bblib;


int main (int argc, char ** argv)
{
    std::cout << "\n\nTest REST API\n\n";

        BinanceBeast bb;

        auto config = ConnectionConfig::MakeTestNetConfig();
        bb.start(config);


        std::condition_variable cvHaveReply;

        bb.exchangeInfo([&cvHaveReply](RestResult result)
        {
            if (!bblib_test::handleError(result))
            {
                std::cout << result.json.as_object() << "\n";
            }

            cvHaveReply.notify_one();
        });

        
        // wait for REST reply
        std::mutex mux;
        std::unique_lock lck(mux);
        cvHaveReply.wait(lck);


        std::cout << "\n\nTest Websockets API\n\n";

        bb.monitorMarkPrice([](WsResult result)
        {
            if (!bblib_test::handleError(result))
            {
                std::cout << result.json << "\n";
            }

        }, "btcusdt@markPrice@1s");
        
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(5s);

    return 0;
}