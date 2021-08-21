#ifndef BINANCEBEAST_H
#define BINANCEBEAST_H


#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <thread>


namespace bblib
{

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>


using std::string;


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


public:
    ConnectionConfig() : verifyPeer(false)
    {
    }


private:
    ConnectionConfig (const string& restUri, const string& wsUri, const bool sslVerifyPeer) : restApiUri(restUri), wsApiUri(wsUri), verifyPeer(sslVerifyPeer)
    {
        
    }

    
public:
    string restApiUri;
    string wsApiUri;
    bool verifyPeer;
};


class RestSession : public std::enable_shared_from_this<RestSession>
{

public:
    explicit RestSession(net::any_io_executor ex, ssl::context& ctx) : resolver_(ex) , stream_(ex, ctx)
    {
    }


    void run(const string& host, const string& port, const string& target, const int version)
    {
        // Set SNI Hostname (many hosts need this to handshake successfully)
        if (!SSL_set_tlsext_host_name(stream_.native_handle(), host.c_str()))
        {
            beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
            std::cerr << ec.message() << "\n";
            return;
        }

        // Set up an HTTP GET request message
        req_.version(version);
        req_.method(http::verb::get);
        req_.target(target);
        req_.set(http::field::host, host);
        req_.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

        // Look up the domain name
        resolver_.async_resolve(host, port, beast::bind_front_handler(&RestSession::on_resolve,shared_from_this()));
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

        if(ec)
            return fail(ec, "read");

        // Write the message to standard out
        std::cout << res_ << std::endl;

        // Set a timeout on the operation
        beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

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
    void fail(beast::error_code ec, char const* what)
    {
        std::cerr << what << ": " << ec.category().name() << " : " << ec.message() << "\n";
    }


private:
    tcp::resolver resolver_;
    beast::ssl_stream<beast::tcp_stream> stream_;
    beast::flat_buffer buffer_;     // must persist between reads
    http::request<http::string_body> req_;
    http::response<http::string_body> res_;
};


class BinanceBeast
{
public:
    BinanceBeast() ;
    ~BinanceBeast();

    void start(const ConnectionConfig& config);


    // REST calls
    void ping ();
    void exchangeInfo();

private:
    void createRestSession(const string& host, const string& path, const bool createStrand = false)
    {
        std::shared_ptr<RestSession> session;

        if (createStrand)
        {
            session = std::make_shared<RestSession>(net::make_strand(m_restIoc), *m_restCtx);
        }
        else
        {
            session = std::make_shared<RestSession>(m_restIoc.get_executor(), *m_restCtx);
        }
        
        session->run(host, "443", path, 11);
    }

private:
    ConnectionConfig m_config;
    net::io_context m_restIoc;  // single io context for REST calls
    std::unique_ptr<net::executor_work_guard<net::io_context::executor_type>> m_restWorkGuard;
    std::unique_ptr<ssl::context> m_restCtx;
    std::unique_ptr<std::thread> m_iocRestThread;
};

}   // namespace BinanceBeast


#endif
