#include <binancebeast/BinanceBeast.h>
#include "testcommon.h"
#include <future>
#include <chrono>
#include <condition_variable>
#include <gtest/gtest.h>


using namespace bblib;
using namespace bblib_test;

std::filesystem::path g_keyFile;


/// These test the REST api. They require an API key.
class RestTest : public testing::Test
{
protected:


    virtual void SetUp() override
    {
        auto config = ConnectionConfig::MakeTestNetConfig(Market::USDM, g_keyFile);

        m_bb.start(config);
    }


    bool runTest(const string& path, RestParams params, RestSign sign)
    {
        std::condition_variable cvHaveReply;

        auto handler = [&](RestResponse result)
        {
            m_dataError = bblib_test::hasError(path, result);

            cvHaveReply.notify_one();
        };

        m_bb.sendRestRequest(handler, path, sign, params, RequestType::Get);

        auto haveReply = waitReply(cvHaveReply) ;
            
        return !m_dataError && haveReply;
    }


    bool waitReply (std::condition_variable& cvHaveReply, const std::chrono::milliseconds timeout = 5s)
    {
        std::mutex mux;

        std::unique_lock lck(mux);
        return cvHaveReply.wait_for(lck, timeout) != std::cv_status::timeout;
    }


private:
    BinanceBeast m_bb;
    string m_path;
    bool m_dataError;
};


// USD-M
TEST_F(RestTest, exchangeInfo)
{
    EXPECT_TRUE(runTest("/fapi/v1/exchangeInfo", RestParams{}, RestSign::Unsigned));
}

TEST_F(RestTest, time)
{
    EXPECT_TRUE(runTest("/fapi/v1/time", RestParams{}, RestSign::Unsigned));
}

TEST_F(RestTest, depth)
{
    EXPECT_TRUE(runTest("/fapi/v1/depth", RestParams{{{"symbol", "BTCUSDT"}}}, RestSign::Unsigned));
}

TEST_F(RestTest, trades)
{
    EXPECT_TRUE(runTest("/fapi/v1/trades", RestParams{{{"symbol", "BTCUSDT"}}}, RestSign::Unsigned));
}

TEST_F(RestTest, historicalTrades)
{
    EXPECT_TRUE(runTest("/fapi/v1/historicalTrades", RestParams{{{"symbol", "BTCUSDT"}}}, RestSign::Unsigned));
}

TEST_F(RestTest, aggTrades)
{
    EXPECT_TRUE(runTest("/fapi/v1/aggTrades", RestParams{{{"symbol", "BTCUSDT"}}}, RestSign::Unsigned));
}

TEST_F(RestTest, klines)
{
    EXPECT_TRUE(runTest("/fapi/v1/klines", RestParams{{{"symbol", "BTCUSDT"}, {"interval","15m"}}}, RestSign::Unsigned));
}

TEST_F(RestTest, continuousKlines)
{
    EXPECT_TRUE(runTest("/fapi/v1/continuousKlines", RestParams{{{"pair", "BTCUSDT"}, {"interval","15m"}, {"contractType","PERPETUAL"}}}, RestSign::Unsigned));
}

TEST_F(RestTest, indexPriceKlines)
{
    EXPECT_TRUE(runTest("/fapi/v1/indexPriceKlines", RestParams{{{"pair", "BTCUSDT"}, {"interval","15m"}}}, RestSign::Unsigned));
}

TEST_F(RestTest, premiumIndex)
{
    EXPECT_TRUE(runTest("/fapi/v1/premiumIndex", RestParams{{{"symbol", "BTCUSDT"}, {"interval","15m"}}}, RestSign::Unsigned));
}

TEST_F(RestTest, ticker24hr)
{
    EXPECT_TRUE(runTest("/fapi/v1/ticker/24hr", RestParams{{{"symbol", "BTCUSDT"}}}, RestSign::Unsigned));
}

TEST_F(RestTest, tickerPrice)
{
    EXPECT_TRUE(runTest("/fapi/v1/ticker/price", RestParams{{{"symbol", "BTCUSDT"}}}, RestSign::Unsigned));
}

TEST_F(RestTest, tickerbookTicker)
{
    EXPECT_TRUE(runTest("/fapi/v1/ticker/bookTicker", RestParams{{{"symbol", "BTCUSDT"}}}, RestSign::Unsigned));
}

TEST_F(RestTest, openInterest)
{
    EXPECT_TRUE(runTest("/fapi/v1/openInterest", RestParams{{{"symbol", "BTCUSDT"}}}, RestSign::Unsigned));
}

TEST_F(RestTest, indexInfo)
{
    EXPECT_TRUE(runTest("/fapi/v1/indexInfo", RestParams{{{"symbol", "DEFIUSDT"}}}, RestSign::Unsigned));
}


// COIN-M
TEST_F(RestTest, dapi_premiumIndex)
{
    EXPECT_TRUE(runTest("/dapi/v1/premiumIndex", RestParams{{{"symbol", "BTCUSD_PERP"}}}, RestSign::Unsigned));
}

TEST_F(RestTest, dapi_klines)
{
    EXPECT_TRUE(runTest("/dapi/v1/klines", RestParams{{{"symbol", "BTCUSD_PERP"}, {"interval","15m"}}}, RestSign::Unsigned));
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
    
    g_keyFile = std::filesystem::path{argv[1]};

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}