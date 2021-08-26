#include <binancebeast/BinanceBeast.h>
#include <binancebeast/SslCertificates.h>

#include <functional>


namespace bblib
{
    BinanceBeast::BinanceBeast() : m_nextWsIoContext(0), m_nextRestIoContext(0)
    {

    }


    BinanceBeast::~BinanceBeast()
    {
        stop();
    }


    void BinanceBeast::stop()
    {
        // this will stop all io_context processing
        m_wsIocThreads.clear();
        m_restIocThreads.clear();
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


    //// REST
    void BinanceBeast::sendRestRequest(RestResponseHandler rc, const string& path, const RestSign sign, RestParams params, const RequestType type)
    {
        createRestSession(m_config.restApiUri, path, true, std::move(rc), sign == RestSign::HMAC_SHA256, std::move(params), type);
    }


    void BinanceBeast::ping (RestResponseHandler rr)
    { 
        createRestSession(m_config.restApiUri, "/fapi/v1/ping", false,  std::move(rr), false, RestParams{});
    }


    void BinanceBeast::exchangeInfo(RestResponseHandler rr)
    {
        createRestSession(m_config.restApiUri, "/fapi/v1/exchangeInfo", false, std::move(rr), false, RestParams{});
    }

    void BinanceBeast::serverTime(RestResponseHandler rr)
    {
        createRestSession(m_config.restApiUri, "/fapi/v1/time", false, std::move(rr), false, RestParams{});
    }

    void BinanceBeast::orderBook(RestResponseHandler rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/fapi/v1/depth", false, std::move(rr), false, std::move(params));
    }

    void BinanceBeast::allOrders(RestResponseHandler rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/fapi/v1/allOrders", false, std::move(rr), true, std::move(params));
    }

    void BinanceBeast::recentTradesList(RestResponseHandler rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/fapi/v1/trades", false, std::move(rr), true, std::move(params));
    }

    void BinanceBeast::historicTrades(RestResponseHandler rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/fapi/v1/historicalTrades", false, std::move(rr), true, std::move(params));
    }

    void BinanceBeast::aggregateTradesList(RestResponseHandler rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/fapi/v1/aggTrades", false, std::move(rr), true, std::move(params));
    }

    void BinanceBeast::klines(RestResponseHandler rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/fapi/v1/klines", false, std::move(rr), true, std::move(params));
    }

    void BinanceBeast::contractKlines(RestResponseHandler rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/fapi/v1/continuousKlines", false, std::move(rr), true, std::move(params));
    }

    void BinanceBeast::indexPriceKlines(RestResponseHandler rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/fapi/v1/indexPriceKlines", false, std::move(rr), true, std::move(params));
    }

    void BinanceBeast::markPriceKlines(RestResponseHandler rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/fapi/v1/markPriceKlines", false, std::move(rr), true, std::move(params));
    }

    void BinanceBeast::markPrice(RestResponseHandler rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/fapi/v1/premiumIndex", false, std::move(rr), true, std::move(params));
    }

    void BinanceBeast::fundingRate(RestResponseHandler rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/fapi/v1/fundingRate", false, std::move(rr), true, std::move(params));
    }

    void BinanceBeast::tickerPriceChange24hr(RestResponseHandler rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/fapi/v1/ticker/24hr", false, std::move(rr), true, std::move(params));
    }

    void BinanceBeast::symbolPriceTicker(RestResponseHandler rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/fapi/v1/ticker/price", false, std::move(rr), true, std::move(params));
    }
    
    void BinanceBeast::symbolBookTicker(RestResponseHandler rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/fapi/v1/ticker/bookTicker", false, std::move(rr), true, std::move(params));
    }

    void BinanceBeast::openInterest(RestResponseHandler rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/fapi/v1/openInterest", false, std::move(rr), true, std::move(params));
    }

    void BinanceBeast::openInterestStats(RestResponseHandler rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/futures/data/openInterestHist", false, std::move(rr), true, std::move(params));
    }

    void BinanceBeast::topTraderLongShortRatioAccounts(RestResponseHandler rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/futures/data/topLongShortAccountRatio", false, std::move(rr), true, std::move(params));
    }
    
    void BinanceBeast::topTraderLongShortRatioPositions(RestResponseHandler rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/futures/data/topLongShortPositionRatio", false, std::move(rr), true, std::move(params));
    }

    void BinanceBeast::longShortRatio(RestResponseHandler rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/futures/data/globalLongShortAccountRatio", false, std::move(rr), true, std::move(params));
    }
    
    void BinanceBeast::takerBuySellVolume(RestResponseHandler rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/futures/data/takerlongshortRatio", false, std::move(rr), true, std::move(params));
    }

    void BinanceBeast::historicalBlvtNavKlines(RestResponseHandler rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/futures/v1/lvtKlines", false, std::move(rr), true, std::move(params));
    }
    
    void BinanceBeast::compositeIndexSymbolInfo(RestResponseHandler rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/futures/v1/indexInfo", false, std::move(rr), true, std::move(params));
    }


    //// WebSockets

    void BinanceBeast::startWebSocket (WebSocketResponseHandler wc, string streamName)
    {
        createWsSession(m_config.wsApiUri, std::move("/ws/"+std::move(streamName)), std::move(wc));
    }


   
    void BinanceBeast::monitorMarkPrice (WebSocketResponseHandler wc, string params)
    {
        createWsSession(m_config.wsApiUri, std::move("/ws/"+std::move(params)), std::move(wc));        
    }

    
    void BinanceBeast::monitorKline (WebSocketResponseHandler wc, string params)
    {
        createWsSession(m_config.wsApiUri, std::move("/ws/"+std::move(params)), std::move(wc));
    }

    void BinanceBeast::monitorIndividualSymbolMiniTicker (WebSocketResponseHandler wc, string symbol)
    {
        createWsSession(m_config.wsApiUri, std::move("/ws/"+std::move(symbol)+"@miniTicker"), std::move(wc));
    }

    void BinanceBeast::monitorAllMarketMiniTickers (WebSocketResponseHandler wc)
    {
        createWsSession(m_config.wsApiUri, std::move("/ws/!miniTicker@arr"), std::move(wc));
    }

    void BinanceBeast::monitorIndividualSymbolTicker(WebSocketResponseHandler wc, string symbol)
    {
        createWsSession(m_config.wsApiUri, std::move("/ws/"+std::move(symbol) + "@ticker"), std::move(wc));
    }

    void BinanceBeast::monitorSymbolBookTicker(WebSocketResponseHandler wc, string symbol)
    {
        createWsSession(m_config.wsApiUri, std::move("/ws/"+std::move(symbol) + "@bookTicker"), std::move(wc));
    }

    void BinanceBeast::monitorAllBookTicker(WebSocketResponseHandler wc)
    {
        createWsSession(m_config.wsApiUri, std::move("/ws/!bookTicker"), std::move(wc));
    }

    void BinanceBeast::monitorLiquidationOrder(WebSocketResponseHandler wc, string symbol)
    {
        createWsSession(m_config.wsApiUri, std::move("/ws/"+std::move(symbol)+"@forceOrder"), std::move(wc));
    }

    void BinanceBeast::monitorAllMarketLiduiqdationOrder(WebSocketResponseHandler wc)
    {
        createWsSession(m_config.wsApiUri, std::move("/ws/!forceOrder@arr"), std::move(wc));
    }

    void BinanceBeast::monitorPartialBookDepth(WebSocketResponseHandler wc, string symbol, string levels, string updateSpeed)
    {
        createWsSession(m_config.wsApiUri, std::move("/ws/"+std::move(symbol)+"@depth"+levels+(updateSpeed.empty() ? "" : "@"+updateSpeed)), std::move(wc));
    }

    void BinanceBeast::monitorDiffBookDepth(WebSocketResponseHandler wc, string symbol, string updateSpeed)
    {
        createWsSession(m_config.wsApiUri, std::move("/ws/"+std::move(symbol)+"@depth"+(updateSpeed.empty() ? "" : "@"+updateSpeed)), std::move(wc));
    }

    void BinanceBeast::monitorCompositeIndexSymbolInfo(WebSocketResponseHandler wc, string symbol)
    {
        createWsSession(m_config.wsApiUri, std::move("/ws/"+std::move(symbol)+"@compositeIndex"), std::move(wc));
    }

    void BinanceBeast::startUserData(WebSocketResponseHandler wc)
    {
        if (amendUserDataListenKey(wc, UserDataStreamMode::Create))
        {
            createWsSession(m_config.wsApiUri, std::move("/ws/"+m_listenKey), std::move(wc));
        }
    }    

    void BinanceBeast::renewListenKey(WebSocketResponseHandler wc)
    {
        if (amendUserDataListenKey(wc, UserDataStreamMode::Extend))
        {
            wc(WsResult{WsResult::State::Success});
        }
    }

    void BinanceBeast::closeUserData (WebSocketResponseHandler wc)
    {
        if (amendUserDataListenKey(wc, UserDataStreamMode::Close))
        {
            m_listenKey.clear();
            wc(WsResult{WsResult::State::Success});
        }
    }

}   // namespace BinanceBeast
