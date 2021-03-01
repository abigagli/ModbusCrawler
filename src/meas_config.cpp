#include "meas_config.h"

#include <nlohmann/json.hpp>
#include <tuple>

using json = nlohmann::json;

namespace nlohmann {
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
} // namespace nlohmann

namespace modbus {
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

} // namespace modbus

namespace measure {
// Can't rely on the handy NLOHMANN_DEFINE_TYPE_INTRUSIVE macro
// for server_t as I want to allow for default values and optionality...
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
    auto serial_device_it = j.find("serial_device");

    if (serial_device_it != j.end()) // A real modbus source
    {
        serial_device_it->get_to(s.serial_device);

        // Required fields
        j.at("modbus_id").get_to(s.modbus_id);
        j.at("name").get_to(s.name);

        // "Optional" fields, if they are missing, a default value will be used
        auto const lc_it = j.find("line_config");
        if (lc_it != j.end())
            lc_it->get_to(s.line_config);

        auto const at_it = j.find("answering_time_ms");
        if (at_it != j.end())
            at_it->get_to(s.answering_time);
    }
    else // This is a for-testing random source
    {
        // Hard code to some self-explanatory name...
        s.name = "RANDOM";

        // The only required fields
        j.at("modbus_id").get_to(s.modbus_id);
        j.at("line_config").get_to(s.line_config); // Encodes MEAN:STDEV
    }
}

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(source_register_t,
                                   address,
                                   size,
                                   endianess,
                                   type,
                                   scale_factor)

// NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(measure_t,
//                                    name,
//                                    accumulating,
//                                    sampling_period,
//                                    source)
void
to_json(json &j, measure_t const &m)
{
    j = json{{"name", m.name},
             {"accumulating", m.accumulating},
             {"sampling_period", m.sampling_period}};
    if (m.source)
    {
        j["source"] = m.source.value();
    }
}

void
from_json(json const &j, measure_t &m)
{
    j.at("name").get_to(m.name);

    auto acc_it = j.find("accumulating");
    if (acc_it != j.end())
        acc_it->get_to(m.accumulating);

    j.at("sampling_period").get_to(m.sampling_period);

    auto source_it = j.find("source");

    if (source_it != j.end())
        m.source = source_it->get<source_register_t>();
}


NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(descriptor_t, server, measures)

configuration_map_t
read_config(std::string const &measconfig_file)
{
    std::ifstream ifs(measconfig_file);
    json j;
    ifs >> j;

    std::map<server_id_t, descriptor_t> measure_descriptors;

    for (auto const &desc: j.get<std::vector<descriptor_t>>())
    {
        auto const server_id = desc.server.modbus_id;
        bool added;

        std::tie(std::ignore, added) =
          measure_descriptors.try_emplace(server_id, std::move(desc));

        if (!added)
            throw std::invalid_argument("Duplicate Modbus ID: " +
                                        std::to_string(server_id));
    }

    return measure_descriptors;
}

} // namespace measure