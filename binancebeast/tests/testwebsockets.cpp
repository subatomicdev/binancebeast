#include <binancebeast/BinanceBeast.h>
#include "testcommon.h"
#include <future>
#include <chrono>
#include <condition_variable>
#include <gtest/gtest.h>


using namespace bblib;
using namespace bblib_test;

std::filesystem::path g_keyFile;



class WsTest : public testing::Test
{
protected:

    virtual void SetUp() override
    {
        auto config = ConnectionConfig::MakeTestNetConfig(Market::USDM, g_keyFile);

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
};


/// Start a websocket, do not explicitly close.
class NormalWsTest : public WsTest
{
protected:
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


/// Start a websocket, wait a few seconds, then close the stream.
class DisconnectWsTest : public WsTest
{
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


TEST_F (NormalWsTest, aggregrateTrade) { EXPECT_TRUE(runTest("btcusdt@aggTrade")); }
TEST_F (NormalWsTest, markPrice) { EXPECT_TRUE(runTest("btcusdt@markPrice@1s")); }
TEST_F (NormalWsTest, markPriceForAll) { EXPECT_TRUE(runTest("!markPrice@arr@1s"));}
TEST_F (NormalWsTest, klines) { EXPECT_TRUE(runTest("btcusdt@kline_15m"));}
TEST_F (NormalWsTest, continuousContractKline) { EXPECT_TRUE(runTest("btcusdt_perpetual@continuousKline_1m"));}
TEST_F (NormalWsTest, individualSymbolMiniTicker) { EXPECT_TRUE(runTest("btcusdt@miniTicker"));}
TEST_F (NormalWsTest, allMarketTicker) { EXPECT_TRUE(runTest("!ticker@arr"));}
TEST_F (NormalWsTest, individualSymbolBookTicker) { EXPECT_TRUE(runTest("btcusdt@bookTicker"));}
TEST_F (NormalWsTest, allBookTicker) { EXPECT_TRUE(runTest("!bookTicker"));}
TEST_F (NormalWsTest, liquidationOrder) { EXPECT_TRUE(runTest("btcusdt@forceOrder", false));}
TEST_F (NormalWsTest, allMarketLiquidationOrder) { EXPECT_TRUE(runTest("!forceOrder@arr", false));}
TEST_F (NormalWsTest, partialBookDepth) { EXPECT_TRUE(runTest("btcusdt@depth5@100ms"));}
TEST_F (NormalWsTest, diffBookDepth) { EXPECT_TRUE(runTest("btcusdt@depth@100ms"));}
TEST_F (NormalWsTest, compositeIndexSymbolInfo) { EXPECT_TRUE(runTest("defiusdt@compositeIndex"));}


TEST_F (DisconnectWsTest, allBookTicker) { EXPECT_TRUE(runTest("!bookTicker"));}
 



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
