#include "BinanceBeast.h"
#include "SslCertificates.h"

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
        m_restCtx = std::make_unique<ssl::context> (ssl::context::tlsv12_client);
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
        // create listen key which we'll do synchronously
        
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
        http::request<http::string_body> req{http::verb::post, "/fapi/v1/listenKey", 11};
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
                
        m_listenKey.clear();
        if (res[http::field::content_type] == "application/json")
        {
            json::error_code ec;
            if (auto value = json::parse(res.body(), ec); ec)
            {
                fail(ec, "monitorUserData(): json read");
            }
            else
            {
                m_listenKey = json::value_to<string>(value.as_object()["listenKey"]);
            }
        }
        else
        {
            fail("monitorUserData(): content not json");
        }

        // Gracefully close the stream
        beast::error_code ec;
        stream.shutdown(ec);
        

        // start stream
        if (!m_listenKey.empty())
        {
            createWsSession(m_config.wsApiUri, std::move("/ws/"+m_listenKey), std::move(wc));
        }
    }    

}   // namespace BinanceBeast
