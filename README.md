---
## NOTE: this is in the early stages and is subject to breaking changes
---

A C++ library for the Binance Futures exchange, using Boost's Beast and JSON libraries for networking (http/websockets) and json respectively. 

The library has is developed on Ubuntu and only tested on Ubuntu. Support for Windows may be added later.


## Status

### USD-M Futures
- Rest
  - All GET requests
  - Others coming soon
- WebSockets: All
- User Data: All

---

## Quick Guide

* Consider using Websockets rather than frequent REST calls 
* All API functions are asychronous, supplied with a callback function (`RestResponseHandler` or `WebSocketResponseHandler`):
  *   `using RestResponseHandler = std::function<void(RestResponse)>`
  *   `using WebSocketResponseHandler = std::function<void(WsResponse)>;`
* `RestResponse` and `WsResponse` contain the json, a `state` flag, `failMessage` and `hasErrorCode()`
*   If `hasErrorCode()` returns true, the `failMessage` is set
* There are multiple `boost::asio::io_context` for Rest and Websockets calls which are set with `BinanceBeast::start()`
  * Rest default is 4
  * Websockets default is 6


### Configuration
Configs store the API keys, they are created with `ConnectionConfig::MakeTestNetConfig()` or `ConnectionConfig::MakeLiveConfig()`, which have overloads:

* Pass a path to a key file
* Supply just an API key or API and secret key

Which keys you need depends on the endpoints you use: https://binance-docs.github.io/apidocs/futures/en/#endpoint-security-type 


### REST

#### Get Orders
Get all orders for BTCUSDT:

```cpp
int main (int argc, char ** argv)
{
    // allOrders requires both keys
    auto config = ConnectionConfig::MakeTestNetConfig("YOUR API KEY", "YOUR SECRET KEY");
    
    std::condition_variable cvHaveReply;

    BinanceBeast bb;

    bb.start(config);                                               // call once to start the networking processing loop

    bb.sendRestRequest([&](RestResponse result)                     // the RestResponseHandler
    {
        if (result.hasErrorCode())
            std::cout << "\nError: " << result.failMessage << "\n";
        else
            std::cout << "\n" << result.json << "\n";

        cvHaveReply.notify_one();
    },
    "/fapi/v1/allOrders",                                            // the stream path
    RestSign::HMAC_SHA256,                                           // this calls requires a signature
    RestParams{{{"symbol", "BTCUSDT"}}},                             // rest parameters
    RequestType::Get);                                               // this is a GET request

    
    std::mutex mux;
    std::unique_lock lck(mux);

    cvHaveReply.wait(lck);
    
    return 0;
}

```

#### New Order
```cpp
int main (int argc, char ** argv)
{
    // allOrders requires both keys
    auto config = ConnectionConfig::MakeTestNetConfig("YOUR API KEY", "YOUR SECRET KEY");
    // to notify main thread when we have a reply
    std::condition_variable cvHaveReply;


    BinanceBeast bb;

    // start the network processing
    bb.start(config);

    // create a new order
    bb.sendRestRequest([&](RestResponse result)
    {
        if (result.hasErrorCode())    
            std::cout << "Error: " << result.failMessage << "\n";
        else
            std::cout << "\nNew Order info:\n" << result.json << "\n";

        cvHaveReply.notify_one();
    },
    "/fapi/v1/order",
    RestSign::HMAC_SHA256,
    RestParams{{{"symbol", "BTCUSDT"}, {"side", "BUY"}, {"type", "MARKET"}, {"quantity", "0.001"}}},
    RequestType::Post);


    std::mutex mux;
    std::unique_lock lck(mux);    
    cvHaveReply.wait(lck);

    return 0;
}
```


### WebSockets
A websocket stream is closed when the `BinanceBeast` object is destructed, there is no close monitor function, this may be added later.

Receive Mark Price for ETHUSDT for 10 seconds:

```cpp
int main (int argc, char ** argv)
{
    // you don't need API or secret keys for mark price
    auto config = ConnectionConfig::MakeTestNetConfig();    // or MakeLiveConfig()
    
    BinanceBeast bb;

    bb.start(config);                           // call once to start the networking processing loop

    bb.startWebSocket([&](WsResponse result)      
    {  
        std::cout << result.json << "\n\n";

        if (result.hasErrorCode())
        {
            std::cout << "\nError " << result.failMessage << "\n";
        }
        else
        {
            // access symbol and mark price in the result
            std::cout << "\n" << result.json.as_object()["s"] << " = " << result.json.as_object()["p"] << "\n";
        }

    },
    "ethusdt@markPrice@1s");      // params

    using namespace std::chrono_literals;    
    std::this_thread::sleep_for(10s);

    return 0;
}
```


### User Data
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


## Build
It has been developed with GCC 10.3.0 but older versions that support C++17 will work.

* `git clone --recurse-submodules -j8 https://github.com/subatomicdev/binancebeast.git binancebeast`
* `cd binancebeast`
* `./build_linux_x64_static_release.sh`
  *  Uses `vcpkg` to install the required libraries. This installs cmake and ninja but it's local to vcpkg so will not affect existing installations
  * Configure and build
* After the build a short test runs (which doesn't require an API key)
* The library and test binary are in the lib and bin directories
  
