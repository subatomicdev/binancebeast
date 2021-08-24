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


namespace bblib
{
    /// 
    /// REST API docs:  https://binance-docs.github.io/apidocs/futures/en/#market-data-endpoints, 
    ///                 https://binance-docs.github.io/apidocs/futures/en/#account-trades-endpoints
    ///
    /// WebSockets API docs: https://binance-docs.github.io/apidocs/futures/en/#websocket-market-streams
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

    public:
        BinanceBeast() ;
        ~BinanceBeast();


        /// Start the networking event processing.
        /// If you're only REST calls and it's not a call that requires an API key, you can leave the api and secret keys in the config empty.
        /// config - the config, created by ConnectionConfig::MakeTestNetConfig() or ConnectionConfig::MakeLiveConfig().
        /// nRestIoContexts - how many asio::io_context to handle REST calls. Leave as default if unsure.
        /// nWebsockIoContexts - how many asio::io_context to handle websockets. Leave as default if unsure.
        void start(const ConnectionConfig& config, const size_t nRestIoContexts = 4, const size_t nWebsockIoContexts = 6);
        

        // REST calls
        void ping ();
        void exchangeInfo(RestCallback rr);
        void serverTime(RestCallback rr);
        void orderBook(RestCallback rr, RestParams params);
        void allOrders(RestCallback rr, RestParams params);        
        void recentTradesList(RestCallback rr, RestParams params);
        void historicTrades(RestCallback rr, RestParams params);
        void aggregateTradesList(RestCallback rr, RestParams params);        
        void klines(RestCallback rr, RestParams params);
        void contractKlines(RestCallback rr, RestParams params);
        void indexPriceKlines(RestCallback rr, RestParams params);
        void markPriceKlines(RestCallback rr, RestParams params);
        void markPrice(RestCallback rr, RestParams params);
        void fundingRate(RestCallback rr, RestParams params);
        void tickerPriceChange24hr(RestCallback rr, RestParams params);
        void symbolPriceTicker(RestCallback rr, RestParams params);
        void symbolBookTicker(RestCallback rr, RestParams params);
        void openInterest(RestCallback rr, RestParams params);
        void openInterestStats(RestCallback rr, RestParams params);
        void topTraderLongShortRatioAccounts(RestCallback rr, RestParams params);
        void topTraderLongShortRatioPositions(RestCallback rr, RestParams params);
        void longShortRatio(RestCallback rr, RestParams params);
        void takerBuySellVolume(RestCallback rr, RestParams params);
        void historicalBlvtNavKlines(RestCallback rr, RestParams params);
        void compositeIndexSymbolInfo(RestCallback rr, RestParams params);


        // WebSockets
        void monitorMarkPrice (WsCallback wc, string params);
        void monitorKline (WsCallback wc, string params);
        void monitorIndividualSymbolMiniTicker (WsCallback wc, string symbol);
        void monitorAllMarketMiniTickers (WsCallback wc);
        void monitorIndividualSymbolTicker(WsCallback wc, string symbol);
        void monitorSymbolBookTicker(WsCallback wc, string symbol);
        void monitorAllBookTicker(WsCallback wc);
        void monitorUserData(WsCallback wc);


        // Listen Key

        /// You should call this every 60 minutes to extend your listen key, otherwise your user data stream will become invalid/closed by Binance.
        /// You must first call monitorUserData() to create (or reuse exiting) listen key, there after calll this function every ~ 60 minutes.
        void renewListenKey(WsCallback wc);

        /// This will invalidate your key, so you will no longer receive user data updates. Only call if you intend to shutdown.
        void closeUserData (WsCallback wc);

    private:

        void createWsSession (const string& host, const std::string& path, WsCallback&& wc)
        {
            if (wc == nullptr)
                throw std::runtime_error("callback is null");

            std::shared_ptr<WsSession> session = std::make_shared<WsSession>(getWsIoContext(), m_wsCtx, std::move(wc));
            
            session->run(host, "443", path);
        }


