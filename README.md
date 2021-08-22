# Binance Beast

A C++ library for the Binance Futures exchange, using the boost Beast library for networking and Boost for json.

NOTE: the library has only been tested on Ubuntu. It *should* work on Windows but until I have time I can't confirm.

## Aims
- Performance: handle many Rest requests and websocket sessions
- Configurable: provide configuration on how io_context are used, particularly if issuing many Rest calls
- Usability: easy to build and use
- Documentation: provide tests/examples on how to use and best practises

## Quick Guide



## Build
You must have Git installed and a development environment installed (i.e. gcc, cmake). It has been developed with GCC 10.3.0.

* Clone repo
* Run script
  * Uses `vcpkg` to install the required libraries
  * Builds with `cmake`



