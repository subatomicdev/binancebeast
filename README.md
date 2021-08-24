---
## NOTE: this is in the early stages and is subject to breaking changes
---

A C++ library for the Binance Futures exchange, using Boost's Beast and JSON libraries for networking (http/websockets) and json respectively. 


NOTE: the library has only been tested on Ubuntu. It *should* work on Windows but I can't confirm.


## Example
An example is below:

* Uses an REST call to get all orders for BTCUSDT
* I use a mutex and cv to wait for the async `allOrders()` to return
* The call to `allOrders()`
  * An std::function which is the result handler, called when there is an error or the reply is received
  * The params which are appended to the REST query


```cpp
int main (int argc, char ** argv)
{
    auto config = ConnectionConfig::MakeTestNetConfig();    // or MakeLiveConfig()
    config.keys.api     = "YOUR API KEY";
    config.keys.secret  = "YOUR SECRET KEY";

    std::condition_variable cvHaveReply;

    BinanceBeast bb;

    bb.start(config);   // must always call this once to start the networking processing loop

    bb.allOrders(   [&](RestResult result)      // this is called when the reply is received or an error
                    {  
                        std::cout << result.json.as_array() << "\n";
                        cvHaveReply.notify_one();
                    },
                    RestParams {RestParams::QueryParams {{"symbol", "BTCUSDT"}}});      // params for REST call

    std::mutex mux;
    std::unique_lock lck(mux);

    cvHaveReply.wait(lck);

    return 0;
}
```

### Notes
* The REST handlers are called from a boost::thread_pool
* The WebSocket handlers is called from a thread pool which gaurantees the order is maintained
* If your handler takes time to process, it doesn't affect the networking processing thread(s)
* The are two instatiations of `boost::asio::io_context` , one for Rest calls and the other for Websockets
* I am considering creating a pool of `boost::asio::io_context` for the Websockets and distributing work evenly

---

## Aims
- Performance: handle many Rest requests and websocket sessions
- Configurable: provide configuration on how io_context are used, particularly if issuing many Rest calls
- Usability: easy to build and use
- Documentation: provide tests/examples on how to use and best practises


## Quick Guide

*NOTE: If you are calling a Rest function often, use the equivalent Websocket function instead*

*NOTE: All API functions are asychronous.*

The general usage is:

- Create an account with Binance, verify your account and create an API key
  - There are separate registration and keys for the live and test exchanges
- Create the config with `ConnectionConfig::MakeTestNetConfig()` or `ConnectionConfig::MakeLiveConfig()` 
- Instatiate a `BinanceBeast` object then call `start()`
- Call a Rest or websocket function, all of which are asynchronous, supplied with a callback function (`RestCallback` or `WsCallback`)
  - A websocket stream is closed when the `BinanceBeast` object is destructed
  - There is no close monitor function, this may be added later
- In the handler, use the `hasErrorCode()` function
  - if so access `code` and `msg` to find information
- The JSON is stored in a boost::json::value, so if there's no error, use the `as_object()` or `as_array()` 



### Rest Calls
#### All Orders Example
```cpp
int main (int argc, char ** argv)
{
    auto config = ConnectionConfig::MakeTestNetConfig();    // or MakeLiveConfig()
    config.keys.api     = "YOUR API KEY";
    config.keys.secret  = "YOUR SECRET KEY";

    std::condition_variable cvHaveReply;

    BinanceBeast bb;

    bb.start(config);   // must always call this once to start the networking processing loop

    bb.allOrders(   [&](RestResult result)      // this is called when the reply is received or an error
                    {  
                        std::cout << result.json << "\n";
                        cvHaveReply.notify_one();
                    },
                    RestParams {RestParams::QueryParams {{"symbol", "BTCUSDT"}}});      // params for REST call

    std::mutex mux;
    std::unique_lock lck(mux);

    cvHaveReply.wait(lck);

    return 0;
}

```

### Websockets
#### Mark Price Symbol
Receive Mark Price for ETHUSDT for 10 seconds:

```cpp
int main (int argc, char ** argv)
{
    auto config = ConnectionConfig::MakeTestNetConfig();    // or MakeLiveConfig()
    config.keys.api     = "YOUR API KEY";
    config.keys.secret  = "YOUR SECRET KEY";

    std::condition_variable cvHaveReply;

    BinanceBeast bb;

    bb.start(config);   // must always call this once to start the networking processing loop

    bb.monitorMarkPrice([&](WsResult result)      // this is called for each message or error
    {  
        std::cout << result.json << "\n\n";

        if (result.hasErrorCode())
        {
            std::cout << "\nError code: " << std::to_string(json::value_to<std::int32_t>(result.json.as_object()["code"]))
                      << "\nError msg: " << json::value_to<std::string>(result.json.as_object()["msg"]) << "\n";
        }
        else
        {
            std::cout << "\n" << result.json.as_object()["s"] << " = " << result.json.as_object()["p"] << "\n";
        }

    }, "ethusdt@markPrice@1s");      // params for Websocket call

    using namespace std::chrono_literals;    
    std::this_thread::sleep_for(10s);

    return 0;
}
```

## Build
You must have Git installed and a development environment installed (i.e. gcc, cmake). It has been developed with GCC 10.3.0.

* `git clone --recurse-submodules -j8 https://github.com/subatomicdev/binancebeast.git binancebeast`
* `cd binancebeast`
* `./build_linux_x64_release.sh`
  *  Uses `vcpkg` to install the required libraries. This installs cmake and ninja but it's local to vcpkg so will not affect existing installations
  * Configure and build
* After the build a short test runs (which doesn't require an API key)
* The library and test binary are in the lib and bin directories
  



