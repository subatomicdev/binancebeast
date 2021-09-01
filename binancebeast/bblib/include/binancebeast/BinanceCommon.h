#ifndef BINANCEBEAST_COMMON_H
#define BINANCEBEAST_COMMON_H

#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/bind/bind.hpp>
#include <boost/json.hpp>
#include <filesystem>
#include <fstream>
#include <tuple>
#include <string>
#include <string_view>


namespace bblib
{
    #define BB_FUNCTION std::string {__func__}
    #define BB_FUNCTION_MSG(msg) std::string {__func__} +"()"+ msg
    #define BB_FUNCTION_ENTER BB_FUNCTION_MSG(" enter")

    
    using namespace boost::placeholders;    // to supress global placeholders warning from boost::bind

    namespace beast = boost::beast;         // from <boost/beast.hpp>
    namespace http = beast::http;           // from <boost/beast/http.hpp>
    namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
    namespace net = boost::asio;            // from <boost/asio.hpp>
    namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
    namespace json = boost::json;


    using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
    using std::string;
    using std::string_view;


    const std::string BINANCEBEAST_USER_AGENT = "binancebeast";


    enum class Market
    {
        None,
        USDM,
        COINM,
        SPOT
    };


    struct ConnectionConfig
    {   
        static std::tuple<string, string> readKeyFile (const std::filesystem::path& p, const bool isLive)
        {
            if (!std::filesystem::exists(p))
            {
                throw std::runtime_error("key file path does not exist");
            }
            else if (std::filesystem::file_size(p) > 140)
			{
				throw std::runtime_error("key file invalid format. Should be 3 lines:\nLine 1: <test | live>\nLine 2: api key\nLine 3: secret key");
			}
			else
			{
				string line, api, secret;
									
				std::ifstream fileStream(p.c_str());
				std::getline(fileStream, line);

				if ((line == "live" && !isLive) || (line == "test" && isLive))
				{
                    throw std::runtime_error("key file is for " + line + " but not being configured for " + line);
				}
                else
                {
                    std::getline(fileStream, api);
					std::getline(fileStream, secret);

                    return std::make_tuple(api, secret);
                }
			}
        }

        static ConnectionConfig MakeTestNetConfig (const Market market, const std::filesystem::path& keyFile) 
        {
            if (!std::filesystem::path(keyFile).empty())
            {
                auto keys = readKeyFile(keyFile, false);
                return MakeConfig(std::get<0>(keys), std::get<1>(keys), false, market);
            }
            else
                return MakeConfig("", "", false, market);
        }

        static ConnectionConfig MakeLiveConfig (const Market market, const std::filesystem::path& keyFile)
        {
            if (!std::filesystem::path(keyFile).empty())
            {
                auto keys = readKeyFile(keyFile, true);
                return MakeConfig(std::get<0>(keys), std::get<1>(keys), true, market);
            }
            else
                return MakeConfig("", "", true, market);
        }

        static ConnectionConfig MakeTestNetConfig (const Market market, const string& apiKey = "", const string& secretKey = "")
        {
            return MakeConfig(apiKey, secretKey, false, market);
        }

        static ConnectionConfig MakeLiveConfig (const Market market, const string& apiKey = "", const string& secretKey = "")
        {
            return MakeConfig(apiKey, secretKey, true, market);
        }

    public:
        struct ConnectionKeys
        {
            ConnectionKeys(const string& apiKey = "") : api(apiKey)
            {

            }
            ConnectionKeys(const string& apiKey, const string secretKey) : api(apiKey), secret(secretKey)
            {

            }

            string api;
            string secret;
        };


    public:
        ConnectionConfig() : verifyPeer(false), usingTestRootCertificates(true)
        {
        }


    private:
        ConnectionConfig (const string& restUri, const string& wsUri, const bool sslVerifyPeer, const ConnectionKeys& apiKeys, const string restPort = "443", const string websockPort = "443") :   
            restApiUri(restUri),
            wsApiUri(wsUri),
            verifyPeer(sslVerifyPeer),
            keys(apiKeys),
            usingTestRootCertificates(true),
            restPort(restPort),
            wsPort(websockPort)
        {
        }

