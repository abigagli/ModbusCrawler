#include "meas_config.h"

#include "json_support.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

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
NLOHMANN_JSON_SERIALIZE_ENUM(value_type,
                             {
                               {value_type::INT16, "INT16"},
                               {value_type::UINT16, "UINT16"},
                               {value_type::INT32, "INT32"},
                               {value_type::UINT32, "UINT32"},
                               {value_type::INT64, "INT64"},
                               {value_type::UINT64, "UINT64"},
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

    auto enabled_it = j.find("enabled");
    if (enabled_it != j.end())
        enabled_it->get_to(s.enabled);

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
                                   endianess,
                                   type,
                                   scale_factor)

void
to_json(json &j, measure_t const &m)
{
    j = json{{"name", m.name},
             {"accumulating", m.accumulating},
             {"sampling_period", m.sampling_period},
             {"report_raw_samples", m.report_raw_samples},
             {"value_type", m.value_type}};

    if (m.source)
    {
        j["source"] = m.source.value();
    }
}

void
from_json(json const &j, measure_t &m)
{
    j.at("name").get_to(m.name);

    auto enabled_it = j.find("enabled");
    if (enabled_it != j.end())
        enabled_it->get_to(m.enabled);

    auto acc_it = j.find("accumulating");
    if (acc_it != j.end())
        acc_it->get_to(m.accumulating);

    auto report_raw_it = j.find("report_raw_samples");
    if (report_raw_it != j.end())
        report_raw_it->get_to(m.report_raw_samples);

    j.at("sampling_period").get_to(m.sampling_period);

    auto source_it = j.find("source");

    if (source_it != j.end())
    {
        m.source = source_it->get<source_register_t>();
        // If there's a source specification, there's also must be a value_type
        j.at("value_type").get_to<modbus::value_type>(m.value_type);
    }
}


NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(descriptor_t, server, measures)

configuration_map_t
read_config(std::string const &measconfig_file)
{
    std::ifstream ifs(measconfig_file);
    json j;
    ifs >> j;

    std::map<server_id_t, descriptor_t> measure_descriptors;

    for (auto &desc: j.get<std::vector<descriptor_t>>())
    {
        if (!desc.server.enabled)
            continue;

        auto const server_id = desc.server.modbus_id;
        bool added;

        auto insertion =
          measure_descriptors.try_emplace(server_id, std::move(desc));

        if (!insertion.second)
            throw std::invalid_argument("Duplicate Modbus ID: " +
                                        std::to_string(server_id));

        // Prune non-enabled measures
        auto &measures = insertion.first->second.measures;
        measures.erase(std::remove_if(std::begin(measures),
                                      std::end(measures),
                                      [](auto const &el)
                                      { return !el.enabled; }),
                       std::end(measures));
    }

    return measure_descriptors;
}

} // namespace measure
