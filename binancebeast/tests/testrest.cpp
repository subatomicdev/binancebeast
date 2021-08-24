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

    auto config = ConnectionConfig::MakeTestNetConfig(argc == 2 ? argv[1] : "");

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

    runTest(bb, &BinanceBeast::fundingRate, RestParams {RestParams::QueryParams {{"symbol", "BTCUSDT"}}}, "fundingRate");

    runTest(bb, &BinanceBeast::tickerPriceChange24hr, RestParams {RestParams::QueryParams {{"symbol", "BTCUSDT"}}}, "24hrTickerPriceChange");
    
    runTest(bb, &BinanceBeast::symbolPriceTicker, RestParams {RestParams::QueryParams {{"symbol", "BTCUSDT"}}}, "symbolPriceTicker");

    runTest(bb, &BinanceBeast::symbolBookTicker, RestParams {RestParams::QueryParams {{"symbol", "BTCUSDT"}}}, "symbolBookTicker");
        
    runTest(bb, &BinanceBeast::openInterest, RestParams {RestParams::QueryParams {{"symbol", "BTCUSDT"}}}, "openInterest");

    runTest(bb, &BinanceBeast::openInterestStats, RestParams {RestParams::QueryParams {{"symbol", "BTCUSDT"}, {"period", "15m"}}}, "openInterestStats");

    runTest(bb, &BinanceBeast::topTraderLongShortRatioAccounts, RestParams {RestParams::QueryParams {{"symbol", "BTCUSDT"}, {"period", "15m"}}}, "topTraderLongShortRatioAccounts");

    runTest(bb, &BinanceBeast::topTraderLongShortRatioPositions, RestParams {RestParams::QueryParams {{"symbol", "BTCUSDT"}, {"period", "15m"}}}, "topTraderLongShortRatioPositions");
    
    runTest(bb, &BinanceBeast::longShortRatio, RestParams {RestParams::QueryParams {{"symbol", "BTCUSDT"}, {"period", "15m"}}}, "longShortRatio");

    runTest(bb, &BinanceBeast::takerBuySellVolume, RestParams {RestParams::QueryParams {{"symbol", "BTCUSDT"}, {"period", "15m"}}}, "takerBuySellVolume");
    
    runTest(bb, &BinanceBeast::historicalBlvtNavKlines, RestParams {RestParams::QueryParams {{"symbol", "BTCUSDT"}, {"period", "15m"}}}, "historicalBlvtNavKlines");

    runTest(bb, &BinanceBeast::compositeIndexSymbolInfo, RestParams {RestParams::QueryParams {{"symbol", "BTCUSDT"}}}, "compositeIndexSymbolInfo");
}