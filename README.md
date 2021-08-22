# Binance Beast

A C++ library for the Binance Futures exchange, using Boost's Beast and JSON libraries for networking (http/websockets) and json respectively. 


NOTE: the library has only been tested on Ubuntu. It *should* work on Windows but I can't confirm.


## Aims
- Performance: handle many Rest requests and websocket sessions
- Configurable: provide configuration on how io_context are used, particularly if issuing many Rest calls
- Usability: easy to build and use
- Documentation: provide tests/examples on how to use and best practises


## Quick Guide

*NOTE: If you want frequent symbol information you should use the Websockets rather than Rest*

*NOTE: All API functions are asychronous.*


### Rest Calls

#### Order Book

```cpp
int main (int argc, char ** argv)
{
  auto config = ConnectionConfig::MakeTestNetConfig();
  config.keys.api     = "e40fd4783309eed8285e5f00d60e19aa712ce0ecb4d449f015f8702ab1794abf";
  config.keys.secret  = "6c3d765d9223d2cdf6fe7a16340721d58689e26d10e6a22903dd76e1d01969f0";

  std::condition_variable cvHaveReply;
  std::mutex mux;


  BinanceBeast bb;

  bb.start(config);
  bb.allOrders(   [&](RestResult result)
                  {  
                      std::cout << result.json.as_array() << "\n";
                      cvHaveReply.notify_one();
                  },
                  RestParams {RestParams::QueryParams {{"symbol", "BTCUSDT"}}});

  std::unique_lock lck(mux);
  cvHaveReply.wait(lck);

  return 0;
}


```

## Build
You must have Git installed and a development environment installed (i.e. gcc, cmake). It has been developed with GCC 10.3.0.

* Clone repo
* Run script
  * Uses `vcpkg` to install the required libraries
  * Builds with `cmake`



