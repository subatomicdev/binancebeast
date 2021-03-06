#ifndef BINANCEBEAST_WS_H
#define BINANCEBEAST_WS_H

#include "BinanceCommon.h"
#include <sstream>
#include <ordered_thread_pool.h>


namespace bblib
{
    struct WsResponse
    {
        enum class State { Fail, Success, Disconnect };

        WsResponse (const State s) : state(s)
        {

        }

        WsResponse (std::string_view failReason) : state(State::Fail), failMessage(failReason)
        {

        }

        WsResponse(json::value&& object) : json (std::move(object)), state(State::Success)
        {

        }

        /// Check if there's an error in the response
        bool hasErrorCode(bool isNullAllowed = false)
        {
            bool error = false;
            if (state != State::Disconnect)
            {
                try
                {
                    if (!isNullAllowed && json.is_null())
                        error = "json is null/empty";
                    else if (json.is_object())
                    {
                        auto& jsonObj = json.as_object();
                        error = jsonObj.if_contains("code") || jsonObj.if_contains("error");
                        
                        if (jsonObj.if_contains("code"))
                        {
                            failMessage = (jsonObj.if_contains("msg") ? json::value_to<string>(jsonObj["msg"]) : "");
                        }
                    }
                }
                catch(...)
                {        
                    error = true;    
                }

                state = error ? State::Fail : State::Success;
            }

            return error;
        }

        json::value json;
        State state;
        string failMessage;
    };

    struct WsToken
    {
        using TokenId = std::uint32_t;

        bool isValidId() const { return id != 0; }

        TokenId id = 0;
    };

    using WebSocketResponseHandler = std::function<void(WsResponse)>;
    

    /// Manages a websocket client session, from initial connection until disconnect.
    /// The websocket data (json) is sent via a WsResponse object to the supplied callback handler.
    /// The handler is called from a thread pool, implemented so handlers are called in-order.
    ///
    /// NOTE:   if you pass an invalid stream target, it seems that Binance accepts an upgrade to WebSocket 
    ///         and does not return a HTTP NOT FOUND.
    class WsSession : public std::enable_shared_from_this<WsSession>
    {

    public:
        using CloseConnectionHandler = std::function<void(void)>;
        
        // Resolver and socket require an io_context
        explicit WsSession(net::io_context& ioc, std::shared_ptr<ssl::context> ctx, WebSocketResponseHandler&& callback)
            :   m_resolver(net::make_strand(ioc)),
                m_ws(net::make_strand(ioc), *ctx),
                m_callback(std::move(callback)),
                m_sslContext(ctx)
        {
            // the user's handler is documented as non-reentrant, but we don't want to delay io processing
            // if the handler is still running, so we create a thread pool of 1, and we can queue up to 4 
            // more before we block.
            // NOTE: important: this thread pool gaurantees the handlers are called in the same order as 
            //       as pushed onto the pool
            m_handlersPool = std::make_unique<OrderedThreadPool<WsResponse>> (1, 4) ; 

            // TODO is this actually worthwhile? it allows processing messages from the network sooner,
            //      but they'll still be be blocked until the handler queue is free.
            //      on the other hand, for handlers that can process before the next response is ready, it means
            //      they'll receive their data sooner (than if we always called the handler on this thread)

            // TODO could have a setting allowing the handler to be reentrant - any advantage?

            // TODO consider implications of the websocket stream not using a strand - it means on_read() must be rentrant
            //      aware. That is more complex because the m_buffer is shared between calls, which is not a problem with a strand.
            //      A beast::flat_buffer pool? Hmm ...
        }


        ~WsSession()
        {
            // let beast handle disconnection
        }


        void close (CloseConnectionHandler callback)
        {
            m_ws.async_close(websocket::close_code::normal, beast::bind_front_handler([callback](beast::error_code)
            {
                callback();
            }));
        }


        /// Start the websocket session:
        ///     - resolve the address
        ///     - connect
        ///     - ssl handshake
        ///     - read until stream closed by the server or object destruction
        void run(const string_view& host, const string_view& port, const string_view& path)
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

            // SSL handshake
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
            m_ws.set_option(websocket::stream_base::decorator([](websocket::request_type& req)
            {
                req.set(http::field::user_agent, BINANCEBEAST_USER_AGENT);
            }));

            m_ws.async_handshake(m_host, m_path, beast::bind_front_handler(&WsSession::on_handshake,shared_from_this()));
        }


        void on_handshake(beast::error_code ec)
        {
            if(ec)
                return fail(ec, "handshake", m_callback);
            
            m_ws.async_read(m_buffer, beast::bind_front_handler(&WsSession::on_read, shared_from_this()));
        }


        void on_read(beast::error_code ec, std::size_t/* bytes_transferred*/)
        {
            // operation_aborted: if user calls close() whilst there's a pending async_read() in the event queue            
            if (ec == net::error::shut_down || ec == net::error::operation_aborted)
                return ;
            else if (ec)
                return fail(ec, "read", m_callback);

            json::error_code jsonEc;
            if (auto jsonValue = json::parse(beast::buffers_to_string(m_buffer.cdata()), jsonEc); jsonEc)
                fail(jsonEc, "json read", m_callback);
            else
            {
                WsResponse result {std::move(jsonValue)};
                m_handlersPool->Do(m_callback, std::move(result));
            }
            
            m_buffer.clear();
            m_ws.async_read(m_buffer, beast::bind_front_handler(&WsSession::on_read,shared_from_this()));
        }

        
        WebSocketResponseHandler handler() const
        {
            return m_callback;
        }


    private:
        tcp::resolver m_resolver;
        websocket::stream<beast::ssl_stream<beast::tcp_stream>> m_ws;
        http::response<http::string_body> m_httpRes;
        beast::flat_buffer m_buffer;
        std::string m_host;
        std::string m_path;
        WebSocketResponseHandler m_callback;
        std::shared_ptr<ssl::context> m_sslContext;
        std::unique_ptr<OrderedThreadPool<WsResponse>> m_handlersPool;
        net::io_context m_handlersIoc;
    };
}


#endif
