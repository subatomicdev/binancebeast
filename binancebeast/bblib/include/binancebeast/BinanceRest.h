#ifndef BINANCEBEAST_REST_H
#define BINANCEBEAST_REST_H

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/bind/bind.hpp>

#include <openssl/hmac.h>   // to sign query params

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <unordered_map>
#include <map>
#include <sstream>

#include "BinanceCommon.h"


namespace bblib
{
    struct RestResult
    {
        enum class State { Fail, Success };

        RestResult (string failReason) : state(State::Fail), failMessage(failReason)
        {

        }

        RestResult(json::value&& object) : json (std::move(object)), state(State::Success)
        {

        }

        bool hasErrorCode() const
        {
            bool error = false;
            try
            {
                if (json.is_object())
                    error = json.as_object().if_contains("code");
            }
            catch(...)
            {            
            }
            
            return error;
        }

        json::value json;
        State state;
        string failMessage;
    };

    struct RestParams
    {
        using QueryParams = std::unordered_map<string, string>;

        RestParams () = default;
        
        RestParams (QueryParams&& params) : queryParams(std::move(params))
        {
        }

        RestParams (const QueryParams& params) : queryParams(params)
        {
        }


        QueryParams queryParams;
    };


    using RestCallback = std::function<void(RestResult)>;



    class RestSession : public std::enable_shared_from_this<RestSession>
    {

    public:
        explicit RestSession(net::any_io_executor ex, std::shared_ptr<ssl::context> ctx, const ConnectionConfig::ConnectionKeys& keys, const RestCallback&& callback, net::thread_pool& threadPool) :
            m_resolver(ex),
            m_stream(ex, *ctx),
            m_apiKeys(keys),
            m_callback(callback),
            m_threadPool(threadPool)
        {
        }


        void run(const string& host, const string& port, const string& target, const int version)
        {
            // set SNI Hostname (many hosts need this to handshake successfully)
            if (!SSL_set_tlsext_host_name(m_stream.native_handle(), host.c_str()))
            {
                beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
                fail(ec, "SNI hostname", m_threadPool, m_callback);
            }
            else
            {
                // Set up an HTTP GET request message
                m_req.version(version);
                m_req.method(http::verb::get);
                m_req.target(target);
                m_req.set(http::field::host, host);
                m_req.set(http::field::user_agent, BINANCEBEAST_USER_AGENT);
                m_req.insert("X-MBX-APIKEY", m_apiKeys.api);
                
                // look up the domain name
                m_resolver.async_resolve(host, port, beast::bind_front_handler(&RestSession::on_resolve,shared_from_this()));
            }
        }


        void on_resolve(beast::error_code ec, tcp::resolver::results_type results)
        {
            if (ec)
                fail(ec, "resolve", m_threadPool, m_callback);
            else
            {
                beast::get_lowest_layer(m_stream).expires_after(std::chrono::seconds(10));   // TODO is this ok

                beast::get_lowest_layer(m_stream).async_connect(results, beast::bind_front_handler(&RestSession::on_connect,shared_from_this()));    
            }
        }


        void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type)
        {
            if (ec)
            {
                fail(ec, "connect", m_threadPool, m_callback);
            } 
            else
            {
                // Perform the SSL handshake
                m_stream.async_handshake( ssl::stream_base::client, beast::bind_front_handler(&RestSession::on_handshake, shared_from_this()));
            }            
        }


        void on_handshake(beast::error_code ec)
        {        
            if (ec)
            {
                fail(ec, "handshake", m_threadPool, m_callback);
            } 
            else
            {
                // Set a timeout on the operation
                beast::get_lowest_layer(m_stream).expires_after(std::chrono::seconds(10));

                // Send the HTTP request to the remote host
                http::async_write(m_stream, m_req, beast::bind_front_handler(&RestSession::on_write, shared_from_this()));
            }
            
        }


        void on_write(beast::error_code ec, std::size_t bytes_transferred)
        {
            boost::ignore_unused(bytes_transferred);

            // connected and handshaked, so can begin reading websocket data

            beast::get_lowest_layer(m_stream).expires_after(std::chrono::seconds(30));   

            if (ec)
                fail(ec, "write", m_threadPool, m_callback);
            else 
                http::async_read(m_stream, m_buffer, m_res, beast::bind_front_handler(&RestSession::on_read, shared_from_this()));
        }


        void on_read(beast::error_code ec, std::size_t bytes_transferred)
        {
            boost::ignore_unused(bytes_transferred);

            if (ec)
            {
                return fail(ec, "read", m_threadPool, m_callback);
            }

            if (m_res[http::field::content_type] == "application/json")
            {
                json::error_code ec;

                if (auto value = json::parse(m_res.body(), ec); ec)
                {
                    fail(ec, "json read", m_threadPool, m_callback);
                }
                else
                {   
                    RestResult result {std::move(value)};
                    net::post(m_threadPool, boost::bind(m_callback, std::move(result)));
                }            
            }
        }


        void on_shutdown(beast::error_code ec)
        {
            if (ec == net::error::eof)
            {
                // Rationale:
                // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
                ec = {};
            }

            if (ec)
                return fail(ec, "shutdown", m_threadPool, m_callback);

            // If we get here then the connection is closed gracefully
        }


    private:
        tcp::resolver m_resolver;
        beast::ssl_stream<beast::tcp_stream> m_stream;
        beast::flat_buffer m_buffer;
        http::request<http::string_body> m_req;
        http::response<http::string_body> m_res;
        ConnectionConfig::ConnectionKeys m_apiKeys;
        RestCallback m_callback;
        net::thread_pool& m_threadPool;
    };
}

#endif
