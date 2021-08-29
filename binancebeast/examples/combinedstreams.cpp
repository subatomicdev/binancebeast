#include <binancebeast/BinanceBeast.h>
#include <mutex>
#include <functional>
#include <condition_variable>
#include <chrono>


using namespace bblib;
using namespace std::chrono_literals;


/// Run a combined mark price stream for 10 seconds.
int main (int argc, char ** argv)
{
    if (argc != 2 && argc != 3)
    {   
        std::cout << "Usage, requires key file or keys:\n"
                  << "For key file: " << argv[0] << " <full path to keyfile>\n"
                  << "For keys: " << argv[0] << " <api key> <secret key>\n";
        return 1;
    }

    ConnectionConfig config;

    if (argc == 2)
        config = ConnectionConfig::MakeTestNetConfig(Market::USDM, std::filesystem::path{argv[1]});
    else if (argc == 3)
        config = ConnectionConfig::MakeTestNetConfig(Market::USDM, argv[1], argv[2]);


    BinanceBeast bb;
   
    bb.start(config);
    
    // each stream's data is pushed separately.
    // in this case, we have two combined streams, the handler will be called once for btcusdt and again for ethusdt.
    // the "stream" value contains the stream name
    bb.startWebSocket([](WsResponse result)
    {
        if (result.hasErrorCode())
            std::cout << "Error: " << result.failMessage << "\n";
        else
        {
            auto& object = result.json.as_object();
            auto& streamName = object["stream"];

            if (streamName == "btcusdt@markPrice@1s")
            {
                std::cout << "Mark price for BTCUSDT:\n" << object["data"] << "\n";
            }
            else if (streamName == "ethusdt@markPrice@1s")
            {
                std::cout << "Mark price for ETHUSDT:\n" << object["data"] << "\n";
            }
        }   
    },
    {{"btcusdt@markPrice@1s"}, {"ethusdt@markPrice@1s"}});

    std::this_thread::sleep_for(10s);

    return 0;
}