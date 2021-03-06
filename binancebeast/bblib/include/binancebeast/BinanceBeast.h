#ifndef BINANCEBEAST_H
#define BINANCEBEAST_H


#include "BinanceRest.h"
#include "BinanceWebsockets.h"

#include <openssl/hmac.h>   // to sign query params
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <unordered_map>
#include <sstream>
#include <set>


namespace bblib
{
    enum class RestSign
    {
        Unsigned,
        HMAC_SHA256
    };

    

    /// 
    /// REST API docs:  https://binance-docs.github.io/apidocs/futures/en/#market-data-endpoints, 
    ///                 https://binance-docs.github.io/apidocs/futures/en/#account-trades-endpoints
    ///
    /// WebSockets API docs: https://binance-docs.github.io/apidocs/futures/en/#websocket-market-streams
    ///
    /// See GitHub for more documentation, and the tests and examples directories.
    ///
    /// All REST query are sent with sendRestRequest().
    ///
    /// All websockets are started with startWebSocket() , except user data which is startUserData().
    /// 
    /// startWebSocket() requires a result handler and the stream name, which is on the Binance docs.
    ///
    /// for mark price:  
    ///     binanceBeast.startWebSocket(onWsResponse, "btcusdt@markPrice@1s");
    ///
    /// where binanceBeast is a BinanceBeast object and onWsResponse is the result handler.
    ///
    class BinanceBeast
    {
    private:
        struct IoContext
        {
            IoContext () = default;
            IoContext(IoContext&&) = default;

            IoContext(const IoContext&) = delete;
            IoContext& operator=(const IoContext&) = delete;

            ~IoContext()
            {
                guard.reset();
                ioc->stop();
                iocThread.join();
            }

            void start()
            {
                ioc = std::make_unique<net::io_context>();
                guard = std::make_unique<net::executor_work_guard<net::io_context::executor_type>> (ioc->get_executor());
                iocThread = std::move(std::thread([this]() { ioc->run(); }));
            }
            
            std::unique_ptr<net::io_context> ioc;
            std::thread iocThread;
            std::unique_ptr<net::executor_work_guard<net::io_context::executor_type>> guard;
        };

        enum class UserDataStreamMode
        {
            Create,
            Extend,
            Close
        };


    public:
        BinanceBeast() ;
        ~BinanceBeast();


        /// Start the networking event processing.
        /// If you're only REST calls and it's not a call that requires an API key, you can leave the api and secret keys in the config empty.
        /// config - the config, created by ConnectionConfig::MakeTestNetConfig() or ConnectionConfig::MakeLiveConfig().
        /// nRestIoContexts - how many asio::io_context to handle REST calls. Leave as default if unsure.
        /// nWebsockIoContexts - how many asio::io_context to handle websockets. Leave as default if unsure.
        void start(const ConnectionConfig& config, const size_t nRestIoContexts = 4, const size_t nWebsockIoContexts = 6);
        
        /// Send a request to a REST endpoint.
        /// Some requests require a signature, the Binance API docs will say "HMAC SHA256" if so.
        /// For unsigned requests: set 'sign' to RestSign::Unsigned.
        /// For signed rqeuests:   set 'sign' to RestSign::HMAC_SHA256 but DO NOT include a 'timestamp' in the params, BinanceBeast will do that.
        void sendRestRequest(RestResponseHandler&& handler, const string& path, const RestSign sign, const RestParams& params, const RequestType type);
        
        /// As sendRestRequest(RestResponseHandler, const string&, const RestSign, RestParams, const RequestType)
        /// but the path and params are moved.
        void sendRestRequest(RestResponseHandler&& handler, string&& path, const RestSign sign, RestParams&& params, const RequestType type);
        

        /// Start a new websocket session, for all websocket endpoints except user data (use startUserData() for that).
        /// The supplied callback handler will be called for each response, which may include an error.
        ///
        /// Warning: if stream does not exist, Binance does not report this. Instead no data is pushed.
        ///
        /// 'stream' is the "streamName" as defined on the Binance API docs.
        WsToken startWebSocket (WebSocketResponseHandler handler, const string& stream);

