#include <binancebeast/BinanceBeast.h>
#include "testcommon.h"
#include <future>
#include <chrono>
#include <condition_variable>



using namespace bblib;
using namespace bblib_test;

using RestFunction = void (BinanceBeast::*)(RestResponseHandler);
using RestAndParamsFunction = void (BinanceBeast::*)(RestResponseHandler, RestParams);



void runTest(BinanceBeast& bb, const string& path, RestParams params, RestSign sign, const bool showData = false)
{
    std::condition_variable cvHaveReply;
    bool dataError = false;

    auto handler = [&](RestResult result)
    {
        if (showData)
            std::cout << "\n" << result.json << "\n";

        dataError = bblib_test::hasError(path, result);

        cvHaveReply.notify_one();
    };

    std::cout << "Test: " << path << " : ";

    bb.sendRestRequest(handler, path, sign, params);

    auto haveReply = waitReply(cvHaveReply, path);
    
    std::cout << (!dataError && haveReply ? "PASS" : "FAIL") << "\n";
}





int main (int argc, char ** argv)
{
    std::cout << "\n\nTest REST API\n\n";

    if (argc != 2)
    {   
        std::cout << "Usage, requires key file or keys:\n"
                  << argv[0] << " <full path to keyfile>\n";
        return 1;
    }

    auto config = ConnectionConfig::MakeTestNetConfig(std::filesystem::path{argv[1]});

    BinanceBeast bb;
    bb.start(config);
    
    // market
    runTest(bb, "/fapi/v1/exchangeInfo", RestParams{}, RestSign::Unsigned, false);
    runTest(bb, "/fapi/v1/time", RestParams{}, RestSign::Unsigned, false);
    runTest(bb, "/fapi/v1/depth", RestParams{{{"symbol", "BTCUSDT"}}}, RestSign::Unsigned, false);
    runTest(bb, "/fapi/v1/trades", RestParams{{{"symbol", "BTCUSDT"}}}, RestSign::Unsigned, false);
    runTest(bb, "/fapi/v1/historicalTrades", RestParams{{{"symbol", "BTCUSDT"}}}, RestSign::Unsigned, false);
    runTest(bb, "/fapi/v1/aggTrades", RestParams{{{"symbol", "BTCUSDT"}}}, RestSign::Unsigned, false);
    runTest(bb, "/fapi/v1/klines", RestParams{{{"symbol", "BTCUSDT"}, {"interval","15m"}}}, RestSign::Unsigned, false);
    runTest(bb, "/fapi/v1/continuousKlines", RestParams{{{"pair", "BTCUSDT"}, {"interval","15m"}, {"contractType","PERPETUAL"}}}, RestSign::Unsigned, false);
    runTest(bb, "/fapi/v1/indexPriceKlines", RestParams{{{"pair", "BTCUSDT"}, {"interval","15m"}}}, RestSign::Unsigned, false);
    runTest(bb, "/fapi/v1/markPriceKlines", RestParams{{{"symbol", "BTCUSDT"}, {"interval","15m"}}}, RestSign::Unsigned, false);
    runTest(bb, "/fapi/v1/premiumIndex", RestParams{{{"symbol", "BTCUSDT"}}}, RestSign::Unsigned, false);
    runTest(bb, "/fapi/v1/ticker/24hr", RestParams{{{"symbol", "BTCUSDT"}}}, RestSign::Unsigned, false);
    runTest(bb, "/fapi/v1/ticker/price", RestParams{{{"symbol", "BTCUSDT"}}}, RestSign::Unsigned, false);
    runTest(bb, "/fapi/v1/ticker/bookTicker", RestParams{{{"symbol", "BTCUSDT"}}}, RestSign::Unsigned, false);
    runTest(bb, "/fapi/v1/openInterest", RestParams{{{"symbol", "BTCUSDT"}}}, RestSign::Unsigned, false);
    runTest(bb, "/fapi/v1/indexInfo", RestParams{{{"symbol", "DEFIUSDT"}}}, RestSign::Unsigned, false);
    
    // TODO always an invalid symbol
    //runTest(bb, "/fapi/v1/lvtKlines", RestParams{{{"symbol", "BTCUSDT"}, {"interval","15m"}}}, RestSign::Unsigned, false);

    // TODO these all timeout
    //runTest(bb, "/fapi/v1/fundingRate", RestParams{{{"symbol", "BTCUSDT"}}}, RestSign::Unsigned, false);
    //runTest(bb, "/fapi/v1/openInterestHist", RestParams{{{"symbol", "BTCUSDT"}, {"period", "15m"}}}, RestSign::Unsigned, false);
    //runTest(bb, "/futures/data/topLongShortAccountRatio", RestParams{{{"symbol", "BTCUSDT"}, {"period", "15m"}}}, RestSign::Unsigned, false);
    //runTest(bb, "/futures/data/topLongShortPositionRatio", RestParams{{{"symbol", "BTCUSDT"}, {"period", "15m"}}}, RestSign::Unsigned, false);
    //runTest(bb, "/futures/data/globalLongShortAccountRatio", RestParams{{{"symbol", "BTCUSDT"}, {"period", "15m"}}}, RestSign::Unsigned, false);
    //runTest(bb, "/futures/data/longshortRatio", RestParams{{{"symbol", "BTCUSDT"}, {"period", "15m"}}}, RestSign::Unsigned, false);
    //runTest(bb, "/futures/data/takerlongshortRatio", RestParams{{{"symbol", "BTCUSDT"}, {"period", "15m"}}}, RestSign::Unsigned, false);


    // account/trades
    runTest(bb, "/fapi/v1/allOrders", RestParams{{{"symbol", "BTCUSDT"}}}, RestSign::HMAC_SHA256, false);    
    runTest(bb, "/fapi/v1/multiAssetsMargin", RestParams{}, RestSign::HMAC_SHA256, false);
    
    // TODO add more tests
}