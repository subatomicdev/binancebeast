---
## NOTE: this is in the early stages and is subject to breaking changes
---

A C++ library for the Binance Futures exchange, using Boost's Beast and JSON libraries for networking (http/websockets) and json respectively. 

The library has is developed on Ubuntu and only tested on Ubuntu. Support for Windows can be added later.


## Status

26th August 
**BREAKING CHANGES**
- The REST functions have been deprecated and will be removed. Use `sendRequest()` instead. See docs and `examples/rest.cpp`.

25th August 
**BREAKING CHANGES**
- The monitor functions have been deprecated and will be removed. Use `startWebSocket()`. 
- For user data, `monitorUserData()` which has been renamed `startUserData()`.See `examples/userdata.cpp`.


### USD-M Futures
- Rest
  - Market: All
  - Account/trade: All
- WebSockets: All
- User Data: All


## Example

See `examples` directory for code.

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

    bb.sendRestRequest( [&](RestResult result)      // this is called when the reply is received or an error
                        {  
                            if (result.hasErrorCode())
                                std::cout << "\nFAIL: " << result.failMessage << "\n";
                            else
                                std::cout << "\n" << result.json << "\n";

                            cvHaveReply.notify_one();
                        },
                        "/fapi/v1/allOrders",                   // path
                        RestSign::HMAC_SHA256,                  // request must be signed
                        RestParams{{{"symbol", "BTCUSDT"}}});   // request parameters

    std::mutex mux;
    std::unique_lock lck(mux);

    cvHaveReply.wait(lck);

    return 0;
}
```

---

## Build
You must have Git installed and a development environment installed (i.e. gcc, cmake). It has been developed with GCC 10.3.0.

* `git clone --recurse-submodules -j8 https://github.com/subatomicdev/binancebeast.git binancebeast`
* `cd binancebeast`
* `./build_linux_x64_static_release.sh`
  *  Uses `vcpkg` to install the required libraries. This installs cmake and ninja but it's local to vcpkg so will not affect existing installations
  * Configure and build
* After the build a short test runs (which doesn't require an API key)
* The library and test binary are in the lib and bin directories

---

## Quick Guide

* Consider using Websockets rather than frequent REST calls
* All API functions are asychnronous
* There are multiple `boost::asio::io_context` for Rest and Websockets calls which are set with `BinanceBeast::start()`
  * Rest default is 4
  * Websockets default is 6
  * Work is distributed evenly with a simple round-robin
* 


General usage:

- Create an account with Binance, verify your account and create an API key
  - There are separate registration and keys for the live and test exchanges
- Create the config with `ConnectionConfig::MakeTestNetConfig()` or `ConnectionConfig::MakeLiveConfig()` 
- Instatiate a `BinanceBeast` object then call `start()`
- Call a Rest or websocket function, all of which are asynchronous, supplied with a callback function (`RestResponseHandler` or `WebSocketResponseHandler`)
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
    // you don't need API or secret keys for mark price

    BinanceBeast bb;

    bb.start(config);   // must always call this once to start the networking processing loop

    bb.startWebSocket([&](WsResult result)      // this is called for each message or error
    {  
        std::cout << result.json << "\n\n";   // show entire JSON

        if (result.hasErrorCode())
            std::cout << "\nError: " << result.failMessage << "\n";
        else // how to access values
            std::cout << "\n" << result.json.as_object()["s"] << " = " << result.json.as_object()["p"] << "\n";

    }, "ethusdt@markPrice@1s");      // params for Websocket call

    using namespace std::chrono_literals;    
    std::this_thread::sleep_for(10s);

    return 0;
}
```

## User Data
See `examples\userdata.cpp` for a useful starting point.

Use the `BinanceBeast::startUserData()`, it's a standard websocket session. 

* User data has a key, "e", which is the eventType
* Listen keys expire after 60 minutes
* You should use `BinanceBeast::renewListenKey()` to extend the key within 60 minutes
* If the key expires you should call `BinanceBeast::startUserData()` to create a new key
  * When a key expires it does not close the websocket connection

NOTE: because user data relates to positions and orders, you won't receive anything unless positions are opened/closed or orders are filled.


```cpp
void onUserData(WsResult result)
{
    if (result.hasErrorCode())
    {
        std::cout << result.failMessage << "\n";
    }
    else
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
}
```

## How To
### Configuration
`BinanceBeast::start()` requires a `ConnectionConfig` which is created with either `ConnectionConfig::MakeTestNetConfig()` or `ConnectionConfig::MakeLiveConfig()`.

These functions have overloads:

* No API or secret key
* API key with optional secret key
* Read keys from a key file

A key file is a simple convenient way to avoid having keys in code. A key file is text file with three 3 lines:

```
<live | test>
<api key>
<secret key>
```

It is convenient during development and test, not necessarily for production.


### Start

```cpp
void start(const ConnectionConfig& config, const size_t nRestIoContexts = 4, const size_t nWebsockIoContexts = 6)
```
This allows you to set the number of `boost::asio::io_context` for REST and WebSockets.


### REST Requests
```cpp
void sendRestRequest(RestResponseHandler rc, const string& path, const RestSign sign, RestParams params = RestParams {});
```

`path` is the path on the Binance API docs, i.e. for All Orders , the path is `/fapi/v1/allOrders'.
`sign` tells BinanceBeast if it should add the `timestamp` and `signature` params. If the Binance API docs says "(HMAC SHA256)", i.e. for All Orders, then use RestSign::HMAC_SHA256. 

Calls which are signed require your secret key.

Signing reduces the risk of someone else sending a request to access your account/trading account.


