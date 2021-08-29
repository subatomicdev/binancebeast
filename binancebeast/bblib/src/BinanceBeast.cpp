#include <binancebeast/BinanceBeast.h>
#include <binancebeast/SslCertificates.h>

#include <functional>


namespace bblib
{
    BinanceBeast::BinanceBeast() : m_nextWsIoContext(0), m_nextRestIoContext(0)
    {
        m_nextWsId.store(1);
        m_sslCtx = std::make_shared<ssl::context> (ssl::context::tlsv12_client);
    }


    BinanceBeast::~BinanceBeast()
    {
        stop();
    }


    void BinanceBeast::stop()
    {
        {
            std::scoped_lock (m_wsSessionsMux);
            m_wsSessions.clear();
        }

        // stop all io_context processing
        m_wsIocThreads.clear();
        m_restIocThreads.clear();
        m_nextWsId.store(0);
    }


    void BinanceBeast::start (const ConnectionConfig& config, const size_t nRestIoContexts, const size_t nWebsockIoContexts)
    {  
        m_nextWsIoContext = 0;
        m_nextRestIoContext = 0;
        m_config = config;        

        boost::system::error_code ec;

        // using certificates shipped with Beast. Do not do this for production. Use loadRootCertificate()
        if (config.usingTestRootCertificates)
        {
            load_test_certificates(*m_sslCtx, ec);
            
            if (ec)
                fail(ec, "failed to load root certificates");
        }
            
        
        // if this enabled on the testnet, it fails validation.
        // using some online tools shows the testnet does not send the root certificate, this maybe the cause of the problem
        if (m_config.verifyPeer)
            m_sslCtx->set_verify_mode(ssl::verify_peer); 
        else
            m_sslCtx->set_verify_mode(ssl::verify_none);


        // rest io_contexts
        m_nextRestIoContext.store(0);

        m_restIocThreads.resize(std::max<size_t>(0, std::min<size_t>(nRestIoContexts, 24)));   // clamp for sanity
        for (auto& ioc : m_restIocThreads)
            ioc.start();


        // websocket io_contexts
        m_nextWsIoContext.store(0);

        m_wsIocThreads.resize(std::max<size_t>(0, std::min<size_t>(nWebsockIoContexts, 24)));   // clamp for sanity
        for (auto& ioc : m_wsIocThreads)
            ioc.start();
    }


    net::io_context& BinanceBeast::getWsIoContext() noexcept
    {
        size_t curr = m_nextWsIoContext.load();
        size_t next = curr + 1 < m_wsIocThreads.size() ? curr + 1 : 0;

        m_nextWsIoContext.store(next);
        return *m_wsIocThreads[curr].ioc;
    }

    
    net::io_context& BinanceBeast::getRestIoContext() noexcept
    {
        size_t curr = m_nextRestIoContext.load();
        size_t next = curr + 1 < m_restIocThreads.size() ? curr + 1 : 0;

        m_nextRestIoContext.store(next);
        return *m_restIocThreads[curr].ioc;
    }


    void BinanceBeast::sendRestRequest(RestResponseHandler handler, const string& path, const RestSign sign, RestParams params, const RequestType type)
    {
        createRestSession(m_config.restApiUri, path, true, std::move(handler), sign == RestSign::HMAC_SHA256, std::move(params), type);
    }


    WsToken BinanceBeast::startWebSocket (WebSocketResponseHandler handler, string streamName)
    {
        return createWsSession(m_config.wsApiUri, std::move("/ws/"+std::move(streamName)), std::move(handler));
    }


    WsToken BinanceBeast::startWebSocket (WebSocketResponseHandler handler, const std::vector<string>& streams)
    {
        std::stringstream target ;
        
        for (auto& stream : streams)
            target << stream + "/";

        return createWsSession(m_config.wsApiUri, std::move("/stream?streams="+std::move(target.str())), std::move(handler));
    }


    void BinanceBeast::stopWebSocket (const WsToken& token, WebSocketResponseHandler handler)
    {
        if (auto sessionIt = m_wsSessions.find(token.id); sessionIt != m_wsSessions.end())
        {                
            m_wsSessions[token.id]->close([token, handler, this, session = m_wsSessions[token.id]]()
            {                    
                auto cb = handler ? handler : session->handler();

                {
                    std::scoped_lock (m_wsSessionsMux);
                    m_wsSessions.erase(token.id);
                }

                cb(WsResponse {WsResponse::State::Disconnect});                    
            });                
        }
    }


    void BinanceBeast::createRestSession(const string& host, const string& path, const bool createStrand, RestResponseHandler&& rc,  const bool sign, RestParams params, const RequestType type)
    {
        if (rc == nullptr)
            throw std::runtime_error("callback is null");

        std::shared_ptr<RestSession> session;

        if (createStrand)
            session = std::make_shared<RestSession>(net::make_strand(getRestIoContext()), m_sslCtx, m_config.keys, std::move(rc), m_restCallersThreadPool);
        else
            session = std::make_shared<RestSession>(getRestIoContext().get_executor(), m_sslCtx, m_config.keys, std::move(rc), m_restCallersThreadPool);

        // we don't need to worry about the session's lifetime because RestSession::run() passes the session's shared_ptr
        // by value into the io_context. The session will be destroyed when there are no more io operations pending.

        if (!params.queryParams.empty())
        {
            string pathToSend;

            if (sign)
            {
                // signing rrequires a 'signature' param which is a SHA256 of the query params:
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

            session->run(host, "443", pathToSend, 11, type);   // 11 is HTTP version 1.1
        }
        else
        {
            session->run(host, "443", path, 11, type);
        }
    }


    WsToken BinanceBeast::createWsSession (const string& host, const std::string& path, WebSocketResponseHandler&& handler)
    {
        if (handler == nullptr)
            throw std::runtime_error("callback is null");

        auto session = std::make_shared<WsSession>(getWsIoContext(), m_sslCtx, std::move(handler));
        
        auto wsid = m_nextWsId.load();
        m_nextWsId.store(wsid+1U);

        {
            std::scoped_lock (m_wsSessionsMux);
            m_wsSessions[wsid] = session;
        }
        
        session->run(host, "443", path);

        return WsToken{.id = wsid};
    }
    

    WsToken BinanceBeast::startUserData(WebSocketResponseHandler handler)
    {
        WsToken token;
        if (amendUserDataListenKey(handler, UserDataStreamMode::Create))
        {
            token = createWsSession(m_config.wsApiUri, std::move("/ws/"+m_listenKey), std::move(handler));
        }
        return token;
    }    


    void BinanceBeast::renewListenKey(WebSocketResponseHandler handler)
    {
        if (amendUserDataListenKey(handler, UserDataStreamMode::Extend))
        {
            handler(WsResult{WsResult::State::Success});
        }
    }


    void BinanceBeast::closeUserData (WebSocketResponseHandler handler)
    {
        if (amendUserDataListenKey(handler, UserDataStreamMode::Close))
        {
            m_listenKey.clear();
            handler(WsResult{WsResult::State::Success});
        }
    }

}   // namespace BinanceBeast
