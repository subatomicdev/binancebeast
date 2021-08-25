---
## NOTE: this is in the early stages and is subject to breaking changes
---

A C++ library for the Binance Futures exchange, using Boost's Beast and JSON libraries for networking (http/websockets) and json respectively. 

The library has only been tested on Ubuntu. It *should* work on Windows but I can't confirm.


## Status
- Rest: all market endpoints, 50% of account/trade endpoints
- WebSockets: 50%
- User Data: all


## Example

* Use a REST call to get all orders for BTCUSDT
* I use a mutex and cv to wait for the _async_ `allOrders()` to return
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
* The WebSocket handlers are called from a thread pool which gaurantees the order is maintained
* If your handler takes time to process, it doesn't affect the networking processing thread(s)
* There are multiple `boost::asio::io_context` for Rest and Websockets calls which are set with `BinanceBeast::start()`
  * Rest default is 4
  * Websockets default is 6
* Work is distributed evenly with a simple round-robin


---

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



### REST
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

### WebSockets
Receive Mark Price for ETHUSDT for 10 seconds:

```cpp
int main (int argc, char ** argv)
{
    auto config = ConnectionConfig::MakeTestNetConfig();    // or MakeLiveConfig()
    config.keys.api     = "YOUR API KEY";
    config.keys.secret  = "YOUR SECRET KEY";

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

## User Data
Use the `BinanceBeast::monitorUserData()`, it's a standard websocket session.

* User data has a key, "e", which is the eventType
* Listen keys expire after 60 minutes
* You should use `BinanceBeast::renewListenKey()` to extend the key within 60 minutes
* If the key expires you should call `BinanceBeast::monitorUserData()` to create a new key
  * When a key expires it does not close the websocket connection


```cpp
void onUserData(WsResult result)
{
    auto topLevel = result.json.as_object();
    const auto eventType = json::value_to<string>(topLevel["e"]);

    if (eventType == "listenKeyExpired")
    {
        std::cout << "listen key expired, renew with BinanceBeast::renewListenKey()\n";
    }
    else if (eventType == "MARGIN_CALL")
    {
        std::cout << "margin call\n";
    }
    else if (eventType == "ACCOUNT_UPDATE")
    {
        std::cout << "account update\n";
    }
    else if (eventType == "ORDER_TRADE_UPDATE")
    {
        std::cout << "order trade update\n";
    }
    else if (eventType == "ACCOUNT_CONFIG_UPDATE")
    {
        std::cout << "account config update\n";
    }
}
```


## Build
You must have Git installed and a development environment installed (i.e. gcc, cmake). It has been developed with GCC 10.3.0.

* `git clone --recurse-submodules -j8 https://github.com/subatomicdev/binancebeast.git binancebeast`
* `cd binancebeast`
* `./build_linux_x64_static_release.sh`
  *  Uses `vcpkg` to install the required libraries. This installs cmake and ninja but it's local to vcpkg so will not affect existing installations
  * Configure and build
* After the build a short test runs (which doesn't require an API key)
* The library and test binary are in the lib and bin directories
  



