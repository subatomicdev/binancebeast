A C++ library for the Binance Futures exchange, using Boost's Beast and JSON, developed and test on Ubuntu. There are no plans to support Windows.

The JSON returned from Binance is passed to response handlers and the BinanceBeast is lightweight API with functions for sending REST requests and starting Websocket sessions, rather than separate functions for specific calls. For example, rather than `getAllOrders()`, `getSymbolMarkPrice()` etc, all REST requests use `sendRestRequest()`. This means the BinanceBeast API is simpler and doesn't require updating if Binance add more endpoints or parameters.


_Disclaimer: The author(s) of Binance Beast are not liable for any losses (be it financial or otherwise), caused by irresponsible trades or from bugs in the Binance Beast library. Trading is risky, 90% of consumer traders will lose money. By using Binance Beast you agree to this._


## Status

REST, WebSockets and User Data fully supported for USD-M and COIN-M.


**Updates**

30th August
* Added util function, `urlEncode()`, and example for batch orders call
* Added overload of `startWebSocket()` and example to create a combined stream


29th August
* Support for COIN-M added. Requires you set the market when the config is created
 ```cpp
 // old
 auto config = ConnectionConfig::MakeTestNetConfig("your API key", "your secret key");
 auto config = ConnectionConfig::MakeTestNetConfig(filesysem::path{"/path/to/keyfile.txt"});
 
 // new, set Market to either USDM or COINM
 auto config = ConnectionConfig::MakeTestNetConfig(Market::USDM, "your API key", "your secret key");  
 auto config = ConnectionConfig::MakeTestNetConfig(Market::USDM, filesysem::path{"/path/to/keyfile.txt"});  
 ```

## Quick Guide

* Consider using Websockets rather than frequent REST calls 
* All API functions are asychronous, supplied with a callback function, either:
  *   `using RestResponseHandler = std::function<void(RestResponse)>`
  *   `using WebSocketResponseHandler = std::function<void(WsResponse)>`
* `RestResponse` and `WsResponse` contain the json, a `state` flag, `failMessage` and `hasErrorCode()`
*   If `hasErrorCode()` returns true, the `failMessage` is set
* There are multiple `boost::asio::io_context` for Rest and Websockets calls which are set with `BinanceBeast::start()`
  * Rest default is 4
  * Websockets default is 6


### Configuration
Configs store the API keys, they are created with `ConnectionConfig::MakeTestNetConfig()` or `ConnectionConfig::MakeLiveConfig()`, which have overloads:

* Pass a path to a key file
  *  A key file format is 3 lines, first line is "test" or "live", the next two lines are the API then secret key:
  ```
  test
  myapiKeyMyKey723423Ju&jNhuayNahas617238Jaiasjd31as52v46523435vs
  8LBwbPvcub5GHtxLgWDZnm23KFcXwXwXwXwLBwbLBwbAABBca-sdasdasdas123
  ```
* Supply just an API key, or an API and secret key

Which keys you need depends on the endpoints you use: https://binance-docs.github.io/apidocs/futures/en/#endpoint-security-type 


### REST

#### Get Orders
Get all orders for BTCUSDT:

```cpp
int main (int argc, char ** argv)
{
    // allOrders requires both keys
    auto config = ConnectionConfig::MakeTestNetConfig(Market::USDM, "YOUR API KEY", "YOUR SECRET KEY");
    
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
    auto config = ConnectionConfig::MakeTestNetConfig(Market::USDM, "YOUR API KEY", "YOUR SECRET KEY");
    
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

#### Batch Orders
A batch order is a JSON array with the same content as single order but it's URL encoded. Binance Beast provides a function to do this.

See `examples\neworder.cpp` for full code.


```cpp
boost::json::array order =
{
    {{"symbol", "BTCUSDT"}, {"side", "BUY"}, {"type", "MARKET"}, {"quantity", "0.001"}},
    {{"symbol", "BTCUSDT"}, {"side", "BUY"}, {"type", "MARKET"}, {"quantity", "0.001"}}
};

