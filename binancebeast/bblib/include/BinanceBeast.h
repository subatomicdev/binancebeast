#ifndef BINANCEBEAST_H
#define BINANCEBEAST_H


#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/json.hpp>
#include <boost/bind/bind.hpp>
#include <openssl/hmac.h>   // to sign query params
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <unordered_map>
#include <map>
#include <sstream>


namespace bblib
{

#define BB_FUNCTION std::string {__func__}
#define BB_FUNCTION_MSG(msg) std::string {__func__} +"()"+ msg
#define BB_FUNCTION_ENTER BB_FUNCTION_MSG(" enter")


using namespace boost::placeholders;


namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
namespace json = boost::json;




using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
using std::string;


struct RestResult
{
    enum class State { Fail, Success };

    RestResult (string failReason) : state(State::Fail), failMessage(failReason)
    {

    }

    RestResult(json::value&& object) : json (std::move(object)), state(State::Success)
    {

    }

    bool hasErrorCode() const
    {
        bool error = false;
        try
        {
            if (json.is_object())
                error = json.as_object().if_contains("code");
        }
        catch(...)
        {            
        }
        
        return error;
    }

    json::value json;
    State state;
    string failMessage;
};

struct RestParams
{
    using QueryParams = std::unordered_map<string, string>;

    RestParams () 
    {

    }
    
    RestParams (QueryParams&& params) : queryParams(std::move(params))
    {
    }

    RestParams (const QueryParams& params) : queryParams(params)
    {
    }


    QueryParams queryParams;
};


using RestCallback = std::function<void(RestResult)>;


struct ConnectionConfig
{    
    static ConnectionConfig MakeTestNetConfig ()
    {
        static std::string DefaultFuturesTestnetWsUri {"wss://stream.binancefuture.com"};
        static std::string DefaultUsdFuturesTestnetRestUri {"testnet.binancefuture.com"};

        return ConnectionConfig {DefaultUsdFuturesTestnetRestUri, DefaultFuturesTestnetWsUri, false};
    }

    static ConnectionConfig MakeLiveConfig ()
    {
        static std::string DefaultFuturesWsUri {"wss://fstream.binance.com"};
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


class RestSession : public std::enable_shared_from_this<RestSession>
{

public:
    explicit RestSession(net::any_io_executor ex, ssl::context& ctx, const ConnectionConfig::ConnectionKeys& keys, const RestCallback&& callback, net::thread_pool& threadPool) :
        resolver_(ex),
        stream_(ex, ctx),
        apiKeys_(keys),
        callback_(callback),
        threadPool_(threadPool)
    {
    }


    void run(const string& host, const string& port, const string& target, const int version)
    {
        // Set SNI Hostname (many hosts need this to handshake successfully)
        if (!SSL_set_tlsext_host_name(stream_.native_handle(), host.c_str()))
        {
            beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
            fail(ec, "SNI hostname");
        }
        else
        {
            // Set up an HTTP GET request message
            req_.version(version);
            req_.method(http::verb::get);
            req_.target(target);
            req_.set(http::field::host, host);
            req_.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
            req_.insert("X-MBX-APIKEY", apiKeys_.api);
            
            // Look up the domain name
            resolver_.async_resolve(host, port, beast::bind_front_handler(&RestSession::on_resolve,shared_from_this()));
        }
    }


    void on_resolve(beast::error_code ec, tcp::resolver::results_type results)
    {
        if (ec)
            return fail(ec, "resolve");

        // Set a timeout on the operation
        beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

        // Make the connection on the IP address we get from a lookup
        beast::get_lowest_layer(stream_).async_connect(results, beast::bind_front_handler(&RestSession::on_connect,shared_from_this()));
    }


    void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type)
    {
        if (ec)
            return fail(ec, "connect");

        // Perform the SSL handshake
        stream_.async_handshake( ssl::stream_base::client, beast::bind_front_handler(&RestSession::on_handshake, shared_from_this()));
    }


    void on_handshake(beast::error_code ec)
    {        
        if (ec)
            return fail(ec, "handshake");
        
        // Set a timeout on the operation
        beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

        // Send the HTTP request to the remote host
        http::async_write(stream_, req_, beast::bind_front_handler(&RestSession::on_write, shared_from_this()));
    }


    void on_write(beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return fail(ec, "write");

        // Receive the HTTP response
        http::async_read(stream_, buffer_, res_, beast::bind_front_handler(&RestSession::on_read, shared_from_this()));
    }


    void on_read(beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
        {
            return fail(ec, "read");
        }

        if (res_[http::field::content_type] == "application/json")
        {
            json::error_code ec;

            if (auto value = json::parse(res_.body(), ec); ec)
            {
                fail(ec, "json read");
            }
            else
            {                
                if (callback_)
                {
                    RestResult result {std::move(value)};
                    net::post(threadPool_, boost::bind(callback_, std::move(result)));
                }
            }            
        }

        // Set a timeout on the operation
        //beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));
        // Gracefully close the stream
        //stream_.async_shutdown(beast::bind_front_handler(&RestSession::on_shutdown,shared_from_this()));
    }


    void on_shutdown(beast::error_code ec)
    {
        if (ec == net::error::eof)
        {
            // Rationale:
            // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
            ec = {};
        }

        if (ec)
            return fail(ec, "shutdown");

        // If we get here then the connection is closed gracefully
    }


private:
    void fail(beast::error_code ec, const string what)
    {
        if (callback_)
        {
            net::post(threadPool_, boost::bind(callback_, RestResult {std::move(what)})); // call with RestResult with a Failed state
        }
        else
        {
            std::cerr << what << ": " << ec.category().name() << " : " << ec.message() << "\n";    
        }        
    }


private:
    tcp::resolver resolver_;
    beast::ssl_stream<beast::tcp_stream> stream_;
    beast::flat_buffer buffer_;     // must persist between reads
    http::request<http::string_body> req_;
    http::response<http::string_body> res_;
    ConnectionConfig::ConnectionKeys apiKeys_;
    RestCallback callback_;
    net::thread_pool& threadPool_;
};


class BinanceBeast
{
public:
    BinanceBeast() ;
    ~BinanceBeast();

