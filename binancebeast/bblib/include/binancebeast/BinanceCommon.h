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
#include <boost/json.hpp>
#include <boost/bind/bind.hpp>
#include <boost/json.hpp>


namespace bblib
{
    #define BB_FUNCTION std::string {__func__}
    #define BB_FUNCTION_MSG(msg) std::string {__func__} +"()"+ msg
    #define BB_FUNCTION_ENTER BB_FUNCTION_MSG(" enter")

    using namespace boost::placeholders;    // to surpress global placeholders warning from boost::bind


    namespace beast = boost::beast;         // from <boost/beast.hpp>
    namespace http = beast::http;           // from <boost/beast/http.hpp>
    namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
    namespace net = boost::asio;            // from <boost/asio.hpp>
    namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
    namespace json = boost::json;

    using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

    using std::string;
    const std::string BINANCEBEAST_USER_AGENT = "binancebeast";


    struct ConnectionConfig
    {    
        static ConnectionConfig MakeTestNetConfig ()
        {
            static std::string DefaultFuturesTestnetWsUri {"stream.binancefuture.com"};
            static std::string DefaultUsdFuturesTestnetRestUri {"testnet.binancefuture.com"};

            return ConnectionConfig {DefaultUsdFuturesTestnetRestUri, DefaultFuturesTestnetWsUri, false};
        }

        static ConnectionConfig MakeLiveConfig ()
        {
            static std::string DefaultFuturesWsUri {"fstream.binance.com"};
            static std::string DefaultUsdFuturesRestUri {"fapi.binance.com"};

            return ConnectionConfig {DefaultUsdFuturesRestUri, DefaultFuturesWsUri, true};
        }

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
        ConnectionConfig() : verifyPeer(false)
        {
        }


    private:
        ConnectionConfig (const string& restUri, const string& wsUri, const bool sslVerifyPeer, const ConnectionKeys& apiKeys = ConnectionKeys{}) :   
                restApiUri(restUri),
                wsApiUri(wsUri),
                verifyPeer(sslVerifyPeer),
                keys(apiKeys)
        {
        }

        
    public:
        string restApiUri;
        string wsApiUri;
        bool verifyPeer;
        ConnectionKeys keys;
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
            net::post(callerPool, boost::bind(callback, ResultT {std::move(what)})); // call callback with  a Failed state
        }    
    }

    /// Call the user's callback with the failure on the calling thread.
    template<typename ResultT>
    inline void fail(beast::error_code ec, const string what, std::function<void(ResultT)> callback)
    {
        if (callback)
        {
            callback(ResultT {std::move(what)});
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
