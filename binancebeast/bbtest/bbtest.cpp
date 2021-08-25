#include <binancebeast/BinanceBeast.h>
#include <future>
#include <thread>
#include <chrono>


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
        std::cout << result.json.as_object() << "\n";
    }
}


void onServerTime (RestResult result)
{
    std::cout << BB_FUNCTION_ENTER << "\n";

    if (!handleError(result))
    {
        std::cout << result.json.as_object() << "\n";
    }
}


void onOrderBook (RestResult result)
{
    std::cout << BB_FUNCTION_ENTER << "\n";

    if (!handleError(result))
    {
        std::cout << result.json.as_object() << "\n";
    }
}


void onAllOrders (RestResult result)
{
    std::cout << BB_FUNCTION_ENTER << "\n";

    if (!handleError(result))
    {
        std::cout << result.json << "\n";
    }
}


void onMonitorMarkPriceAll(WsResult result)
{
    std::cout << BB_FUNCTION_ENTER << "\n";

    if (!handleError(result))
    {
        std::cout << result.json << "\n";
    }
}


void onMonitorMarkPriceSymbol(WsResult result)
{
    std::cout << BB_FUNCTION_ENTER << "\n";

    if (!handleError(result))
    {
        std::cout << result.json << "\n";
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


void onLiquidationOrder(WsResult result)
{
    std::cout << BB_FUNCTION_ENTER << "\n";

    if (!handleError(result))
    {
        std::cout << result.json << "\n";
    }
}


void onBookDepth(WsResult result)
{
    std::cout << BB_FUNCTION_ENTER << "\n";

    if (!handleError(result))
    {
        std::cout << result.json << "\n";
    }
}


void onBltv(WsResult result)
{
    std::cout << BB_FUNCTION_ENTER << "\n";

    if (!handleError(result))
    {
        std::cout << result.json << "\n";
    }
}


void onUserData(WsResult result)
{
    std::cout << BB_FUNCTION_ENTER << "\n";
    
    if (result.hasErrorCode())
    {
        std::cout << result.failMessage << "\n";
    }
    else
    {
        auto topLevel = result.json.as_object();
        const auto eventType = json::value_to<string>(topLevel["e"]);

        if (eventType == "listenKeyExpired")
        {
            std::cout << "listen key expired, renew with BinanceBeast::renewListenKey()\n";
        }
        else if (eventType == "MARGIN_CALL")
        {
            std::cout << "margin call\n";
        }
        else if (eventType == "ACCOUNT_UPDATE")
        {
            std::cout << "account update\n";
        }
        else if (eventType == "ORDER_TRADE_UPDATE")
        {
            std::cout << "order trade update\n";
        }
        else if (eventType == "ACCOUNT_CONFIG_UPDATE")
        {
            std::cout << "account config update\n";
        }
    }
}


void onRenewListenKey(WsResult result)
{
    std::cout << BB_FUNCTION_ENTER << "\n";

    std::cout << "\nListen key renewal/extend success(true) or fail(false): " << std::boolalpha << (result.state == WsResult::State::Success) << "\n";
}


void onCloseUserData(WsResult result)
{
    std::cout << BB_FUNCTION_ENTER << "\n";

    std::cout << "\nUser data close success(true) or fail(false): " << std::boolalpha << (result.state == WsResult::State::Success) << "\n";
}



class ListenKeyExtender
{
public:
    ListenKeyExtender(BinanceBeast& bb) : m_bb(bb)
    {
        m_guard = std::make_unique<boost::asio::executor_work_guard<net::io_context::executor_type>> (m_ioc.get_executor());

        m_timer = std::make_unique<boost::asio::steady_timer> (m_ioc, boost::asio::chrono::minutes(59));
        m_timer->async_wait(std::bind(&ListenKeyExtender::onTimerExpire, this, std::placeholders::_1));

        m_thread = std::move(std::thread([this]{m_ioc.run();}));
    }

    ~ListenKeyExtender()
    {
        m_timer->cancel();
        m_guard.reset();
        m_ioc.stop();
        m_thread.join();
    }

    void onTimerExpire(const boost::system::error_code)
    {
        std::cout << "Sending listen key extend request\n";
        m_bb.renewListenKey(std::bind(&ListenKeyExtender::onListenKeyRenew, this, std::placeholders::_1));
    }

    void onListenKeyRenew(WsResult result)
    {
        // call with 'true': the call to renew the listen key returns empty (null) json
        if (result.hasErrorCode(true))
        {
            std::cout << "Listen key renewal/extend fail: " << result.json << "\n";
        }
        else
        {
            std::cout << "Listen key renewal/extend success\n";
        }   

        m_timer->expires_after(boost::asio::chrono::minutes(59));    
        m_timer->async_wait(std::bind(&ListenKeyExtender::onTimerExpire, this, std::placeholders::_1));         
    }

    BinanceBeast& m_bb;
    boost::asio::io_context m_ioc;
    std::unique_ptr<boost::asio::steady_timer> m_timer;
    std::unique_ptr<net::executor_work_guard<net::io_context::executor_type>> m_guard;
    std::thread m_thread;
};


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

    auto config = ConnectionConfig::MakeTestNetConfig(argc == 2 ? argv[1] : "");

    BinanceBeast bb;
    bb.start(config);
    
    
    ListenKeyExtender listenKeyExtender{bb};


    bb.exchangeInfo(onExchangeInfo);
    //bb.serverTime(onServerTime);
    //bb.orderBook(onOrderBook, RestParams {RestParams::QueryParams {{"symbol", "BTCUSDT"}}});
    //bb.allOrders(onAllOrders, RestParams {RestParams::QueryParams {{"symbol", "BTCUSDT"}}});


    //bb.monitorMarkPrice(onMonitorMarkPriceAll, "!markPrice@arr@1s");
    //bb.monitorMarkPrice(onMonitorMarkPriceSymbol, "btcusdt@markPrice@1s");
    //bb.monitorKline(onMonitorKline, "btcusdt@kline_15m");
    //bb.monitorIndividualSymbolMiniTicker(onSymbolMiniTicker, "btcusdt");
    //bb.monitorAllMarketMiniTickers(onAllMarketMiniTickers);
    //bb.monitorIndividualSymbolTicker(onIndividualSymbolTicker, "btcusdt");
    //bb.monitorSymbolBookTicker(onSymbolBookTicker, "btcusdt");
    //bb.monitorAllBookTicker(onAllBookTickers);
    //bb.monitorLiquidationOrder(onLiquidationOrder, "btcusdt");
    //bb.monitorAllMarketLiduiqdationOrder(onLiquidationOrder);
    //bb.monitorPartialBookDepth(onBookDepth, "btcusdt", "20", "100ms");
    //bb.monitorDiffBookDepth(onBookDepth, "btcusdt", "100ms");

    //bb.monitorBlvtInfo(onBltv, "TRXDOWN");
    //bb.monitorBlvtNavKlines(onBltv, "TRXDOWN", "1m");
    //bb.monitorCompositeIndexSymbolInfo(onBltv, "btcusdt");
    
        
    //bb.monitorUserData(onUserData);
   

    {
        //bb.monitorUserData(onUserData);
        //bb.renewListenKey(onRenewListenKey);
        //bb.closeUserData(onCloseUserData);
        //bb.monitorUserData(onUserData);
    }


    {   // testing number of websocket io contexts
        //bb.monitorMarkPrice(onMonitorMarkPriceSymbol, "btcusdt@markPrice@1s");
        //bb.monitorMarkPrice(onMonitorMarkPriceSymbol, "ethusdt@markPrice@1s");
        //bb.monitorMarkPrice(onMonitorMarkPriceSymbol, "blzusdt@markPrice@1s");
        //bb.monitorMarkPrice(onMonitorMarkPriceSymbol, "adausdt@markPrice@1s");
        //bb.monitorMarkPrice(onMonitorMarkPriceSymbol, "xrpusdt@markPrice@1s");        
    }

    cmdFut.wait();
    

    return 0;
}


