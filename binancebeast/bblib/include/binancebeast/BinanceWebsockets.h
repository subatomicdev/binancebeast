#ifndef BINANCEBEAST_WS_H
#define BINANCEBEAST_WS_H

#include "BinanceCommon.h"
#include <sstream>
#include <ordered_thread_pool.h>


namespace bblib
{
    struct WsResult
    {
        enum class State { Fail, Success };

        WsResult (const State s) : state(s)
        {

        }

        WsResult (std::string_view failReason) : state(State::Fail), failMessage(failReason)
        {

        }

        WsResult(json::value&& object) : json (std::move(object)), state(State::Success)
        {

        }

        /// Check if there's an error in the response
        bool hasErrorCode(bool isNullAllowed = false)
        {
            bool error = false;
            try
            {
                if (!isNullAllowed && json.is_null())
                    error = "null";
                else if (json.is_object())
                    error = json.as_object().if_contains("code") || json.as_object().if_contains("error");
            }
            catch(...)
            {        
                error = true;    
            }
            
            if (error)
                state = State::Fail;

            return error;
        }

        json::value json;
        State state;
        string failMessage;
    };


    using WsCallback = std::function<void(WsResult)>;
    

    /// Manages a websocket client session, from initial connection until disconnect.
    /// The websocket data (json) is sent via a WsResult object to the supplied callback handler.
    /// The handler is called from a thread pool, implemented so handlers are called in-order.
    class WsSession : public std::enable_shared_from_this<WsSession>
    {

    public:
    
        // Resolver and socket require an io_context
        explicit WsSession(net::io_context& ioc, std::shared_ptr<ssl::context> ctx, WsCallback&& callback)
            :   m_resolver(net::make_strand(ioc)),
                m_ws(net::make_strand(ioc), *ctx),
                m_callback(std::move(callback)),
                m_sslContext(ctx)
        {
            m_handlersPool = std::make_unique<OrderedThreadPool<WsResult>> (4,4); // 4 threads and allow 4 queued functions before blocking
        }


        ~WsSession()
        {
            // we don't want to use close() because thats mixing sync with async calls.
            // we don't do an async_close() because shared_from_this() will refer to this object after being destructed
            // we let beast handle it
        }


        /// Start the websocket session:
        ///     - resolve the address
        ///     - connect
        ///     - ssl handshake
        ///     - read until stream closed by the server or object destruction
        void run(const string& host, const string& port, const string& path)
        {
            m_host = host;
            m_path = path;

            // Look up the domain name
            m_resolver.async_resolve(host, port, beast::bind_front_handler(&WsSession::on_resolve,shared_from_this()));
        }


        void on_resolve(beast::error_code ec, tcp::resolver::results_type results)
        {
            if(ec)
                return fail(ec, "resolve", m_callback);

            // Set a timeout on the operation
            beast::get_lowest_layer(m_ws).expires_after(std::chrono::seconds(30));

            // Make the connection on the IP address we get from a lookup
            beast::get_lowest_layer(m_ws).async_connect(results,beast::bind_front_handler(&WsSession::on_connect, shared_from_this()));
        }


        void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep)
        {
            if(ec)
                return fail(ec, "connect", m_callback);

            // Update the m_host string. This will provide the value of the host HTTP header during the WebSocket handshake.
            // See https://tools.ietf.org/html/rfc7230#section-5.4
            m_host += ':' + std::to_string(ep.port());

            beast::get_lowest_layer(m_ws).expires_after(std::chrono::seconds(10));

            // SNI Hostname (many hosts need this to handshake successfully)
            if(!SSL_set_tlsext_host_name(m_ws.next_layer().native_handle(),m_host.c_str()))
            {
                ec = beast::error_code(static_cast<int>(::ERR_get_error()),net::error::get_ssl_category());
                return fail(ec, "connect", m_callback);
            }

            // perform the SSL handshake
            m_ws.next_layer().async_handshake(ssl::stream_base::client,beast::bind_front_handler(&WsSession::on_ssl_handshake,shared_from_this()));
        }


        void on_ssl_handshake(beast::error_code ec)
        {
            if(ec)
                return fail(ec, "ssl handshake", m_callback);

            // disable the timeout on the underlying tcp_stream because the websocket stream has its own timeout system
            beast::get_lowest_layer(m_ws).expires_never();

            // set the websocket stream timeouts 
            m_ws.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));

            // set a decorator to change the User-Agent of the handshake
            m_ws.set_option(websocket::stream_base::decorator(
            [](websocket::request_type& req)
            {
                req.set(http::field::user_agent, BINANCEBEAST_USER_AGENT);
            }));

            m_ws.async_handshake(m_host, m_path, beast::bind_front_handler(&WsSession::on_handshake,shared_from_this()));
        }


        void on_handshake(beast::error_code ec)
        {
            if(ec)
                return fail(ec, "handshake", m_callback);

            // begin reading. called repeatedly
            m_ws.async_read(m_buffer, beast::bind_front_handler(&WsSession::on_read,shared_from_this()));
        }


        void on_read(beast::error_code ec, std::size_t bytes_transferred)
        {
            boost::ignore_unused(bytes_transferred);

            if (ec == net::error::shut_down)
                return ;

            if (ec)
                return fail(ec, "read", m_callback);

            json::error_code jsonEc;
            if (auto jsonValue = json::parse(beast::buffers_to_string(m_buffer.cdata()), jsonEc); jsonEc)
            {
                fail(jsonEc, "json read", m_callback);
            }
            else
            {
                WsResult result {std::move(jsonValue)};
                m_handlersPool->Do(m_callback, std::move(result));
            }
            
            m_buffer.clear();
            m_ws.async_read(m_buffer, beast::bind_front_handler(&WsSession::on_read,shared_from_this()));
        }
        

    private:
        tcp::resolver m_resolver;
        websocket::stream<beast::ssl_stream<beast::tcp_stream>> m_ws;
        beast::flat_buffer m_buffer;
        std::string m_host;
        std::string m_path;
        WsCallback m_callback;
        std::shared_ptr<ssl::context> m_sslContext;
        std::unique_ptr<OrderedThreadPool<WsResult>> m_handlersPool;
    };
}


#endif
