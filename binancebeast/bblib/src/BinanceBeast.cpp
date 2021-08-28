#include <binancebeast/BinanceBeast.h>
#include <binancebeast/SslCertificates.h>

#include <functional>


namespace bblib
{
    BinanceBeast::BinanceBeast() : m_nextWsIoContext(0), m_nextRestIoContext(0)
    {
        m_nextWsId.store(1);
    }


    BinanceBeast::~BinanceBeast()
    {
        stop();
    }


    void BinanceBeast::stop()
    {
        m_wsSessions.clear();

        // stop all io_context processing
        m_wsIocThreads.clear();
        m_restIocThreads.clear();
        m_nextWsId.store(0);
    }


    void BinanceBeast::start (const ConnectionConfig& config, const size_t nRestIoContexts, const size_t nWebsockIoContexts)
    {  
        m_nextWsIoContext = 0;
        m_nextRestIoContext = 0;

        m_restCtx = std::make_shared<ssl::context> (ssl::context::tlsv12_client);
        m_wsCtx = std::make_shared<ssl::context> (ssl::context::tlsv12_client);

        boost::system::error_code ec;
        load_root_certificates(*m_restCtx, ec);
        load_root_certificates(*m_wsCtx, ec);


        if (ec)
            throw boost::system::system_error {ec};

        // if this enabled on the testnet, it fails validation.
        // using some online tools shows the testnet does not send the root certificate, this maybe the cause of the problem
        if (m_config.verifyPeer)
        {
            m_restCtx->set_verify_mode(ssl::verify_peer); 
            m_wsCtx->set_verify_mode(ssl::verify_peer); 
        }            
        else
        {
            m_restCtx->set_verify_mode(ssl::verify_none); 
            m_wsCtx->set_verify_mode(ssl::verify_none);
        }
        

        m_config = config;

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


    net::io_context& BinanceBeast::getWsIoContext()
    {
        size_t curr = m_nextWsIoContext.load();
        size_t next = curr + 1 < m_wsIocThreads.size() ? curr + 1 : 0;

        m_nextWsIoContext.store(next);
        return *m_wsIocThreads[curr].ioc;
    }

    
    net::io_context& BinanceBeast::getRestIoContext()
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


    WsToken BinanceBeast::createWsSession (const string& host, const std::string& path, WebSocketResponseHandler&& handler)
    {
        if (handler == nullptr)
            throw std::runtime_error("callback is null");

        auto session = std::make_shared<WsSession>(getWsIoContext(), m_wsCtx, std::move(handler));
        
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
