#include <binancebeast/BinanceBeast.h>
#include <future>
#include <thread>
#include <chrono>


using namespace bblib;



bool handleError(RestResponse& result)
{
    if (result.hasErrorCode())
    {
        std::cout << "REST Error: code = " << result.json.as_object()["code"] << "\nreason: " << result.json.as_object()["msg"] << "\n";   
        return true;
    }
    return false;
}

bool handleError(WsResponse& result)
{
    if (result.hasErrorCode())
    { 
        std::cout << "WS Error: code = " << result.json.as_object()["code"] << "\nreason: " << result.json.as_object()["msg"] << "\n";   
        return true;
    }
    return false;
}


void onUserData(WsResponse result)
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


void onRenewListenKey(WsResponse result)
{
    std::cout << BB_FUNCTION_ENTER << "\n";

    std::cout << "\nListen key renewal/extend success(true) or fail(false): " << std::boolalpha << (result.state == WsResponse::State::Success) << "\n";
}


void onCloseUserData(WsResponse result)
{
    std::cout << BB_FUNCTION_ENTER << "\n";

    std::cout << "\nUser data close success(true) or fail(false): " << std::boolalpha << (result.state == WsResponse::State::Success) << "\n";
}


void onWsResponse(WsResponse result)
{
    if (result.state == WsResponse::State::Disconnect)
        std::cout << "\nDisconnected\n";
    else if (result.hasErrorCode())
        std::cout << "\nFAIL: " << result.failMessage << "\n";
    else
        std::cout << "\n" << result.json << "\n";
}

void onRestResponse(RestResponse result)
{
    if (result.hasErrorCode())
        std::cout << "\nFAIL: " << result.failMessage << "\n";
    else
        std::cout << "\n" << result.json << "\n";
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

    void onListenKeyRenew(WsResponse result)
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


using namespace std::chrono_literals;

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

    auto config = argc == 2 ? ConnectionConfig::MakeTestNetConfig(Market::SPOT, std::filesystem::path{argv[1]}) : ConnectionConfig::MakeTestNetConfig(Market::SPOT);

    BinanceBeast bb;
    bb.start(config, 4, 8);
    
    bb.sendRestRequest([](RestResponse result)
    {
        if (result.hasErrorCode())
            std::cout << result.failMessage << "\n";
        else
            std::cout << result.json << "\n";
        
    },"/api/v3/ticker/price", RestSign::Unsigned, RestParams{}, RequestType::Get);


    bb.startWebSocket([](WsResponse result)
    {
        if (result.hasErrorCode())
            std::cout << result.failMessage << "\n";
        else
            std::cout << result.json << "\n";

    }, "!bookTicker");

    //ListenKeyExtender listenKeyExtender{bb};

    //bb.sendRestRequest(onRestResponse, "/fapi/v1/exchangeInfo", RestSign::Unsigned, RestParams{}, RequestType::Get);
    //bb.sendRestRequest(onRestResponse, "/fapi/v1/allOrders", RestSign::HMAC_SHA256, RestParams{{{"symbol", "BTCUSDT"}}});
    //bb.sendRestRequest(onRestResponse, "/fapi/v1/depth", RestSign::Unsigned, RestParams{{{"symbol", "BTCUSDT"}}});

    //auto token = bb.startWebSocket(onWsResponse, "!bookTicker"/*"btcusdt@aggTrade"*//*"btcusdt@markPrice@1s"*/);

    //bb.startUserData(onUserData);

    /*
    std::this_thread::sleep_for(5s);
    std::cout << "Closing\n";
    bb.stopWebSocket(token);
    */

    {
        //bb.startUserData(onUserData);
        //bb.renewListenKey(onRenewListenKey);
        //bb.closeUserData(onCloseUserData);
        //bb.startUserData(onUserData);
    }

    
    cmdFut.wait();

    return 0;
}
