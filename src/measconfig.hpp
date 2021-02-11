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
    template <>
    struct adl_serializer<std::chrono::seconds>
    {
        static void to_json(json &j, std::chrono::seconds const &sec)
        {
            j = sec.count();
        }
        static void from_json(json const &j, std::chrono::seconds &sec)
        {
            sec = std::chrono::seconds(j.get<std::chrono::seconds::rep>());
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

using server_id_t = int;

inline std::map<server_id_t, std::vector<measure_t>>
read_config(std::string const &measconfig_file)
{
    std::ifstream ifs(measconfig_file);
    json j;
    ifs >> j;

    std::map<server_id_t, std::vector<measure_t>> configured_measures;

    for (auto const &el : j.get<std::vector<json>>())
    {
        auto server_id = el.at("server_id").get<int>();
        auto measures  = el.at("measures").get<std::vector<measure_t>>();
        configured_measures.emplace(server_id, std::move(measures));
    }

    return configured_measures;
}

}