        /// This starts a combined stream, for example receiving mark price for two different symbols without having to separate calls
        /// to startWebSocket(), and two response handlers, you can combine both into one stream.
        ///
        /// Warning: if stream does not exist, Binance does not report this. Instead no data is pushed.
        ///
        /// See https://binance-docs.github.io/apidocs/futures/en/#websocket-market-streams 
        WsToken startWebSocket (WebSocketResponseHandler handler, const std::vector<string>& streams);

        /// This starts a combined stream, for example receiving mark price for two different symbols without having to separate calls
        /// to startWebSocket(), and two response handlers, you can combine both into one stream.
        ///
        /// Warning: if stream does not exist, Binance does not report this. Instead no data is pushed.
        ///
        /// See https://binance-docs.github.io/apidocs/futures/en/#websocket-market-streams 
        WsToken startWebSocket (WebSocketResponseHandler handler, const std::set<string>& streams);

        /// Closes a websocket connection, including user data stream.
        /// token - the token, as returned from startWebSocket() or startUserData().
        /// handler - will be called when the stream is closed. The WebSocketResponseHandler::state will be State::Disconnect.
        ///           If nullptr then the existing handler (passed to startWebSocket()  / startUserData()) will be used.
        void stopWebSocket (const WsToken& token, WebSocketResponseHandler handler = nullptr);


        /// Start a user data websocket session.
        WsToken startUserData(WebSocketResponseHandler handler, const string_view stream);

        /// You should call this every 60 minutes to extend your listen key, otherwise your user data stream will become invalid/closed by Binance.
        /// You must first call monitorUserData() to create (or reuse exiting) listen key, there after calll this function every ~ 60 minutes.
        void renewListenKey(WebSocketResponseHandler handler, const string_view stream);

        /// This will invalidate your key, so you will no longer receive user data updates. 
        /// This does not close the web socket session, for that use stopWebSocket().
        void closeUserData (WebSocketResponseHandler handler, const string_view stream);


        /// Load PEM file with root certificates. Use this in production, but for test/dev then the default certificate is likely ok.
        /// Call this before start().
        void loadRootCertificate (std::filesystem::path& path)
        {
            if (!std::filesystem::exists(path))
                fail("path to root certificate does not exist");
            else
            {
                m_sslCtx->load_verify_file(path);                               
            }
        }

        /// Add a directory containing certificate authority files used for HTTPS verification. Must be PEM format.
        /// Call this before start().
        void addRootVerifyPath(const std::filesystem::path& path)
        {
            m_sslCtx->add_verify_path(path);
        }
        

        /// Encode the string as a URL. Used for calls to the batchOrder, see the neworder example.
        static std::string urlEncode (const string_view& s)  
        {
            std::ostringstream os;

            for ( std::string_view::const_iterator ci = s.cbegin(); ci != s.cend(); ++ci )
            {
                if ( (*ci >= 'a' && *ci <= 'z') ||
                     (*ci >= 'A' && *ci <= 'Z') ||
                     (*ci >= '0' && *ci <= '9') )
                { // allowed
                    os << *ci;
                }
                else if ( *ci == ' ')
                {
                    os << '+';
                }
                else
                {
                    os << '%' << to_hex(*ci >> 4) << to_hex(*ci % 16);
                }
            }

            return os.str();
        }

        

    private:
       
        void stop();


        WsToken createWsSession (const string& host, const std::string& path, WebSocketResponseHandler&& handler);


        inline void createRestSession(const string& host, const string& path, const bool createStrand, RestResponseHandler&& rc,  const bool sign, RestParams params, const RequestType type = RequestType::Get);


        inline net::io_context& getWsIoContext() noexcept;
        

        inline net::io_context& getRestIoContext() noexcept;


