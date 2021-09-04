#include <binancebeast/BinanceBeast.h>
#include "testcommon.h"
#include <future>
#include <chrono>
#include <condition_variable>
#include <gtest/gtest.h>





using namespace bblib;
using namespace bblib_test;

std::filesystem::path   g_futuresKeyFile;


class WsTest : public testing::Test
{
public:
    WsTest(Market market) : m_market(market)
    {

    }

    virtual ~WsTest () = default;


protected:

    virtual void SetUp() override = 0;

    virtual bool runTest(const string& stream) = 0;


protected:
    BinanceBeast m_bb;
    string m_path;
    bool m_dataError;
    bool m_alwaysExpectData = true;
    std::condition_variable m_cvHaveReply;
    Market m_market;
};





class UserDataTest : public WsTest
{
public:

    UserDataTest(Market market) : WsTest(market)
    {
    }

    
    string getPriceForOrder (string& stream)
    {
        std::condition_variable haveReply;
        double price;
        bool dataError = true;

        m_bb.sendRestRequest([&](RestResponse result)
        {
            if (dataError = result.hasErrorCode() ; !dataError)    
                price = std::stod(json::value_to<string>(result.json.as_object()["price"]));

            haveReply.notify_all();            
        },
        stream,
        RestSign::HMAC_SHA256,
        RestParams{{{"symbol", "BTCUSDT"}}},
        RequestType::Get);

        std::mutex mux;
        std::unique_lock lck{mux};
        haveReply.wait(lck);

        return std::to_string( std::floor(((price * 0.9998) * 100) + .5) / 100 );
    }

    
    virtual bool runTest(const string& userDataStream) override
    {
        string  newOrderStream = "/fapi/v1/order",
                symbolPriceStream = "/fapi/v1/ticker/price",
                cancelOpenOrdersStream = "/fapi/v1/allOpenOrders";
        
        if (m_market == Market::SPOT)
        {
            newOrderStream = "";
            symbolPriceStream = "";
        }
        else if (m_market == Market::COINM)
        {
            newOrderStream = "";
            symbolPriceStream = "";
        }

        return runTest(userDataStream, newOrderStream, cancelOpenOrdersStream, getPriceForOrder(symbolPriceStream));
    }

    
    bool runTest(const string& userDataStream, const string& newOrderStream, const string& cancelAllOpenOrdersStream, const string& price) 
    {
        bool dataError = true;

        // start the user data stream
        std::condition_variable cvHaveUserData;
        auto token = m_bb.startUserData([&](WsResponse result)
        {
            dataError = result.hasErrorCode();
            cvHaveUserData.notify_all(); 

        }, userDataStream);


        // create an order, the price is set so it's unlikely to be filled, allowing us to cancel with an cancel open orders call
        std::condition_variable cvCreateOrder;
        m_bb.sendRestRequest([&](RestResponse result)
        {
            dataError = result.hasErrorCode();
            cvCreateOrder.notify_one();
        },
        newOrderStream,
        RestSign::HMAC_SHA256,
        RestParams{{{"symbol", "BTCUSDT"}, {"side", "BUY"}, {"timeInForce", "GTC"}, {"price", price}, {"type", "LIMIT"}, {"quantity", "0.001"}}},
        RequestType::Post);


        waitReply(cvCreateOrder);

        // if we cancel too soon after creating, the user data isn't always triggered
        std::this_thread::sleep_for(2000ms);    // TODO check this 

        // cancel all order we just made, this will trigger user data
        std::condition_variable cvCancel;
        m_bb.sendRestRequest([&](RestResponse result)
        {
            dataError = result.hasErrorCode();
            cvCancel.notify_one();
        },
        cancelAllOpenOrdersStream,
        RestSign::HMAC_SHA256,
        RestParams{{{"symbol", "BTCUSDT"}, {"side", "BUY"}, {"timeInForce", "GTC"}, {"price", price}, {"type", "LIMIT"}, {"quantity", "0.001"}}},
        RequestType::Delete);
        
        
        waitReply(cvCancel);

        auto haveReply = waitReply(cvHaveUserData, 20s);
        return !dataError && haveReply;
    }
};


/// Creates a LIMIT order on the test net, with the price slightly different from current price
/// so that when it's filled, the user data stream will receive the update.
class UserDataFuturesTest : public UserDataTest
{
public:
    UserDataFuturesTest() : UserDataTest(Market::USDM)
    {
    }  

    

protected:
    virtual void SetUp() override
    {
        m_bb.start(ConnectionConfig::MakeTestNetConfig(m_market, g_futuresKeyFile));
    }

};



// user data: usdm
TEST_F (UserDataFuturesTest, UsdFuturesUserData) { EXPECT_TRUE(runTest("/fapi/v1/listenKey")); }


int main (int argc, char ** argv)
{
    std::cout << "\n\nTest User Data API\n\n";
    
    if (argc != 2)
    {   
        std::cout << "Usage, requires TEST key file or keys:\n"
                  << argv[0] << " <path to futures TEST keyfile>\n";
        return 1;
    }
    
    g_futuresKeyFile = std::filesystem::path{argv[1]};
    
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}