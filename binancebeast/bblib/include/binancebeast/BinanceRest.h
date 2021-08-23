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
            resolver_(ex),
            stream_(ex, *ctx),
            apiKeys_(keys),
            callback_(callback),
            threadPool_(threadPool)
        {
        }


        void run(const string& host, const string& port, const string& target, const int version)
        {
            // Set SNI Hostname (many hosts need this to handshake successfully)
            if (!SSL_set_tlsext_host_name(stream_.native_handle(), host.c_str()))
            {
                beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
                fail(ec, "SNI hostname", threadPool_, callback_);
            }
            else
            {
                // Set up an HTTP GET request message
                req_.version(version);
                req_.method(http::verb::get);
                req_.target(target);
                req_.set(http::field::host, host);
                req_.set(http::field::user_agent, BINANCEBEAST_USER_AGENT);
                req_.insert("X-MBX-APIKEY", apiKeys_.api);
                
                // Look up the domain name
                resolver_.async_resolve(host, port, beast::bind_front_handler(&RestSession::on_resolve,shared_from_this()));
            }
        }


        void on_resolve(beast::error_code ec, tcp::resolver::results_type results)
        {
            if (ec)
                fail(ec, "resolve", threadPool_, callback_);
            else
            {
                // Set a timeout on the operation
                beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

                // Make the connection on the IP address we get from a lookup
                beast::get_lowest_layer(stream_).async_connect(results, beast::bind_front_handler(&RestSession::on_connect,shared_from_this()));    
            }
        }


        void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type)
        {
            if (ec)
            {
                fail(ec, "connect", threadPool_, callback_);
            } 
            else
            {
                // Perform the SSL handshake
                stream_.async_handshake( ssl::stream_base::client, beast::bind_front_handler(&RestSession::on_handshake, shared_from_this()));
            }            
        }


        void on_handshake(beast::error_code ec)
        {        
            if (ec)
            {
                fail(ec, "handshake", threadPool_, callback_);
            } 
            else
            {
                // Set a timeout on the operation
                beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

                // Send the HTTP request to the remote host
                http::async_write(stream_, req_, beast::bind_front_handler(&RestSession::on_write, shared_from_this()));
            }
            
        }


        void on_write(beast::error_code ec, std::size_t bytes_transferred)
        {
            boost::ignore_unused(bytes_transferred);

            if (ec)
                fail(ec, "write", threadPool_, callback_);
            else // Receive the HTTP response
                http::async_read(stream_, buffer_, res_, beast::bind_front_handler(&RestSession::on_read, shared_from_this()));
        }


        void on_read(beast::error_code ec, std::size_t bytes_transferred)
        {
            boost::ignore_unused(bytes_transferred);

            if (ec)
            {
                return fail(ec, "read", threadPool_, callback_);
            }

            if (res_[http::field::content_type] == "application/json")
            {
                json::error_code ec;

                if (auto value = json::parse(res_.body(), ec); ec)
                {
                    fail(ec, "json read", threadPool_, callback_);
                }
                else
                {                
                    if (callback_)
                    {
                        RestResult result {std::move(value)};
                        net::post(threadPool_, boost::bind(callback_, std::move(result)));
                    }
                }            
            }

            // Set a timeout on the operation
            //beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));
            // Gracefully close the stream
            //stream_.async_shutdown(beast::bind_front_handler(&RestSession::on_shutdown,shared_from_this()));
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
                return fail(ec, "shutdown", threadPool_, callback_);

            // If we get here then the connection is closed gracefully
        }


    private:
        tcp::resolver resolver_;
        beast::ssl_stream<beast::tcp_stream> stream_;
        beast::flat_buffer buffer_;     // must persist between reads
        http::request<http::string_body> req_;
        http::response<http::string_body> res_;
        ConnectionConfig::ConnectionKeys apiKeys_;
        RestCallback callback_;
        net::thread_pool& threadPool_;
    };
}

#endif
