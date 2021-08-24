#include <binancebeast/BinanceBeast.h>
#include <binancebeast/SslCertificates.h>

#include <functional>


namespace bblib
{
    BinanceBeast::BinanceBeast()
    {

    }


    BinanceBeast::~BinanceBeast()
    {
        m_restWorkGuard.reset();
        m_wsWorkGuard.reset();

        if (!m_restIoc.stopped())
            m_restIoc.stop();

        if (!m_wsIoc.stopped())
            m_wsIoc.stop();

        m_restIocThread->join();
        m_wsIocThread->join();
    }


    void BinanceBeast::start (const ConnectionConfig& config)
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

        m_restWorkGuard = std::make_unique<net::executor_work_guard<net::io_context::executor_type>> (m_restIoc.get_executor());
        m_restIocThread = std::make_unique<std::thread> (std::function { [this]() { m_restIoc.run(); } });

        m_wsWorkGuard = std::make_unique<net::executor_work_guard<net::io_context::executor_type>> (m_wsIoc.get_executor());
        m_wsIocThread = std::make_unique<std::thread> (std::function { [this]() { m_wsIoc.run(); } });
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
