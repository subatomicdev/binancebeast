#include "BinanceBeast.h"
#include "SslCertificates.h"

#include <functional>
#include <chrono>


namespace bblib
{


BinanceBeast::BinanceBeast() 
{

}


BinanceBeast::~BinanceBeast()
{
    m_restWorkGuard.reset();

    if (!m_restIoc.stopped())
        m_restIoc.stop();

    m_iocRestThread->join();
}


void BinanceBeast::start (const ConnectionConfig& config)
{  
    m_restCtx = std::make_unique<ssl::context> (ssl::context::tlsv12_client);

    boost::system::error_code ec;
    load_root_certificates(*m_restCtx, ec);

    if (ec)
        throw boost::system::system_error {ec};

    // if this enabled on the testnet, it fails validation.
    // using some online tools shows the testnet does not send the root certificate, this maybe the cause of the problem
    if (m_config.verifyPeer)
        m_restCtx->set_verify_mode(ssl::verify_peer); 
    else
        m_restCtx->set_verify_mode(ssl::verify_none); 


    m_config = config;

    m_restWorkGuard = std::make_unique<net::executor_work_guard<net::io_context::executor_type>> (m_restIoc.get_executor());
    
    m_iocRestThread = std::make_unique<std::thread> (std::function { [this]() { m_restIoc.run(); } });
}



void BinanceBeast::ping ()
{ 
    createRestSession(m_config.restApiUri, "/fapi/v1/ping");
}


void BinanceBeast::exchangeInfo()
{
    createRestSession(m_config.restApiUri, "/fapi/v1/exchangeInfo");
}


}   // namespace BinanceBeast