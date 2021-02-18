#pragma once

#include "modbus_types.h"

#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <chrono>

namespace measure
{
using std::chrono_literals::operator""ms;
struct modbus_server_t
{
    int modbus_id;
    std::string name;
    std::string serial_device;

    // Optionally present in json, so they have default values
    std::string line_config = "9600:8:N:1";
    std::chrono::milliseconds answering_time = 500ms;
};

struct measure_t
{
    std::string name;
    bool accumulating;
    std::chrono::seconds sampling_period;
    struct source_register_t
    {
        int address;
        int size;
        modbus::word_endianess endianess;
        modbus::regtype type;
        double scale_factor;
    } source;
};

struct descriptor_t
{
    modbus_server_t server;
    std::vector<measure_t> measures;
};

using server_id_t = int;
using configuration_map_t = std::map<server_id_t, descriptor_t>;

configuration_map_t
read_config(std::string const &measconfig_file);

}// namespace measure
