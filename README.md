# binancebeast

A C++ library for the Binance Futures exchange, using the boost Beast library for networking and Boost for json.

## Aims
- Performance: handle many Rest requests and websocket sessions
- Configurable: provide configuration on how io_context are used, particularly if issuing many Rest calls
- Usability: easy to build and use
- Documentation: provide tests/examples on how to use and best practises
