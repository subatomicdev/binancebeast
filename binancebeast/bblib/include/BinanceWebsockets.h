#ifndef BINANCEBEAST_WS_H
#define BINANCEBEAST_WS_H

#include "BinanceCommon.h"
#include <sstream>


namespace bblib
{
    struct WsResult
    {
        enum class State { Fail, Success };

        WsResult (string failReason) : state(State::Fail), failMessage(failReason)
        {

        }

        WsResult(json::value&& object) : json (std::move(object)), state(State::Success)
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


    struct WsParams
    {
        using QueryParams = std::unordered_map<string, string>;

        WsParams () = default;
        
        WsParams (QueryParams&& params) : queryParams(std::move(params))
        {
        }

        WsParams (const QueryParams& params) : queryParams(params)
        {
        }


        QueryParams queryParams;
    };


    using WsCallback = std::function<void(WsResult)>;

    // Manages a websocket client session, from initial connection until disconnect.
    class WsSession : public std::enable_shared_from_this<WsSession>
    {

    public:
    
        // Resolver and socket require an io_context
        explicit WsSession(net::io_context& ioc, std::shared_ptr<ssl::context> ctx, const WsCallback&& callback, net::thread_pool& threadPool)
            :   resolver_(net::make_strand(ioc)),
                ws_(net::make_strand(ioc), *ctx),
                callback_(std::move(callback)),
                threadPool_(threadPool),
                sslContext_(ctx)
        {
        }

        ~WsSession()
        {
            // we don't want to use close() because thats mixing sync with async calls.
            // we don't do an async_close() because shared_from_this() will refer to this object after being destructed
            // we let beast handle it
        }



        // Start the asynchronous operation
        void run(const string& host, const string& port, const string& path)
        {
            host_ = host;
            path_ = path;

            // Look up the domain name
            resolver_.async_resolve(host, port, beast::bind_front_handler(&WsSession::on_resolve,shared_from_this()));
        }


        void on_resolve(beast::error_code ec, tcp::resolver::results_type results)
        {
            if(ec)
                return fail(ec, "resolve");

            // Set a timeout on the operation
            beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));

            // Make the connection on the IP address we get from a lookup
            beast::get_lowest_layer(ws_).async_connect(results,beast::bind_front_handler(&WsSession::on_connect, shared_from_this()));
        }


        void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep)
        {
            if(ec)
                return fail(ec, "connect");

            // Update the host_ string. This will provide the value of the
            // Host HTTP header during the WebSocket handshake.
            // See https://tools.ietf.org/html/rfc7230#section-5.4
            host_ += ':' + std::to_string(ep.port());

            // Set a timeout on the operation
            beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));

            // Set SNI Hostname (many hosts need this to handshake successfully)
            if(! SSL_set_tlsext_host_name(ws_.next_layer().native_handle(),host_.c_str()))
            {
                ec = beast::error_code(static_cast<int>(::ERR_get_error()),net::error::get_ssl_category());
                return fail(ec, "connect");
            }

            // Perform the SSL handshake
            ws_.next_layer().async_handshake(ssl::stream_base::client,beast::bind_front_handler(&WsSession::on_ssl_handshake,shared_from_this()));
        }


        void on_ssl_handshake(beast::error_code ec)
        {
            if(ec)
                return fail(ec, "ssl_handshake");

            // Turn off the timeout on the tcp_stream, because
            // the websocket stream has its own timeout system.
            beast::get_lowest_layer(ws_).expires_never();

            // Set suggested timeout settings for the websocket
            ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));

            // Set a decorator to change the User-Agent of the handshake
            ws_.set_option(websocket::stream_base::decorator(
            [](websocket::request_type& req)
            {
                req.set(http::field::user_agent, BINANCEBEAST_USER_AGENT);
            }));

            // Perform the websocket handshake
            ws_.async_handshake(host_, path_, beast::bind_front_handler(&WsSession::on_handshake,shared_from_this()));
        }


        void on_handshake(beast::error_code ec)
        {
            if(ec)
                return fail(ec, "handshake");

            // Read a message into our buffer
            ws_.async_read(buffer_, beast::bind_front_handler(&WsSession::on_read,shared_from_this()));
        }


        void on_read(beast::error_code ec, std::size_t bytes_transferred)
        {
            boost::ignore_unused(bytes_transferred);

            if (ec == net::error::shut_down)
                return ;

            if (ec)
                return fail(ec, "read");

            json::error_code jsonEc;
            if (auto value = json::parse(beast::buffers_to_string(buffer_.cdata()), jsonEc); jsonEc)
            {
                fail(jsonEc, "json read");
            }
            else
            {   
                if (callback_)
                {
                    WsResult result {std::move(value)};
                    net::post(threadPool_, boost::bind(callback_, std::move(result)));
                }
            }
            
            buffer_.clear();
            ws_.async_read(buffer_, beast::bind_front_handler(&WsSession::on_read,shared_from_this()));
        }


    private:
        void fail(beast::error_code ec, char const* what)
        {
            std::cerr << what << ": " << ec.message() << "\n";
        }

    private:
        tcp::resolver resolver_;
        websocket::stream<beast::ssl_stream<beast::tcp_stream>> ws_;
        beast::flat_buffer buffer_;
        std::string host_;
        std::string path_;
        WsCallback callback_;
        net::thread_pool& threadPool_;
        std::shared_ptr<ssl::context> sslContext_;
    };
}


#endif
