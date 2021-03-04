#include "meas_reporter.h"

#include "json_support.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <nlohmann/json.hpp>

using nlohmann::json;

namespace nlohmann {
template <>
struct adl_serializer<std::pair<measure::Reporter::when_t, double>>
{
    static void to_json(json &j,
                        std::pair<measure::Reporter::when_t, double> const &p)
    {
        j = json{{"timepoint", p.first.time_since_epoch().count()},
                 {"value", p.second}};
    }
};
} // namespace nlohmann

namespace measure {
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Reporter::descriptor_t,
                                   period,
                                   accumulating,
                                   report_raw_samples)

void
Reporter::add_entry(server_key_t const &sk,
                    std::string const &meas_name,
                    descriptor_t descriptor)
{
    auto &results_for_server = results_[sk];

    bool added;
    std::tie(std::ignore, added) =
      results_for_server.try_emplace(meas_name, descriptor);

    if (!added)
        throw std::invalid_argument(
          "add_entry: duplicate measure: " + meas_name + " for server " +
          sk.to_string());
}

void
Reporter::add_measurement(server_key_t const &sk,
                          std::string const &meas_name,
                          when_t when,
                          double value)
{
    auto server_it = results_.find(sk);

    if (server_it == std::end(results_))
        throw std::runtime_error("add_measurement: unknown server " +
                                 sk.to_string());

    auto &results_for_server = server_it->second;
    auto meas_it             = results_for_server.find(meas_name);

    if (meas_it == std::end(results_for_server))
        throw std::runtime_error("add_measurement: duplicate measure: " +
                                 meas_name + " for server " + sk.to_string());

    auto &data = meas_it->second.data;

    data.samples.emplace_back(when, value);
    if (std::isnan(value))
        ++data.num_failures;
}

void
Reporter::close_period()
{
    json jreport;
    for (auto &server_el: results_)
    {
        json jmeasure;
        std::string server_name = server_el.first.to_string();

        for (auto &result_el: server_el.second)
        {
            auto &meas_name = result_el.first;
            auto &result    = result_el.second;

            json jresult;
            /** Fill result_t::descriptor **/
            json jdescriptor(result.descriptor);

            jresult["descriptor"] = std::move(jdescriptor);
            /*******************************/

            /** Fill result_t::data **/
            json jdata{{"num_failures", result.data.num_failures}};

            if (!result.data.samples.empty())
                result.data.statistics = calculate_stats(result.data.samples);

            jdata["statistics"] = {{"min", result.data.statistics.min},
                                   {"max", result.data.statistics.max},
                                   {"mean", result.data.statistics.mean},
                                   {"stdev", result.data.statistics.stdev}};
            if (result.descriptor.report_raw_samples)
                jdata["samples"] = result.data.samples;

            jresult["data"] = std::move(jdata);
            /************************/

            // Attach result_t to the measure name
            jmeasure[result_el.first] = std::move(jresult);

            // Reset data, ready for next period
            result.data.reset();
        }

        jreport[server_name] = std::move(jmeasure);
    }

    std::cout << jreport.dump(2) << std::flush;
}

Reporter::stats_t
Reporter::calculate_stats(decltype(data_t::samples) const &samples)
{
    double sum = 0;
    double min = std::numeric_limits<double>::max();
    double max = std::numeric_limits<double>::min();

    std::for_each(std::begin(samples), std::end(samples), [&](auto const &el) {
        sum += el.second;
        min = std::min(min, el.second);
        max = std::max(max, el.second);
    });

    double const mean = sum / static_cast<double>(samples.size());

    double accum = 0.0;
    std::for_each(std::begin(samples), std::end(samples), [&](auto const &el) {
        accum += (el.second - mean) * (el.second - mean);
    });

    // We calculated mean from the data, so divide by (size - 1).
    // See http://duramecho.com/Misc/WhyMinusOneInSd.html
    double const stdev = sqrt(accum / static_cast<double>(samples.size() - 1));

    return {min, max, mean, stdev};
}


} // namespace measure
