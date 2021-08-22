#include "BinanceBeast.h"
#include <future>


using namespace bblib;


bool handleError(RestResult& result)
{
    if (result.hasErrorCode())
    {
        std::cout << "REST Error: code = " << result.json.as_object()["code"] << "\nreason: " << result.json.as_object()["msg"] << "\n";   
        return true;
    }
    return false;
}

bool handleError(WsResult& result)
{
    if (result.hasErrorCode())
    {
        std::cout << "WS Error: code = " << result.json.as_object()["code"] << "\nreason: " << result.json.as_object()["msg"] << "\n";   
        return true;
    }
    return false;
}


void onExchangeInfo (RestResult result)
{
    std::cout << BB_FUNCTION_ENTER << "\n";

    if (!handleError(result))
    {
        std::cout << result.json.as_object()["rateLimits"].as_array()[0].as_object()["interval"] << "\n";
    }
}


void onServerTime (RestResult result)
{
    std::cout << BB_FUNCTION_ENTER << "\n";

    if (!handleError(result))
    {
        std::cout << result.json.as_object()["serverTime"] << "\n";
    }
}


void onOrderBook (RestResult result)
{
    std::cout << BB_FUNCTION_ENTER << "\n";

    if (!handleError(result))
    {
        std::cout << result.json.as_object()["bids"].as_array() << "\n";
    }
}


void onAllOrders (RestResult result)
{
    std::cout << BB_FUNCTION_ENTER << "\n";

    if (!handleError(result))
    {
        std::cout << result.json.as_array() << "\n";
    }
}


void onMonitorMarkPriceAll(WsResult result)
{
    std::cout << BB_FUNCTION_ENTER << "\n";

    if (!handleError(result))
    {
        std::cout << result.json.as_array() << "\n";
    }
}


void onMonitorMarkPriceSymbol(WsResult result)
{
    std::cout << BB_FUNCTION_ENTER << "\n";

    if (!handleError(result))
    {
        std::cout << result.json.as_object() << "\n";
    }
}


void onMonitorKline(WsResult result)
{
    std::cout << BB_FUNCTION_ENTER << "\n";

    if (!handleError(result))
    {
        std::cout << result.json.as_object() << "\n";
    }
}


void onSymbolMiniTicker(WsResult result)
{
    std::cout << BB_FUNCTION_ENTER << "\n";

    if (!handleError(result))
    {
        std::cout << result.json.as_object() << "\n";
    }
}

void onAllMarketMiniTickers(WsResult result)
{
    std::cout << BB_FUNCTION_ENTER << "\n";

    if (!handleError(result))
    {
        std::cout << result.json.as_array() << "\n";
    }
}

void onIndividualSymbolTicker(WsResult result)
{
    std::cout << BB_FUNCTION_ENTER << "\n";

    if (!handleError(result))
    {
        std::cout << result.json.as_object() << "\n";
    }
}

void onSymbolBookTicker(WsResult result)
{
    std::cout << BB_FUNCTION_ENTER << "\n";

    if (!handleError(result))
    {
        std::cout << result.json.as_object() << "\n";
    }
}


void onAllBookTickers(WsResult result)
{
    std::cout << BB_FUNCTION_ENTER << "\n";

    if (!handleError(result))
    {
        std::cout << result.json.as_object() << "\n";
    }
}


int main (int argc, char ** argv)
{
    auto cmdFut = std::async(std::launch::async, []
    {
        bool done = false;
        while (!done)
        {
            std::cout << ">\n";
            std::string s;
            std::getline(std::cin, s);
            done =  (s == "stop" || s == "exit");
        }
    });

    auto config = ConnectionConfig::MakeTestNetConfig();
    config.keys.api     = "e40fd4783309eed8285e5f00d60e19aa712ce0ecb4d449f015f8702ab1794abf";
    config.keys.secret  = "6c3d765d9223d2cdf6fe7a16340721d58689e26d10e6a22903dd76e1d01969f0";

    BinanceBeast bb;

    bb.start(config);
    
    //bb.exchangeInfo(onExchangeInfo);
    //bb.serverTime(onServerTime);
    //bb.orderBook(onOrderBook, RestParams {RestParams::QueryParams {{"symbol", "BTCUSDT"}}});
    //bb.allOrders(onAllOrders, RestParams {RestParams::QueryParams {{"symbol", "BTCUSDT"}}});

    //bb.monitorMarkPrice(onMonitorMarkPriceAll, "!markPrice@arr@1s");
    //bb.monitorMarkPrice(onMonitorMarkPriceSymbol, "btcusdt@markPrice@1s");
    //bb.monitorMarkPrice(onMonitorMarkPriceSymbol, "ethusdt@markPrice@1s");
    //bb.monitorKline(onMonitorKline, "btcusdt@kline_15m");
    //bb.monitorIndividualSymbolMiniTicker(onSymbolMiniTicker, "btcusdt");
    //bb.monitorAllMarketMiniTickers(onAllMarketMiniTickers);
    //bb.monitorIndividualSymbolTicker(onIndividualSymbolTicker, "btcusdt");
    //bb.monitorSymbolBookTicker(onSymbolBookTicker, "btcusdt");
    bb.monitorAllBookTicker(onAllBookTickers);

    cmdFut.wait();

    return 0;
}
