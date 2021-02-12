#pragma once

#include "modbus_types.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <chrono>

namespace nlohmann
{
    template <class Rep, class Period>
    struct adl_serializer<std::chrono::duration<Rep, Period>>
    {
        static void to_json(json &j, std::chrono::duration<Rep, Period> const &d)
        {
            j = d.count();
        }
        static void from_json(json const &j, std::chrono::duration<Rep, Period> &d)
        {
            d = std::chrono::duration<Rep, Period>(j.get<Rep>());
        }
    };
}// namespace nlohmann

namespace modbus
{
NLOHMANN_JSON_SERIALIZE_ENUM(word_endianess,
                             {
                               {word_endianess::little, "little"},
                               {word_endianess::big, "big"},
                               {word_endianess::dontcare, nullptr},
                             })

NLOHMANN_JSON_SERIALIZE_ENUM(regtype,
                             {
                               {regtype::holding, "holding"},
                               {regtype::input, "input"},
                             })

}// namespace modbus

namespace measure
{
using json = nlohmann::json;
using namespace std::chrono_literals;

struct server_t
{
    int server_id;
    std::string serial_device;

    // Optionally present in json, so they have default values
    std::string line_config = "9600:8:N:1";
    std::chrono::milliseconds answering_time = 500ms;
    //NLOHMANN_DEFINE_TYPE_INTRUSIVE(server_t, server_id, serial_device, line_config, answering_time_ms)
};

// Can't rely on the handy NLOHMANN_DEFINE_TYPE_INTRUSIVE macro
// for server_t as I want to allow for default values for line_config
// and answering_time_ms members, which means the corresponding json keys
// might be missing
inline void
to_json(json &j, server_t const &s)
{
    j = json{{"server_id", s.server_id},
             {"serial_device", s.serial_device},
             {"line_config", s.line_config},
             {"answering_time_ms", s.answering_time}};
}

void
inline from_json(json const &j, server_t &s)
{
    j.at("server_id").get_to(s.server_id);
    j.at("serial_device").get_to(s.serial_device);

    // Handle the optionality...
    auto const lc_it = j.find("line_config");
    if (lc_it != j.end())
        lc_it->get_to(s.line_config);

    auto const at_it = j.find("answering_time_ms");
    if (at_it != j.end())
        at_it->get_to(s.answering_time);
}


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
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(source_register_t, address, size, endianess, type, scale_factor)
    } source;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(measure_t, name, accumulating, sampling_period, source)
};

struct descriptor_t
{
    server_t modbus_server;
    std::vector<measure_t> measures;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(descriptor_t, modbus_server, measures)
};

using server_id_t = int;

inline std::map<server_id_t, descriptor_t>
read_config(std::string const &measconfig_file)
{
    std::ifstream ifs(measconfig_file);
    json j;
    ifs >> j;

    std::map<server_id_t, descriptor_t> measure_descriptors;

    for (auto const &desc : j.get<std::vector<descriptor_t>>())
    {
        auto const server_id = desc.modbus_server.server_id;
        measure_descriptors.emplace(server_id, std::move(desc));
    }

    return measure_descriptors;
}
}// namespace measure