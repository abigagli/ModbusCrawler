#pragma once
#include "modbus_types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace modbus {
void
single_read(modbus::rtu_parameters const &rp,
            int address,
            std::string regspec,
            bool verbose);

void
single_write(modbus::rtu_parameters const &rp,
             int address,
             intmax_t value,
             bool verbose);


void
file_transfer(modbus::rtu_parameters const &rp,
              int address,
              std::string filename,
              bool verbose);

void
flash_update(modbus::rtu_parameters const &rp,
             std::string filename,
             bool verbose);

} // namespace modbus