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
        
    }


    void BinanceBeast::start (const ConnectionConfig& config, const size_t nRestIoContexts, const size_t nWebsockIoContexts)
    {  
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

    void BinanceBeast::ping ()
    { 
        createRestSession(m_config.restApiUri, "/fapi/v1/ping", false, nullptr);
    }


    void BinanceBeast::exchangeInfo(RestCallback rr)
    {
        createRestSession(m_config.restApiUri, "/fapi/v1/exchangeInfo", false, std::move(rr));
    }

    void BinanceBeast::serverTime(RestCallback rr)
    {
        createRestSession(m_config.restApiUri, "/fapi/v1/time", false, std::move(rr));
    }


    void BinanceBeast::orderBook(RestCallback rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/fapi/v1/depth", false, std::move(rr), false, std::move(params));
    }

    void BinanceBeast::allOrders(RestCallback rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/fapi/v1/allOrders", false, std::move(rr), true, std::move(params));
    }

    void BinanceBeast::recentTradesList(RestCallback rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/fapi/v1/trades", false, std::move(rr), true, std::move(params));
    }

    void BinanceBeast::historicTrades(RestCallback rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/fapi/v1/historicalTrades", false, std::move(rr), true, std::move(params));
    }

    void BinanceBeast::aggregateTradesList(RestCallback rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/fapi/v1/aggTrades", false, std::move(rr), true, std::move(params));
    }

    void BinanceBeast::klines(RestCallback rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/fapi/v1/klines", false, std::move(rr), true, std::move(params));
    }

    void BinanceBeast::contractKlines(RestCallback rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/fapi/v1/continuousKlines", false, std::move(rr), true, std::move(params));
    }

    void BinanceBeast::indexPriceKlines(RestCallback rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/fapi/v1/indexPriceKlines", false, std::move(rr), true, std::move(params));
    }

    void BinanceBeast::markPriceKlines(RestCallback rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/fapi/v1/markPriceKlines", false, std::move(rr), true, std::move(params));
    }

    void BinanceBeast::markPrice(RestCallback rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/fapi/v1/premiumIndex", false, std::move(rr), true, std::move(params));
    }

    void BinanceBeast::fundingRate(RestCallback rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/fapi/v1/fundingRate", false, std::move(rr), true, std::move(params));
    }

    void BinanceBeast::tickerPriceChange24hr(RestCallback rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/fapi/v1/ticker/24hr", false, std::move(rr), true, std::move(params));
    }

    void BinanceBeast::symbolPriceTicker(RestCallback rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/fapi/v1/ticker/price", false, std::move(rr), true, std::move(params));
    }
    
    void BinanceBeast::symbolBookTicker(RestCallback rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/fapi/v1/ticker/bookTicker", false, std::move(rr), true, std::move(params));
    }

    void BinanceBeast::openInterest(RestCallback rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/fapi/v1/openInterest", false, std::move(rr), true, std::move(params));
    }

    void BinanceBeast::openInterestStats(RestCallback rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/futures/data/openInterestHist", false, std::move(rr), true, std::move(params));
    }

    void BinanceBeast::topTraderLongShortRatioAccounts(RestCallback rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/futures/data/topLongShortAccountRatio", false, std::move(rr), true, std::move(params));
    }
    
    void BinanceBeast::topTraderLongShortRatioPositions(RestCallback rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/futures/data/topLongShortPositionRatio", false, std::move(rr), true, std::move(params));
    }

    void BinanceBeast::longShortRatio(RestCallback rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/futures/data/globalLongShortAccountRatio", false, std::move(rr), true, std::move(params));
    }
    
    void BinanceBeast::takerBuySellVolume(RestCallback rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/futures/data/takerlongshortRatio", false, std::move(rr), true, std::move(params));
    }

    void BinanceBeast::historicalBlvtNavKlines(RestCallback rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/futures/v1/lvtKlines", false, std::move(rr), true, std::move(params));
    }
    
    void BinanceBeast::compositeIndexSymbolInfo(RestCallback rr, RestParams params)
    {
        createRestSession(m_config.restApiUri, "/futures/v1/indexInfo", false, std::move(rr), true, std::move(params));
    }


    //// WebSockets

    void BinanceBeast::monitorMarkPrice (WsCallback wc, string params)
    {
        createWsSession(m_config.wsApiUri, std::move("/ws/"+std::move(params)), std::move(wc));
    }

    void BinanceBeast::monitorKline (WsCallback wc, string params)
    {
        createWsSession(m_config.wsApiUri, std::move("/ws/"+std::move(params)), std::move(wc));
    }

    void BinanceBeast::monitorIndividualSymbolMiniTicker (WsCallback wc, string symbol)
    {
        createWsSession(m_config.wsApiUri, std::move("/ws/"+std::move(symbol)+"@miniTicker"), std::move(wc));
    }

    void BinanceBeast::monitorAllMarketMiniTickers (WsCallback wc)
    {
        createWsSession(m_config.wsApiUri, std::move("/ws/!miniTicker@arr"), std::move(wc));
    }

    void BinanceBeast::monitorIndividualSymbolTicker(WsCallback wc, string symbol)
    {
        createWsSession(m_config.wsApiUri, std::move("/ws/"+std::move(symbol) + "@ticker"), std::move(wc));
    }

    void BinanceBeast::monitorSymbolBookTicker(WsCallback wc, string symbol)
    {
        createWsSession(m_config.wsApiUri, std::move("/ws/"+std::move(symbol) + "@bookTicker"), std::move(wc));
    }

    void BinanceBeast::monitorAllBookTicker(WsCallback wc)
    {
        createWsSession(m_config.wsApiUri, std::move("/ws/!bookTicker"), std::move(wc));
    }

    void BinanceBeast::monitorUserData(WsCallback wc)
    {
        if (amendUserDataListenKey(wc, UserDataStreamMode::Create))
        {
            createWsSession(m_config.wsApiUri, std::move("/ws/"+m_listenKey), std::move(wc));
        }
    }    

    void BinanceBeast::renewListenKey(WsCallback wc)
    {
        if (amendUserDataListenKey(wc, UserDataStreamMode::Extend))
        {
            wc(WsResult{WsResult::State::Success});
        }
    }

    void BinanceBeast::closeUserData (WsCallback wc)
    {
        if (amendUserDataListenKey(wc, UserDataStreamMode::Close))
        {
            m_listenKey.clear();
            wc(WsResult{WsResult::State::Success});
        }
    }

}   // namespace BinanceBeast
