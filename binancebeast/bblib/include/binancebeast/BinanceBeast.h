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


        // WebSockets: market data
        void monitorMarkPrice (WsCallback wc, string params);
        void monitorKline (WsCallback wc, string params);
        void monitorIndividualSymbolMiniTicker (WsCallback wc, string symbol);
        void monitorAllMarketMiniTickers (WsCallback wc);
        void monitorIndividualSymbolTicker(WsCallback wc, string symbol);
        void monitorSymbolBookTicker(WsCallback wc, string symbol);
        void monitorAllBookTicker(WsCallback wc);
        
        // WebSockets: user data
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
            std::shared_ptr<WsSession> session = std::make_shared<WsSession>(m_wsIoc, m_wsCtx, std::move(wc), m_wsThreadPool);
            
            session->run(host, "443", path);
        }


        void createRestSession(const string& host, const string& path, const bool createStrand, RestCallback&& rc,  const bool sign = false, RestParams params = RestParams{})
        {
            std::shared_ptr<RestSession> session;

            if (createStrand)
            {
                session = std::make_shared<RestSession>(net::make_strand(m_restIoc), m_restCtx, m_config.keys, std::move(rc), m_restThreadPool);
            }
            else
            {
                session = std::make_shared<RestSession>(m_restIoc.get_executor(), m_restCtx, m_config.keys, std::move(rc), m_restThreadPool);
            }

            // we don't need to worry about the session's lifetime because RestSession::run() passes the session's shared_ptr
            // by value into the io_context. The session will be destroyed when there are no more io operations pending.

            if (!params.queryParams.empty())
            {
                string pathToSend;

                if (sign)
                {
                    // signature requires the timestamp and the signature params.
                    // the signature value is a SHA256 of the query params:
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
        net::io_context m_restIoc;  // single io context for REST calls
        std::unique_ptr<net::executor_work_guard<net::io_context::executor_type>> m_restWorkGuard;
        std::shared_ptr<ssl::context> m_restCtx;
        std::unique_ptr<std::thread> m_restIocThread;
        net::thread_pool m_restThreadPool;  // The users's callback functions are called from this pool rather than using the io_context's thread


        // WebSockets
        net::io_context m_wsIoc; 
        std::unique_ptr<net::executor_work_guard<net::io_context::executor_type>> m_wsWorkGuard;
        std::shared_ptr<ssl::context> m_wsCtx;          // TODO do we need different ssl contexts for Rest and WS?
        std::unique_ptr<std::thread> m_wsIocThread;     // TODO think about a thread_pool of these, distributing work as roundrobin 
        net::thread_pool m_wsThreadPool;
    };

}   // namespace BinanceBeast


#endif