    void start(const ConnectionConfig& config);
    

    // REST calls
    void ping ();
    void exchangeInfo(RestCallback rr);
    void serverTime(RestCallback rr);

    void orderBook(RestCallback rr, RestParams params);
    void allOrders(RestCallback rr, RestParams params);


private:
    void createRestSession(const string& host, const string& path, const bool createStrand, RestCallback&& rr,  const bool sign = false, RestParams params = RestParams{})
    {
        std::shared_ptr<RestSession> session;

        if (createStrand)
        {
            session = std::make_shared<RestSession>(net::make_strand(m_restIoc), *m_restCtx, m_config.keys, std::move(rr), m_restThreadPool);
        }
        else
        {
            session = std::make_shared<RestSession>(m_restIoc.get_executor(), *m_restCtx, m_config.keys, std::move(rr), m_restThreadPool);
        }

        // we don't need to worry about the session's lifetime because RestSession::run() passes the session's shared_ptr
        // by value into the io_context. The session will be destroyed when there are no more io operations pending.

        if (!params.queryParams.empty())
        {
            string pathToSend;

            if (sign)
            {
                // signature requires the timestamp and the signature params. The signature value is a SHA256 of the query params except 'signature':
                //  https://fapi.binance.com/fapi/v1/allOrders?symbol=ABCDEF&recvWindow=5000&timestamp=123454
                //                                             ^                                            ^
                //                                          from here                                    to here   

                std::stringstream pathWithParams;
            
                for (auto& param : params.queryParams)
                    pathWithParams << std::move(param.first) << "=" << std::move(param.second) << "&";                

                pathWithParams << "timestamp=" << std::chrono::duration_cast<std::chrono::milliseconds> (std::chrono::system_clock::now().time_since_epoch()).count(); 
                
                auto pathWithoutSig = pathWithParams.str();
                pathToSend = path + "?" + std::move(pathWithoutSig) + "&signature=" + createSignature(m_config.keys.secret, pathWithoutSig);
            }
            else
            {
                std::stringstream pathWithParams;
                pathWithParams << path << "?";

                for (auto& param : params.queryParams)
                    pathWithParams << std::move(param.first) << "=" << std::move(param.second) << "&";                

                pathToSend = std::move(pathWithParams.str());
            }

            session->run(host, "443", pathToSend, 11);    // 11 is HTTP version 1.1
        }
        else
        {
            session->run(host, "443", path, 11); 
        }
    }

  
    inline string b2a_hex(char* byte_arr, int n)
    {
        const static std::string HexCodes = "0123456789abcdef";
        string HexString;
        for (int i = 0; i < n; ++i)
        {
            unsigned char BinValue = byte_arr[i];
            HexString += HexCodes[(BinValue >> 4) & 0x0F];
            HexString += HexCodes[BinValue & 0x0F];
        }
        return HexString;
    }
    
    inline string createSignature(const string& key, const string& data)
    {
        string hash;

        if (unsigned char* digest = HMAC(EVP_sha256(), key.c_str(), static_cast<int>(key.size()), (unsigned char*)data.c_str(), data.size(), NULL, NULL); digest)
        {
            hash = b2a_hex((char*)digest, 32);
        }

        return hash;
    }

private:
    ConnectionConfig m_config;
    net::io_context m_restIoc;  // single io context for REST calls
    std::unique_ptr<net::executor_work_guard<net::io_context::executor_type>> m_restWorkGuard;
    std::unique_ptr<ssl::context> m_restCtx;
    std::unique_ptr<std::thread> m_iocRestThread;
    net::thread_pool m_restThreadPool;
};

}   // namespace BinanceBeast


#endif
