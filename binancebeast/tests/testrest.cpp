#include <binancebeast/BinanceBeast.h>
#include "testcommon.h"
#include <future>
#include <chrono>
#include <condition_variable>



using namespace bblib;
using namespace bblib_test;

using RestFunction = void (BinanceBeast::*)(RestCallback);
using RestAndParamsFunction = void (BinanceBeast::*)(RestCallback, RestParams);



void runTest(BinanceBeast& bb, RestFunction rf, std::string_view test, const bool showData = false)
{
    std::condition_variable cvHaveReply;
    bool dataError = false;

    auto handler = [&](RestResult result)
    {
        if (showData)
            std::cout << "\n" << result.json << "\n";

        dataError = bblib_test::hasError(test, result);

        cvHaveReply.notify_one();
    };

    std::cout << "Test: " << test << " : ";

    auto f = std::mem_fn(rf);
    
    f(bb, handler);

    auto haveReply = waitReply(cvHaveReply, test);
    
    std::cout << (!dataError && haveReply ? "PASS" : "FAIL") << "\n";
}


void runTest(BinanceBeast& bb, RestAndParamsFunction rf, RestParams params, std::string_view test, const bool showData = false)
{
    std::condition_variable cvHaveReply;
    bool dataError = false;

    auto handler = [&](RestResult result)
    {
        if (showData)
            std::cout << "\n" << result.json << "\n";

        dataError = bblib_test::hasError(test, result);

        cvHaveReply.notify_one();
    };

    std::cout << "Test: " << test << " : ";

    auto f = std::mem_fn(rf);
    
    f(bb, handler, params);

    auto haveReply = waitReply(cvHaveReply, test);
    
    std::cout << (!dataError && haveReply ? "PASS" : "FAIL") << "\n";
}




int main (int argc, char ** argv)
{
    std::cout << "\n\nTest REST API\n\n";

    BinanceBeast bb;

    auto config = ConnectionConfig::MakeTestNetConfig();
    config.keys.api     = "e40fd4783309eed8285e5f00d60e19aa712ce0ecb4d449f015f8702ab1794abf";
    config.keys.secret  = "6c3d765d9223d2cdf6fe7a16340721d58689e26d10e6a22903dd76e1d01969f0";

    bb.start(config);
    

    
    runTest(bb, &BinanceBeast::exchangeInfo, "exchangeInfo");

    runTest(bb, &BinanceBeast::serverTime, "serverTime");

    runTest(bb, &BinanceBeast::orderBook, RestParams {RestParams::QueryParams {{"symbol", "BTCUSDT"}}}, "orderBook");

    runTest(bb, &BinanceBeast::allOrders, RestParams {RestParams::QueryParams {{"symbol", "BTCUSDT"}}}, "allOrders");
    
    runTest(bb, &BinanceBeast::recentTradesList, RestParams {RestParams::QueryParams {{"symbol", "BTCUSDT"}}}, "recentTradesList");
    
    runTest(bb, &BinanceBeast::historicTrades, RestParams {RestParams::QueryParams {{"symbol", "BTCUSDT"}}}, "historicTrades");

    runTest(bb, &BinanceBeast::aggregateTradesList, RestParams {RestParams::QueryParams {{"symbol", "BTCUSDT"}}}, "aggregateTradesList");

    runTest(bb, &BinanceBeast::klines, RestParams {RestParams::QueryParams {{"symbol", "BTCUSDT"}, {"interval","15m"}}}, "klines");
    
    runTest(bb, &BinanceBeast::contractKlines, RestParams {RestParams::QueryParams {{"pair", "BTCUSDT"}, {"interval","15m"}, {"contractType","PERPETUAL"}}}, "contract klines");

    runTest(bb, &BinanceBeast::indexPriceKlines, RestParams {RestParams::QueryParams {{"pair", "BTCUSDT"}, {"interval","15m"}}}, "index price klines");
    
    runTest(bb, &BinanceBeast::markPriceKlines, RestParams {RestParams::QueryParams {{"symbol", "BTCUSDT"}, {"interval","15m"}}}, "mark price klines");
    
    runTest(bb, &BinanceBeast::markPrice, RestParams {RestParams::QueryParams {{"symbol", "BTCUSDT"}}}, "mark price");
    
}