        inline string createSignature(const string& key, const string& data) noexcept
        {
            string hash;

            if (unsigned char* digest = HMAC(EVP_sha256(), key.c_str(), static_cast<int>(key.size()), (unsigned char*)data.c_str(), data.size(), NULL, NULL); digest)
            {
                hash = b2a_hex((char*)digest, 32);
            }

            return hash;
        }


        bool amendUserDataListenKey (WebSocketResponseHandler handler, const UserDataStreamMode mode, const string_view streamName)
        {
            net::io_context ioc;

            tcp::resolver resolver(ioc);
            beast::ssl_stream<beast::tcp_stream> stream(ioc, *m_sslCtx);

            // Set SNI Hostname (many hosts need this to handshake successfully)
            if(! SSL_set_tlsext_host_name(stream.native_handle(), m_config.restApiUri.c_str()))
            {
                beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
                throw beast::system_error{ec};
            }

            // Look up the domain name
            auto const results = resolver.resolve(m_config.restApiUri, "443");

            // Make the connection on the IP address we get from a lookup
            beast::get_lowest_layer(stream).connect(results);

            // Perform the SSL handshake
            stream.handshake(ssl::stream_base::client);

            // Set up an HTTP GET request message
            http::verb requestVerb;
            switch (mode)
            {
                case UserDataStreamMode::Create:
                    requestVerb = http::verb::post;
                break;

                case UserDataStreamMode::Extend:
                    requestVerb = http::verb::put;
                break;

                case UserDataStreamMode::Close:
                    requestVerb = http::verb::delete_;
                break;
            }

            http::request<http::string_body> req{requestVerb, string{streamName}, 11};
            req.set(http::field::host, m_config.restApiUri);
            req.set(http::field::user_agent, BINANCEBEAST_USER_AGENT);
            req.insert("X-MBX-APIKEY", m_config.keys.api);

            // Send the HTTP request to the remote host
            http::write(stream, req);

            // This buffer is used for reading and must be persisted
            beast::flat_buffer buffer;

            // Declare a container to hold the response
            http::response<http::string_body> res;

            // Receive the HTTP response
            http::read(stream, buffer, res);

            // Gracefully close the stream
            beast::error_code ec;
            stream.shutdown(ec);


            // when creating stream, a listen key is returned, otherwise nothing is returned
            if (mode == UserDataStreamMode::Create)
                m_listenKey.clear();

            if (res[http::field::content_type] == "application/json" || res[http::field::content_type] == "application/json;charset=UTF-8")
            {
                json::error_code ec;
                if (auto value = json::parse(res.body(), ec); ec)
                {
                    fail(ec, "amendUserDataListenKey(): json read", handler);
                }
                else
                {
                    // when creating, we store the listen key, for other modes just check for error
                    if (value.as_object().if_contains("code"))
                        fail (string{"amendUserDataListenKey(): json contains error code from Binance: " + json::value_to<string>(value.as_object()["msg"])}, handler);
                    else if (mode == UserDataStreamMode::Create)  
                        m_listenKey = json::value_to<string>(value.as_object()["listenKey"]);
                }
            }
            else
            {
                fail("amendUserDataListenKey(): content not json", handler);
            }          

            return !m_listenKey.empty();
        }


        inline string b2a_hex(char* byte_arr, const int n) noexcept
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


        static unsigned char to_hex (const unsigned char x)
        {
            return x + (x > 9 ? ('A'-10) : '0');
        }


    private:

        ConnectionConfig m_config;
        string m_listenKey;
        std::shared_ptr<ssl::context> m_sslCtx;

        // REST
        net::thread_pool m_restCallersThreadPool;       // The users's callback functions are called from this pool rather than using the io_context's thread
        std::vector<IoContext> m_restIocThreads;
        std::atomic_size_t m_nextRestIoContext;

        // WebSockets
        std::vector<IoContext> m_wsIocThreads;
        std::atomic_size_t m_nextWsIoContext;
        std::atomic_uint32_t m_nextWsId;
        std::map<WsToken::TokenId, std::shared_ptr<WsSession>> m_wsSessions;
        std::mutex m_wsSessionsMux;
    };

}   // namespace BinanceBeast


#endif