        void createRestSession(const string& host, const string& path, const bool createStrand, RestCallback&& rc,  const bool sign = false, RestParams params = RestParams{})
        {
            if (rc == nullptr)
                throw std::runtime_error("callback is null");

            std::shared_ptr<RestSession> session;

            if (createStrand)
                session = std::make_shared<RestSession>(net::make_strand(getRestIoContext()), m_restCtx, m_config.keys, std::move(rc), m_restCallersThreadPool);
            else
                session = std::make_shared<RestSession>(getRestIoContext().get_executor(), m_restCtx, m_config.keys, std::move(rc), m_restCallersThreadPool);

            // we don't need to worry about the session's lifetime because RestSession::run() passes the session's shared_ptr
            // by value into the io_context. The session will be destroyed when there are no more io operations pending.

            if (!params.queryParams.empty())
            {
                string pathToSend;

                if (sign)
                {
                    // signature requires the timestamp and the value is a SHA256 of the query params:
                    // 
                    //  https://fapi.binance.com/fapi/v1/allOrders?symbol=ABCDEF&recvWindow=5000&timestamp=123454
                    //                                             ^                                            ^
                    //                                          from here                                    to here   
                    // the "&signature=123456456565672565624" is appended

                    std::ostringstream pathWithParams;
                
                    for (auto& param : params.queryParams)
                        pathWithParams << std::move(param.first) << "=" << std::move(param.second) << "&";                

                    pathWithParams << "timestamp=" << std::chrono::duration_cast<std::chrono::milliseconds> (std::chrono::system_clock::now().time_since_epoch()).count(); 
                    
                    auto pathWithoutSig = pathWithParams.str();
                    pathToSend = path + "?" + std::move(pathWithoutSig) + "&signature=" + createSignature(m_config.keys.secret, pathWithoutSig);
                }
                else
                {
                    std::ostringstream pathWithParams;
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


        net::io_context& getWsIoContext();

        net::io_context& getRestIoContext();

    
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
        enum class UserDataStreamMode { Create, Extend, Close };

        bool amendUserDataListenKey (WsCallback wc, const UserDataStreamMode mode)
        {
            net::io_context ioc;

            tcp::resolver resolver(ioc);
            beast::ssl_stream<beast::tcp_stream> stream(ioc, *m_wsCtx);

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

            http::request<http::string_body> req{requestVerb, "/fapi/v1/listenKey", 11};
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
                
            if (res[http::field::content_type] == "application/json")
            {
                json::error_code ec;
                if (auto value = json::parse(res.body(), ec); ec)
                {
                    fail(ec, "monitorUserData(): json read", wc);
                }
                else
                {
                    // for creating, we store the listen key, for other modes just check for error

                    if (value.as_object().if_contains("code"))
                        fail (string{"monitorUserData(): json contains error code from Binance: " + json::value_to<string>(value.as_object()["msg"])}, wc);
                    else if (mode == UserDataStreamMode::Create)  
                        m_listenKey = json::value_to<string>(value.as_object()["listenKey"]);
                }
            }
            else
            {
                fail("monitorUserData(): content not json", wc);
            }          

            return !m_listenKey.empty();
        }

    private:

        ConnectionConfig m_config;
        string m_listenKey;


        // REST
        std::shared_ptr<ssl::context> m_restCtx;
        net::thread_pool m_restCallersThreadPool;  // The users's callback functions are called from this pool rather than using the io_context's thread
        std::vector<IoContext> m_restIocThreads;
        std::atomic_size_t m_nextRestIoContext;

        // WebSockets
        std::shared_ptr<ssl::context> m_wsCtx;          // TODO do we need different ssl contexts for Rest and WS?
        std::vector<IoContext> m_wsIocThreads;
        std::atomic_size_t m_nextWsIoContext;
    };

}   // namespace BinanceBeast


#endif
