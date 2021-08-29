#include <binancebeast/BinanceBeast.h>
#include <mutex>
#include <functional>
#include <condition_variable>


using namespace bblib;



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


/// Start the user data stream.
/// We only do so for 30 seconds so the listen key will never expire but code is an example, but it has been tested with a long timeout.
/// To see output from the user data, you'll need to create/close orders on the Binance TestNet while the app is running.
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
    ListenKeyExtender extender {bb};    // will send a listen key renew request every 59 minutes

    // start the network processing
    bb.start(config);

    // receive user data 
    auto token = bb.startUserData([&](WsResponse result)      // this is called for each message or error
    {  
        std::cout << result.json << "\n\n";

        if (result.hasErrorCode())
        {
            std::cout << "Error: " << result.failMessage << "\n";;
        }
        else
        {
            auto topLevel = result.json.as_object();
            if (const auto eventType = json::value_to<string>(topLevel["e"]); eventType == "listenKeyExpired")
            {
                std::cout << "listen key expired\n";
                bb.renewListenKey([](WsResponse renewKeyResult)
                { 
                    if (renewKeyResult.hasErrorCode())
                        std::cout << "Error: " << renewKeyResult.failMessage << "\n";
                });
            }
            else
            {
                std::cout << result.json << "\n";
            }
        }
    });

    
    std::cout << "Running. Create or close orders on the Binance Futures TestNet to see user data. Running for 30 seconds.\n";

    using namespace std::chrono_literals;    
    std::this_thread::sleep_for(30s);
    
    // don't need to call this, for demonstration purposes only
    bb.stopWebSocket(token, [](WsResponse result)
    {
        if (result.state == WsResponse::State::Disconnect)
        {
            std::cout << "Disconnected\n";
        }
    });

    return 0;
}
