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


        // WebSockets
        void monitorMarkPrice (WsCallback wc, string params);
        void monitorKline (WsCallback wc, string params);
        void monitorIndividualSymbolMiniTicker (WsCallback wc, string symbol);
        void monitorAllMarketMiniTickers (WsCallback wc);
        void monitorIndividualSymbolTicker(WsCallback wc, string symbol);
        void monitorSymbolBookTicker(WsCallback wc, string symbol);
        void monitorAllBookTicker(WsCallback wc);
        

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
                session = std::make_shared<RestSession>(net::make_strand(m_restIoc), *m_restCtx, m_config.keys, std::move(rc), m_restThreadPool);
            }
            else
            {
                session = std::make_shared<RestSession>(m_restIoc.get_executor(), *m_restCtx, m_config.keys, std::move(rc), m_restThreadPool);
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
        ConnectionConfig m_config;

        // REST
        net::io_context m_restIoc;  // single io context for REST calls
        std::unique_ptr<net::executor_work_guard<net::io_context::executor_type>> m_restWorkGuard;
        std::unique_ptr<ssl::context> m_restCtx;
        std::unique_ptr<std::thread> m_restIocThread;
        //TODO find out if this a good idea
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
