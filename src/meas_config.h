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
    std::chrono::seconds sampling_period;

    // These are not present when using a random source for testing
    optional<source_register_t> source;
    optional<modbus::value_type> value_type;

    // These values are read from strings in the configuration using std::stoull
    // (instead of std::stoll) to allow maximum positive range and exploit
    // well-defined signed->unsigned wrap-around based conversion But once
    // converted from string into uint64_t, they are stored as _signed_ int64_t
    // (through a "safe conversion" that avoids the implementation-defined
    // unsigned->signed conversion) because storing them as signed allows a safe
    // and portable re-conversion to uint64_t when they are used in comparisons
    // with unsigned values
    int64_t min_allowed = std::numeric_limits<int64_t>::min();
    int64_t max_allowed = std::numeric_limits<int64_t>::max();

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
