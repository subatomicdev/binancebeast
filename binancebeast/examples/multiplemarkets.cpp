#include <binancebeast/BinanceBeast.h>
#include <mutex>
#include <functional>
#include <condition_variable>
#include <string>
#include <filesystem>
#include <functional>
#include <future>
#include <thread>
#include <set>
#include <map>

using namespace bblib;
using namespace std::chrono_literals;
using std::string;


///
/// A simple example of how to receive data from multiple markets.
///


class MarketScanner
{
public:
    MarketScanner (Market m, const string& apiK, const string& secretK) : m_market(m)
    {
        m_config = ConnectionConfig::MakeTestNetConfig(m, apiK, secretK);
    }

    
    MarketScanner (Market m, const std::filesystem::path& keyFile) : m_market(m)
    {
        m_config = ConnectionConfig::MakeTestNetConfig(m, keyFile);
    }


    virtual ~MarketScanner()
    {
        stop();
    }


    void start ()
    {
        m_running.store(true);

        m_bb.start(m_config);


        setSymbolsToScan();   // default is all symbols

        symbolBookTicker();

        m_thread = std::move(std::thread{std::bind(&MarketScanner::run, this)});
    }


    void stop()
    {
        m_running.store(false);

        m_runCv.notify_all();

        m_thread.join();
    }


protected:
    bool running() 
    {
        return m_running.load();
    }


    void waitForExit()
    {
        std::mutex mux;
        std::unique_lock lck(mux);

        m_runCv.wait(lck, [this]{ return running();});
    }


    string marketName () const
    {
        static std::map<Market, string> name = {{Market::USDM, "Futures USD-M"},{Market::SPOT, "SPOT"},{Market::COINM, "Futures COIN-M"}};
        return name[m_market];
    }


    virtual void setSymbolsToScan()
    {
       m_symbolsToScan = getAllSymbols();
    }


    std::set<string> getAllSymbols()
    {
        std::condition_variable cvData;
        
        std::set<string> syms;

        // get all symbols
        m_bb.sendRestRequest([&, this](RestResponse result)
        {
            if (result.hasErrorCode())
                std::cout << "ERROR: " << result.failMessage << "\n";
            else
            {
                auto& arr = result.json.as_array();

                for (auto&& entry : arr)
                    syms.insert(json::value_to<string>(entry.as_object()["symbol"]));
            }

            cvData.notify_one();

        }, allSymbolsStreamName(), RestSign::Unsigned, RestParams {}, RequestType::Get);


        std::mutex mux;
        std::unique_lock lck(mux);
        cvData.wait(lck);

        return syms;
    }


    /// User all book ticker to get all symbols, ignore price.
    void symbolBookTicker()
    {
        m_bb.startWebSocket([this](WsResponse result)
        {
            std::cout << marketName() << ":\n";

            if (result.hasErrorCode())
                std::cout << result.failMessage << "\n";
            else
            {
                // offload data to analyse, etc
                std::cout << result.json << "\n";
            }

        }, "!bookTicker");  
    }


private:
    virtual void run () = 0;
    virtual string allSymbolsStreamName() = 0;

protected:
    BinanceBeast m_bb;
    std::set<string> m_symbolsToScan;

private:
    std::thread m_thread;
    Market m_market;
    ConnectionConfig m_config;
    std::atomic_bool m_running;
    std::condition_variable m_runCv;
};


class UsdmScanner : public MarketScanner
{
public:

    UsdmScanner (const std::filesystem::path& keyFile) : MarketScanner (Market::USDM, keyFile)
    {

    }

    UsdmScanner (const string& apiK, const string& secretK) : MarketScanner (Market::USDM, apiK, secretK)
    {
    }

private:
    
    virtual void run () override
    {
        waitForExit();
    }

protected:
    virtual string allSymbolsStreamName() override
    {
        return "/fapi/v1/ticker/price";
    }

private:
};


class CoinmScanner : public MarketScanner
{
public:

    CoinmScanner (const std::filesystem::path& keyFile) : MarketScanner (Market::COINM, keyFile)
    {

    }

    CoinmScanner (const string& apiK, const string& secretK) : MarketScanner (Market::COINM, apiK, secretK)
    {
    }

private:
    
    virtual void run () override
    {
        waitForExit();
    }

protected:
    virtual string allSymbolsStreamName() override
    {
        return "/dapi/v1/ticker/price ";
    }


private:

};


///
class SpotScanner : public MarketScanner
{
public:

    SpotScanner (const std::filesystem::path& keyFile) : MarketScanner (Market::SPOT, keyFile)
    {

    }

    SpotScanner (const string& apiK, const string& secretK) : MarketScanner (Market::SPOT, apiK, secretK)
    {
    }

private:
    
    virtual void run () override
    {        
        waitForExit();
    }

protected:
    virtual string allSymbolsStreamName() override
    {
        return "/api/v3/ticker/price ";
    }


private:
};



int main (int argc, char ** argv)
{
    if (argc != 3)
    {   
        std::cout << "Usage, requires two key file paths:\n"
                  << "For key file: " << argv[0] << " <path to futures keyfile> <path to spot keyfile>\n";        
        return 1;
    }

    const std::vector<std::shared_ptr<MarketScanner>> scanners =
    {
        std::make_shared<UsdmScanner>(argv[1]),
        std::make_shared<CoinmScanner>(argv[1]),
        std::make_shared<SpotScanner>(argv[2])
    };


    for (auto& scanner : scanners)
        scanner->start();


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


    cmdFut.wait();

    return 0;
}