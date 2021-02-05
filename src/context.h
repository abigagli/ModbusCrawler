#pragma once

#include <modbus.h>
#include <memory>
#include <string>

namespace modbus {

    class Context
    {
        std::unique_ptr<modbus_t> ctx_;

        public:
        Context ()
    }

}// namespace modbus