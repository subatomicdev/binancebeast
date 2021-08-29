#include <binancebeast/BinanceBeast.h>
#include <mutex>
#include <functional>
#include <condition_variable>


using namespace bblib;


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


    // to notify main thread when we have a reply
    std::condition_variable cvHaveReply;


    BinanceBeast bb;
    
    // start the network processing
    bb.start(config);
  
    // create a new order
    bb.sendRestRequest([&](RestResponse result)
    {
        if (result.hasErrorCode())    
            std::cout << "Error: " << result.failMessage << "\n";
        else
            std::cout << "\nNew Order info:\n" << result.json << "\n";

        cvHaveReply.notify_one();
    },
    "/fapi/v1/order",
    RestSign::HMAC_SHA256,
    RestParams{{{"symbol", "BTCUSDT"}, {"side", "BUY"}, {"type", "MARKET"}, {"quantity", "0.001"}}},
    RequestType::Post);


    std::mutex mux;
    std::unique_lock lck(mux);    
    cvHaveReply.wait(lck);
    

    return 0;
}
