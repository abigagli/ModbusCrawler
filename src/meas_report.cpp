#include "meas_report.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace measure {
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
    for (auto &el: results_)
    {
        std::string server       = el.first.to_string();
        auto &results_for_server = el.second;

        for (auto &result: results_for_server)
        {
            auto &statistics = result.second.data.statistics =
              calculate_stats(result.second.data.samples);

            std::cout << server << ": " << result.second.data.samples.size()
                      << ", " << result.second.data.num_failures << ": "
                      << statistics.min << ", " << statistics.max << ", "
                      << statistics.mean << ", " << statistics.stdev
                      << std::endl;

            result.second.data.reset();
        }
    }
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