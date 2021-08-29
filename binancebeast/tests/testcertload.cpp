#include <binancebeast/BinanceBeast.h>
#include "testcommon.h"
#include <future>
#include <chrono>
#include <condition_variable>
#include <filesystem>

using namespace bblib;
using namespace bblib_test;


/// Set the usingTestRootCertificates to true but do load certificates from PEM file
void testLoadFromFile(string_view argv)
{
    auto config = ConnectionConfig::MakeLiveConfig(Market::USDM);   // use the live exchange, the testnet is ok without root certificates
    config.usingTestRootCertificates = false;           // we will use our own
    config.verifyPeer = true;

    auto pemPath = std::filesystem::path(argv).parent_path().parent_path() / "tests/testcerts.pem";

    try
    {
        BinanceBeast bb;
        bb.loadRootCertificate(pemPath);
        bb.start(config);    

        std::condition_variable cvHaveReply;
        bool dataError = false;
        string failMessage;

        bb.sendRestRequest([&](RestResponse result)
        {
            if (result.hasErrorCode())
            {
                dataError = true;
                failMessage = result.failMessage;
            }

            cvHaveReply.notify_one();
        },
        "/fapi/v1/ticker/bookTicker",                                       // the stream path
        RestSign::HMAC_SHA256,                                              // this calls requires a signature
        RestParams{{{"symbol", "BTCUSDT"}}},                                // rest parameters
        RequestType::Get);                                                  // this is a GET request

        auto haveReply = waitReply(cvHaveReply, "load cert file");
    
        if (haveReply && !dataError)
            std::cout << "PASS\n";
        else
            std::cout << "FAIL: " + failMessage << "\n";
    }
    catch(const std::exception& e)
    {
        std::cout << "FAIL: " << e.what() << "\n";
    }
}


/// Set the usingTestRootCertificates to true but don't load certificates
void testNoLoad (string_view argv)
{
    auto config = ConnectionConfig::MakeLiveConfig(Market::USDM);
    config.usingTestRootCertificates = false;   // we will use our own
    config.verifyPeer = true;
    
    auto pemPath = std::filesystem::path(argv).parent_path().parent_path() / "tests/testcerts.pem";

    try
    {
        BinanceBeast bb;
        bb.start(config);   

        std::condition_variable cvHaveReply;
        string failMessage;

        bb.sendRestRequest([&](RestResponse result)
        {
            if (result.hasErrorCode())
            {
                failMessage = result.failMessage;
            }

            cvHaveReply.notify_one();
        },
        "/fapi/v1/ticker/bookTicker",                                       // the stream path
        RestSign::HMAC_SHA256,                                              // this calls requires a signature
        RestParams{{{"symbol", "BTCUSDT"}}},                                // rest parameters
        RequestType::Get);                                                  // this is a GET request


        auto haveReply = waitReply(cvHaveReply, "no cert file");
    
        if (failMessage == "handshake certificate verify failed")
            std::cout << "PASS: " << failMessage << "\n";
        else
            std::cout << "FAIL: " << failMessage << "\n";
    }
    catch(const std::exception& e)
    {
        std::cout << "FAIL: " << e.what() << "\n";
    }
}

int main (int argc, char ** argv)
{
    std::cout << "\n\nTest Load SSL Certificates\n\n";

    
    testLoadFromFile(argv[0]);
    testNoLoad(argv[0]);
    
    
    return 0;

}