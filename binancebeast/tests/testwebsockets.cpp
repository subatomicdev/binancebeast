#include <binancebeast/BinanceBeast.h>
#include "testcommon.h"
#include <future>
#include <chrono>
#include <condition_variable>
#include <gtest/gtest.h>


using namespace bblib;
using namespace bblib_test;

std::filesystem::path g_futuresKeyFile, g_spotKeyFile;




class WsTest : public testing::Test
{
public:
    WsTest(Market market) : m_market(market)
    {

    }

    virtual ~WsTest () = default;


protected:

    virtual void SetUp() override
    {
        auto config = ConnectionConfig::MakeLiveConfig(m_market, g_futuresKeyFile);

        m_bb.start(config);
    }


    virtual bool runTest(const string& stream, const bool alwaysExpectData = true) = 0;


    bool waitReply (std::condition_variable& cvHaveReply, const std::chrono::milliseconds timeout = 6s)
    {
        std::mutex mux;

        std::unique_lock lck(mux);
        return cvHaveReply.wait_for(lck, timeout) != std::cv_status::timeout;
    }


protected:
    BinanceBeast m_bb;
    string m_path;
    bool m_dataError;
    bool m_alwaysExpectData = true;
    std::condition_variable m_cvHaveReply;
    Market m_market;
};




/// Start a websocket, do not explicitly close.
class NormalWsTest : public WsTest
{
protected:
    NormalWsTest(Market market) : WsTest(market)
    {

    }

    bool runTest(const string& stream, const bool alwaysExpectData =true) override
    {
        bool dataError;
        
        auto token = m_bb.startWebSocket([&, this](WsResponse result)
        {
            dataError = result.hasErrorCode();
            m_cvHaveReply.notify_all(); 

        }, stream);

        auto haveReply = waitReply(m_cvHaveReply);

        return (alwaysExpectData ? !dataError && haveReply : !dataError);
    }
};


class FuturesUsdmTest : public NormalWsTest
{
public:
    FuturesUsdmTest() : NormalWsTest(Market::USDM)
    {
    }
};

class SpotTest : public NormalWsTest
{
public:
    SpotTest() : NormalWsTest(Market::SPOT)
    {
    }
};


/// Start a websocket, wait a few seconds, then close the stream.
class DisconnectWsTest : public WsTest
{
public:
    DisconnectWsTest() : WsTest(Market::USDM)
    {
    }

protected:
    bool runTest(const string& stream, const bool alwaysExpectData = true) override
    {
        bool dataError;
        
        auto token = m_bb.startWebSocket([&, this](WsResponse result)
        {
            dataError = result.hasErrorCode();
            m_cvHaveReply.notify_all(); 
        }, stream);


        std::this_thread::sleep_for(3s);


        std::condition_variable cvDisconnect;
        bool disconnectFail = false;
        m_bb.stopWebSocket(token, [&](WsResponse result)
        {
            if (result.state == WsResponse::State::Disconnect)
                disconnectFail = result.hasErrorCode();

            cvDisconnect.notify_one();
        });


        std::mutex mux;
        std::unique_lock lck(mux);    

        auto timeout = cvDisconnect.wait_for(lck, 5s) == std::cv_status::timeout;

        return !dataError && !timeout && !disconnectFail;
    }
};



// Futures USDM
TEST_F (FuturesUsdmTest, aggregrateTrade) { EXPECT_TRUE(runTest("btcusdt@aggTrade")); }
TEST_F (FuturesUsdmTest, markPrice) { EXPECT_TRUE(runTest("btcusdt@markPrice@1s")); }
TEST_F (FuturesUsdmTest, markPriceForAll) { EXPECT_TRUE(runTest("!markPrice@arr@1s"));}
TEST_F (FuturesUsdmTest, klines) { EXPECT_TRUE(runTest("btcusdt@kline_15m"));}
TEST_F (FuturesUsdmTest, continuousContractKline) { EXPECT_TRUE(runTest("btcusdt_perpetual@continuousKline_1m"));}
TEST_F (FuturesUsdmTest, individualSymbolMiniTicker) { EXPECT_TRUE(runTest("btcusdt@miniTicker"));}
TEST_F (FuturesUsdmTest, allMarketTicker) { EXPECT_TRUE(runTest("!ticker@arr"));}
TEST_F (FuturesUsdmTest, individualSymbolBookTicker) { EXPECT_TRUE(runTest("btcusdt@bookTicker"));}
TEST_F (FuturesUsdmTest, allBookTicker) { EXPECT_TRUE(runTest("!bookTicker"));}
TEST_F (FuturesUsdmTest, liquidationOrder) { EXPECT_TRUE(runTest("btcusdt@forceOrder", false));}
TEST_F (FuturesUsdmTest, allMarketLiquidationOrder) { EXPECT_TRUE(runTest("!forceOrder@arr", false));}
TEST_F (FuturesUsdmTest, partialBookDepth) { EXPECT_TRUE(runTest("btcusdt@depth5@100ms"));}
TEST_F (FuturesUsdmTest, diffBookDepth) { EXPECT_TRUE(runTest("btcusdt@depth@100ms"));}
TEST_F (FuturesUsdmTest, compositeIndexSymbolInfo) { EXPECT_TRUE(runTest("defiusdt@compositeIndex"));}


TEST_F (DisconnectWsTest, allBookTicker) { EXPECT_TRUE(runTest("!bookTicker"));}

 

// SPOT
TEST_F (SpotTest, aggregrateTrade) { EXPECT_TRUE(runTest("btcusdt@aggTrade")); }
TEST_F (SpotTest, trade) { EXPECT_TRUE(runTest("btcusdt@trade")); }
TEST_F (SpotTest, klines) { EXPECT_TRUE(runTest("btcusdt@kline_15m"));}
TEST_F (SpotTest, individualSymbolMiniTicker) { EXPECT_TRUE(runTest("btcusdt@miniTicker"));}
TEST_F (SpotTest, allMarketTicker) { EXPECT_TRUE(runTest("!ticker@arr"));}
TEST_F (SpotTest, individualSymbolBookTicker) { EXPECT_TRUE(runTest("btcusdt@bookTicker"));}
TEST_F (SpotTest, allBookTicker) { EXPECT_TRUE(runTest("!bookTicker"));}
TEST_F (SpotTest, partialBookDepth) { EXPECT_TRUE(runTest("btcusdt@depth5@100ms"));}
TEST_F (SpotTest, diffBookDepth) { EXPECT_TRUE(runTest("btcusdt@depth@100ms"));}



int main (int argc, char ** argv)
{
    std::cout << "\n\nTest REST API\n\n";
    
    if (argc != 3)
    {   
        std::cout << "Usage, requires LIVE key file or keys:\n"
                  << argv[0] << " <path to futures keyfile> <path to spot keyfile>\n";
        return 1;
    }
    
    g_futuresKeyFile = std::filesystem::path{argv[1]};
    g_spotKeyFile = std::filesystem::path{argv[2]};

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();    
}
