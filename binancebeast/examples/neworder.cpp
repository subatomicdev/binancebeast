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
  
    
    // Single Order

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
    {
        std::unique_lock lck(mux);    
        cvHaveReply.wait(lck);
    }
    
    

    // Batch Order
    // This is contents of a single order inside "batchOrder" JSON array, URL encoded.
    // See the GitHub for more info.

    // create a batch order (max of 5)
    // the safest way is to use boost::json to create the json then serialise
    boost::json::array order =
    {
        {{"symbol", "BTCUSDT"}, {"side", "BUY"}, {"type", "MARKET"}, {"quantity", "0.001"}},
        {{"symbol", "BTCUSDT"}, {"side", "BUY"}, {"type", "MARKET"}, {"quantity", "0.001"}}
    };

    bb.sendRestRequest([&](RestResponse result)
    {
        if (result.hasErrorCode())    
            std::cout << "Error: " << result.failMessage << "\n";
        else
            std::cout << "\nNew Order info:\n" << result.json << "\n";

        cvHaveReply.notify_one();
    },
    "/fapi/v1/batchOrders",
    RestSign::HMAC_SHA256,
    RestParams{{{"batchOrders", BinanceBeast::urlEncode(json::serialize(order))}}},
    RequestType::Post);

    
    std::unique_lock batchlck(mux);    
    cvHaveReply.wait(batchlck);

    return 0;
}
