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
    RestTest(Market m) : m_market(m)
    {

    }

    virtual void SetUp() override
    {
        auto config = ConnectionConfig::MakeLiveConfig(m_market, g_keyFile);

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
    Market m_market;
};


class UsdFuturesRest : public RestTest
{
public:
    UsdFuturesRest () : RestTest(Market::USDM)
    {

    }
};


class CoinFuturesRest : public RestTest
{
public:
    CoinFuturesRest () : RestTest(Market::COINM)
    {

    }
};


class SpotRest : public RestTest
{
public:
    SpotRest () : RestTest(Market::SPOT)
    {

    }
};


// USD-M
TEST_F(UsdFuturesRest, exchangeInfo)
{
    EXPECT_TRUE(runTest("/fapi/v1/exchangeInfo", RestParams{}, RestSign::Unsigned));
}

TEST_F(UsdFuturesRest, time)
{
    EXPECT_TRUE(runTest("/fapi/v1/time", RestParams{}, RestSign::Unsigned));
}

TEST_F(UsdFuturesRest, depth)
{
    EXPECT_TRUE(runTest("/fapi/v1/depth", RestParams{{{"symbol", "BTCUSDT"}}}, RestSign::Unsigned));
}

TEST_F(UsdFuturesRest, trades)
{
    EXPECT_TRUE(runTest("/fapi/v1/trades", RestParams{{{"symbol", "BTCUSDT"}}}, RestSign::Unsigned));
}

TEST_F(UsdFuturesRest, historicalTrades)
{
    EXPECT_TRUE(runTest("/fapi/v1/historicalTrades", RestParams{{{"symbol", "BTCUSDT"}}}, RestSign::Unsigned));
}

TEST_F(UsdFuturesRest, aggTrades)
{
    EXPECT_TRUE(runTest("/fapi/v1/aggTrades", RestParams{{{"symbol", "BTCUSDT"}}}, RestSign::Unsigned));
}

TEST_F(UsdFuturesRest, klines)
{
    EXPECT_TRUE(runTest("/fapi/v1/klines", RestParams{{{"symbol", "BTCUSDT"}, {"interval","15m"}}}, RestSign::Unsigned));
}

TEST_F(UsdFuturesRest, continuousKlines)
{
    EXPECT_TRUE(runTest("/fapi/v1/continuousKlines", RestParams{{{"pair", "BTCUSDT"}, {"interval","15m"}, {"contractType","PERPETUAL"}}}, RestSign::Unsigned));
}

TEST_F(UsdFuturesRest, indexPriceKlines)
{
    EXPECT_TRUE(runTest("/fapi/v1/indexPriceKlines", RestParams{{{"pair", "BTCUSDT"}, {"interval","15m"}}}, RestSign::Unsigned));
}

TEST_F(UsdFuturesRest, premiumIndex)
{
    EXPECT_TRUE(runTest("/fapi/v1/premiumIndex", RestParams{{{"symbol", "BTCUSDT"}, {"interval","15m"}}}, RestSign::Unsigned));
}

TEST_F(UsdFuturesRest, ticker24hr)
{
    EXPECT_TRUE(runTest("/fapi/v1/ticker/24hr", RestParams{{{"symbol", "BTCUSDT"}}}, RestSign::Unsigned));
}

TEST_F(UsdFuturesRest, tickerPrice)
{
    EXPECT_TRUE(runTest("/fapi/v1/ticker/price", RestParams{{{"symbol", "BTCUSDT"}}}, RestSign::Unsigned));
}

TEST_F(UsdFuturesRest, tickerbookTicker)
{
    EXPECT_TRUE(runTest("/fapi/v1/ticker/bookTicker", RestParams{{{"symbol", "BTCUSDT"}}}, RestSign::Unsigned));
}

TEST_F(UsdFuturesRest, openInterest)
{
    EXPECT_TRUE(runTest("/fapi/v1/openInterest", RestParams{{{"symbol", "BTCUSDT"}}}, RestSign::Unsigned));
}

TEST_F(UsdFuturesRest, indexInfo)
{
    EXPECT_TRUE(runTest("/fapi/v1/indexInfo", RestParams{{{"symbol", "DEFIUSDT"}}}, RestSign::Unsigned));
}


// COIN-M
TEST_F(CoinFuturesRest, dapi_premiumIndex)
{
    EXPECT_TRUE(runTest("/dapi/v1/ticker/price", RestParams{{{"symbol", "BTCUSD_PERP"}}}, RestSign::Unsigned));
}

TEST_F(CoinFuturesRest, dapi_klines)
{
    EXPECT_TRUE(runTest("/dapi/v1/depth", RestParams{{{"symbol", "BTCUSD_PERP"}, {"interval","15m"}}}, RestSign::Unsigned));
}


// SPOT
TEST_F(SpotRest, spot_tickerprice)
{
    EXPECT_TRUE(runTest("/api/v3/ticker/price", RestParams{{{"symbol", "ETHBTC"}}}, RestSign::Unsigned));
}

TEST_F(SpotRest, spot_depth)
{
    EXPECT_TRUE(runTest("/api/v3/depth", RestParams{{{"symbol", "BNBBTC"}}}, RestSign::Unsigned));
}


int main (int argc, char ** argv)
{
    std::cout << "\n\nTest REST API\n\n";
    
    if (argc != 2)
    {   
        std::cout << "Usage, requires key LIVE file or keys:\n"
                  << argv[0] << " <full path to keyfile>\n";
        return 1;
    }
    
    g_keyFile = std::filesystem::path{argv[1]};

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}