        static ConnectionConfig MakeConfig (const string& apiKey, const string& secretKey, const bool isLive, const Market market)
        {
            // USD-M
            static std::string DefaultUsdFuturesTestnetWsUri {"stream.binancefuture.com"};
            static std::string DefaultUsdFuturesTestnetRestUri {"testnet.binancefuture.com"};
            static std::string DefaultUsdFuturesWsUri {"fstream.binance.com"};
            static std::string DefaultUsdFuturesRestUri {"fapi.binance.com"};

            // COIN-M
            static std::string DefaultCoinFuturesTestnetWsUri {"dstream.binancefuture.com"};
            static std::string DefaultCoinFuturesTestnetRestUri {"testnet.binancefuture.com"};
            static std::string DefaultCoinFuturesWsUri {"dstream.binance.com"};
            static std::string DefaultCoinFuturesRestUri {"dapi.binance.com"};

            // SPOT
            static std::string DefaultSpotWsUri {"stream.binance.com"};
            static std::string DefaultSpotRestUri {"api.binance.com"};
            static std::string DefaultSpotTestnetWsUri {"testnet.binance.vision"};
            static std::string DefaultSpotTestnetRestUri {"testnet.binance.vision"};

            
            if (market == Market::USDM)
            {
                return (isLive ?    ConnectionConfig {DefaultUsdFuturesRestUri, DefaultUsdFuturesWsUri, true, ConnectionKeys{apiKey, secretKey}} : 
                                    ConnectionConfig {DefaultUsdFuturesTestnetRestUri, DefaultUsdFuturesTestnetWsUri, false, ConnectionKeys{apiKey, secretKey}} );                
            }
            else if (market == Market::COINM)
            {
                return (isLive ?    ConnectionConfig {DefaultCoinFuturesRestUri, DefaultCoinFuturesWsUri, true, ConnectionKeys{apiKey, secretKey}} :
                                    ConnectionConfig {DefaultCoinFuturesTestnetRestUri, DefaultCoinFuturesTestnetWsUri, false, ConnectionKeys{apiKey, secretKey}});
            }
            else if (market == Market::SPOT)
            {
                return (isLive ?    ConnectionConfig {DefaultSpotRestUri, DefaultSpotWsUri, true, ConnectionKeys{apiKey, secretKey}, "443", "9443"} :
                                    ConnectionConfig {DefaultSpotTestnetRestUri, DefaultSpotTestnetWsUri, false, ConnectionKeys{apiKey, secretKey}});
            }
            else
                throw std::runtime_error ("Invalid market type"); 
        }


    public:
        string restApiUri;
        string wsApiUri;
        bool verifyPeer;    // connecting to the TestNet fails to verify peer
        ConnectionKeys keys;
        bool usingTestRootCertificates;
        string restPort = "443";
        string wsPort = "443";
    };

    
    inline void fail(beast::error_code ec, const char * what)
    {
        throw std::runtime_error(ec.message() + what);
    }

    inline void fail(const char * what)
    {
        throw std::runtime_error(what);
    }

    inline void fail(const string& what)
    {
        throw std::runtime_error(what);
    }

    /// Call the user's callback with the failure on the thread pool.
    template<typename ResultT>
    inline void fail(beast::error_code ec, const string what, net::thread_pool& callerPool, std::function<void(ResultT)> callback)
    {
        if (callback)
        {
            net::post(callerPool, boost::bind(callback, ResultT {std::move(what + " " + ec.message())})); // call callback with  a Failed state
        }    
    }

    /// Call the user's callback with the failure on the calling thread.
    template<typename ResultT>
    inline void fail(beast::error_code ec, const string what, std::function<void(ResultT)> callback)
    {
        if (callback)
        {
            callback(ResultT {std::move(what + " " + ec.message())});
        }   
    }

    /// Call the user's callback with the failure on the calling thread.
    template<typename ResultT>
    inline void fail(const string what, std::function<void(ResultT)> callback)
    {
        if (callback)
        {
            callback(ResultT {std::move(what)});
        }    
    }
}

#endif
