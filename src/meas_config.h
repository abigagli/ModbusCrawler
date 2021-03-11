#pragma once

#include "modbus_types.h"

#include <chrono>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#if defined(__has_include)
#    if __has_include(<optional>)
#        include <optional>
using std::optional;
#    else
#        include <experimental/optional>
using std::experimental::optional;
#    endif
#endif


namespace measure {
struct modbus_server_t
{
    int modbus_id;
    std::string name;
    std::string serial_device;

    // Optionally present in json, so they have default values
    bool enabled            = true;
    std::string line_config = "9600:8:N:1";
    std::chrono::milliseconds answering_time{500};
};
struct source_register_t
{
    int address;
    modbus::word_endianess endianess;
    modbus::regtype type;
    double scale_factor;
};
struct measure_t
{
    std::string name;
    optional<source_register_t> source;
    std::chrono::seconds sampling_period;
    modbus::value_type value_type;

    bool enabled            = true;
    bool accumulating       = false;
    bool report_raw_samples = false;
};

struct descriptor_t
{
    modbus_server_t server;
    std::vector<measure_t> measures;
};

using server_id_t         = int;
using configuration_map_t = std::map<server_id_t, descriptor_t>;

configuration_map_t
read_config(std::string const &measconfig_file);

} // namespace measure