bb.sendRestRequest([&](RestResponse result)
{
    if (result.hasErrorCode())    
        std::cout << "Error: " << result.failMessage << "\n";
    else
        std::cout << "\nNew Order info:\n" << result.json << "\n";
},
"/fapi/v1/batchOrders",
RestSign::HMAC_SHA256,
RestParams{{{"batchOrders", BinanceBeast::urlEncode(json::serialize(order))}}},
RequestType::Post);
```


### WebSockets

#### Single Stream
A websocket stream is closed when the `BinanceBeast` object is destructed or calling `BinanceBeast::stopWebSocket()`.

Receive Mark Price for ETHUSDT for 10 seconds:

```cpp
int main (int argc, char ** argv)
{
    // you don't need API or secret keys for mark price
    auto config = ConnectionConfig::MakeTestNetConfig(Market::USDM);    // or MakeLiveConfig()
    
    BinanceBeast bb;

    bb.start(config);

    bb.startWebSocket([&](WsResponse result)      
    {  
        std::cout << result.json << "\n\n";

        if (result.hasErrorCode())
            std::cout << "\nError " << result.failMessage << "\n";
        else
            std::cout << "\n" << result.json.as_object()["s"] << " = " << result.json.as_object()["p"] << "\n";

    },
    "ethusdt@markPrice@1s");      // stream

    using namespace std::chrono_literals;    
    std::this_thread::sleep_for(10s);

    return 0;
}
```


#### Combined Streams
If you want to receive data from multiple streams but do so with one response handler/websocket stream, you can use a combined stream.

Use `BinanceBeast::startWebSocket (WebSocketResponseHandler handler, const std::vector<string>& streams)`.

See `examples\combinedstreams.cpp` for full code.

Receive the mark price for BTCUSDT and ETHUSDT in a combined stream:

```cpp
// each stream's data is pushed separately.
// in this case, we have two combined streams, the handler will be called once for btcusdt and again for ethusdt.
// the "stream" value contains the stream name
bb.startWebSocket([](WsResponse result)
{
    if (result.hasErrorCode())
        std::cout << "Error: " << result.failMessage << "\n";
    else
    {
        auto& object = result.json.as_object();
        auto& streamName = object["stream"];

        if (streamName == "btcusdt@markPrice@1s")
        {
            std::cout << "Mark price for BTCUSDT:\n" << object["data"] << "\n";
        }
        else if (streamName == "ethusdt@markPrice@1s")
        {
            std::cout << "Mark price for ETHUSDT:\n" << object["data"] << "\n";
        }
    }   
},
{{"btcusdt@markPrice@1s"}, {"ethusdt@markPrice@1s"}});
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
        std::cout << result.failMessage << "\n";
    else
    {
        auto topLevel = result.json.as_object();
        const auto eventType = json::value_to<string>(topLevel["e"]);

        if (eventType == "listenKeyExpired")
            std::cout << "listen key expired, renew with BinanceBeast::renewListenKey()\n";
        else if (eventType == "MARGIN_CALL")
            std::cout << "margin call\n";
        else if (eventType == "ACCOUNT_UPDATE")
            std::cout << "account update\n";
        else if (eventType == "ORDER_TRADE_UPDATE")
            std::cout << "order trade update\n";
        else if (eventType == "ACCOUNT_CONFIG_UPDATE")
            std::cout << "account config update\n";
    }
}
```


## Build
It has been developed with GCC 10.3.0 but older versions that support C++17 will work.

* `git clone --recurse-submodules -j8 https://github.com/subatomicdev/binancebeast.git binancebeast`
* `cd binancebeast`
* `./build_linux_x64_static_release.sh` or `./build_linux_x64_static_debug.sh`
  *  Uses `vcpkg` to install the required libraries. This installs cmake and ninja but it's local to vcpkg so will not affect existing installations
  * Configure and build
* After the build a short test runs (which doesn't require an API key)
* The library and test binary are in the lib and bin directories


### Linking
To link from your app:

* Add `binancebeast/lib/Release` and `binancebeast/lib/Debug` to link directories
* Add `-lbinancebeast` to target link libraries


### Dev
The `.vscode` directories are in the repo for convenience. If you use VS Code, just open the binancebeast folder in VS Code.
 
