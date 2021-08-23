#ifndef BB_TESTCOMMON_H
#define BB_TESTCOMMON_H

#include <binancebeast/BinanceBeast.h>

namespace bblib_test
{

    bool handleError(bblib::RestResult& result)
    {
        if (result.hasErrorCode())
        {
            std::cout << "REST Error: code = " << result.json.as_object()["code"] << "\nreason: " << result.json.as_object()["msg"] << "\n";   
            return true;
        }
        return false;
    }

    bool handleError(bblib::WsResult& result)
    {
        if (result.hasErrorCode())
        {
            std::cout << "WS Error: code = " << result.json.as_object()["code"] << "\nreason: " << result.json.as_object()["msg"] << "\n";   
            return true;
        }
        return false;
    }
}
#endif
