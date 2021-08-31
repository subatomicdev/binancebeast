#include <binancebeast/BinanceBeast.h>
#include "testcommon.h"
#include <future>
#include <chrono>
#include <condition_variable>


using namespace bblib;
using namespace bblib_test;


bool showResponseData = false, dataError = false;
std::condition_variable cvHaveReply;
string_view test;


void onWsResponse(WsResponse result)
{
    if (showResponseData)
        std::cout << "\n" << result.json << "\n";

    dataError = bblib_test::hasError(test, result);  

    cvHaveReply.notify_all(); 
}



void runTest(BinanceBeast& bb, string currentTest, string stream, const bool showData = false, const bool alwaysExpectResponse = true)
{
    showResponseData = showData;
    test = currentTest;

    auto token = bb.startWebSocket(onWsResponse, stream);

    auto haveReply = waitReply(cvHaveReply, test);
    
    
    if (alwaysExpectResponse)
        std::cout << "Test: " << currentTest << " : " << (!dataError && haveReply ? "PASS" : "FAIL") << "\n";
    else
        std::cout << "Test: " << currentTest << " : " << (!dataError ? "PASS" : "FAIL") << "\n";
}


int main (int argc, char ** argv)
{
    std::cout << "\n\nTest WebSockets API\n\n";

    if (argc != 2)
    {   
        std::cout << "Usage, requires key file:\n"
                  << argv[0] << " <full path to keyfile>\n";
        return 1;
    }
    
    auto config = ConnectionConfig::MakeTestNetConfig(Market::USDM, std::filesystem::path{argv[1]});
    
    BinanceBeast bb;
    bb.start(config);
    
    runTest(bb, "aggregrateTrade", "btcusdt@aggTrade");
    runTest(bb, "markPrice", "btcusdt@markPrice@1s");
    runTest(bb, "markPriceForAll", "!markPrice@arr@1s");
    runTest(bb, "klines", "btcusdt@kline_15m");
    runTest(bb, "continuousContractKline", "btcusdt_perpetual@continuousKline_1m");
    runTest(bb, "individualSymbolMiniTicker", "btcusdt@miniTicker");
    runTest(bb, "allMarketMiniTicker", "!miniTicker@arr");
    runTest(bb, "individualSymbolTicker", "btcusdt@ticker");
    runTest(bb, "allMarketTicker", "!ticker@arr");
    runTest(bb, "individualSymbolBookTicker", "btcusdt@bookTicker");
    runTest(bb, "allBookTicker", "!bookTicker");
    runTest(bb, "liquidationOrder", "btcusdt@forceOrder", false, false); 
    runTest(bb, "allMarketLiquidationOrder", "!forceOrder@arr", false, false); 
    runTest(bb, "partialBookDepth", "btcusdt@depth5@100ms"); 
    runTest(bb, "diffBookDepth", "btcusdt@depth@100ms"); 
    runTest(bb, "compositeIndexSymbolInfo", "defiusdt@compositeIndex");     


    // test disconnect only
    auto token = bb.startWebSocket([](WsResponse result)
    {
        if (showResponseData)
            std::cout << "\n" << result.json << "\n";

    }, "!miniTicker@arr");


    std::this_thread::sleep_for(2s);

    std::condition_variable cvDisconnect;
    bool disconnectFail = false;
    bb.stopWebSocket(token, [&](WsResponse result)
    {
        if (result.state == WsResponse::State::Disconnect)
        {
            disconnectFail = result.hasErrorCode();

            if (showResponseData)
                std::cout << "\n" << (result.hasErrorCode() ? result.failMessage : "\nDisconnected\n");
        }

        cvDisconnect.notify_one();
    });

    std::mutex mux;
    std::unique_lock lck(mux);    

    if (cvDisconnect.wait_for(lck, 5s)  == std::cv_status::timeout)
        std::cout << "Test: Disconnect : FAIL : timeout\n";
    else if (disconnectFail)
        std::cout << "Test: Disconnect : FAIL\n";
    else
        std::cout << "Test: Disconnect : PASS\n";
}
