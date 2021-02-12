#include "meas_config.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

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
// Can't rely on the handy NLOHMANN_DEFINE_TYPE_INTRUSIVE macro
// for server_t as I want to allow for default values for line_config
// and answering_time_ms members, which means the corresponding json keys
// might be missing
void
to_json(json &j, modbus_server_t const &s)
{
    j = json{{"modbus_id", s.modbus_id},
             {"name", s.name},
             {"serial_device", s.serial_device},
             {"line_config", s.line_config},
             {"answering_time_ms", s.answering_time}};
}

void
from_json(json const &j, modbus_server_t &s)
{
    j.at("modbus_id").get_to(s.modbus_id);
    j.at("serial_device").get_to(s.serial_device);
    j.at("name").get_to(s.name);

    // Handle the optionality...
    auto const lc_it = j.find("line_config");
    if (lc_it != j.end())
        lc_it->get_to(s.line_config);

    auto const at_it = j.find("answering_time_ms");
    if (at_it != j.end())
        at_it->get_to(s.answering_time);
}

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(measure_t::source_register_t, address, size, endianess, type, scale_factor)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(measure_t, name, accumulating, sampling_period, source)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(descriptor_t, server, measures)

configuration_map_t
read_config(std::string const &measconfig_file)
{
    std::ifstream ifs(measconfig_file);
    json j;
    ifs >> j;

    std::map<server_id_t, descriptor_t> measure_descriptors;

    for (auto const &desc : j.get<std::vector<descriptor_t>>())
    {
        auto const server_id = desc.server.modbus_id;
        auto [_, added] = measure_descriptors.try_emplace(server_id, std::move(desc));

        if (!added)
            throw std::invalid_argument("Duplicate Modbus ID: " + std::to_string(server_id));
    }

    return measure_descriptors;
}

}// namespace measure