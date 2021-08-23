# Binance Beast

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
    auto config = ConnectionConfig::MakeTestNetConfig();    // or MakeLiveConfig() when you're feeling brave
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
* The result handler is called from a `boost::thread_pool`, not from the `boost::asio::io_context` thread. The idea being if your handler takes time to process, it affect the networking processing thread(s)
* The are two instatiations of `boost::asio::io_context` , one for Rest calls and the other for Websockets
* I am considering creating a pool of `boost::asio::io_context` for the Websockets and distributing work evenly

---

## Aims
- Performance: handle many Rest requests and websocket sessions
- Configurable: provide configuration on how io_context are used, particularly if issuing many Rest calls
- Usability: easy to build and use
- Documentation: provide tests/examples on how to use and best practises


## Quick Guide

*NOTE: If you want frequent symbol information you should use the Websockets rather than Rest*

*NOTE: All API functions are asychronous.*


### Rest Calls
TODO

## Build
You must have Git installed and a development environment installed (i.e. gcc, cmake). It has been developed with GCC 10.3.0.

* Clone repo
* Run script
  * Uses `vcpkg` to install the required libraries
  * Builds with `cmake`



