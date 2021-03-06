#include "meas_reporter.h"

#include "json_support.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <loguru.hpp>
#include <nlohmann/json.hpp>
#include <sys/stat.h>

using nlohmann::json;

namespace nlohmann {
template <>
struct adl_serializer<std::pair<infra::when_t, double>>
{
    static void to_json(json &j, std::pair<infra::when_t, double> const &p)
    {
        j = json{{"t", p.first}, {"v", p.second}};
    }
};
} // namespace nlohmann

namespace {
double
fixed_digits(double number, int digits)
{
    auto const factor = std::pow(10, digits);
    return std::round(number * factor) / static_cast<double>(factor);
}
} // namespace

namespace measure {
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Reporter::descriptor_t,
                                   period,
                                   accumulating,
                                   report_raw_samples)

Reporter::Reporter(std::string out_folder) : out_folder_(std::move(out_folder))
{
    mkdir(out_folder_.c_str(), 0x777);
}

void
Reporter::configure_measurement(server_key_t const &sk,
                                std::string const &meas_name,
                                descriptor_t descriptor)
{
    auto &results_for_server = results_[sk];

    bool added;
    std::tie(std::ignore, added) =
      results_for_server.try_emplace(meas_name, descriptor);

    if (!added)
        throw std::invalid_argument(
          "configure_measurement: duplicate measure: " + meas_name +
          " for server " + sk.to_string());
}

void
Reporter::add_measurement(server_key_t const &sk,
                          std::string const &meas_name,
                          infra::when_t when,
                          double value,
                          SampleType sample_type)
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

    switch (sample_type)
    {
    case SampleType::regular:
        data.samples.emplace_back(when, value);
        break;
    case SampleType::read_failure:
        ++data.period_read_failures;
        ++data.total_read_failures;
        break;
    case SampleType::underflow:
        ++data.period_underflows;
        ++data.total_underflows;
        break;
    case SampleType::overflow:
        ++data.period_overflows;
        ++data.total_overflows;
        break;
    }
}

void
Reporter::close_period(infra::when_t now)
{
    ++period_id_;

    std::ofstream os = std::ofstream(out_folder_ + '/' +
                                     infra::to_compact_string(now) + ".json");

    LOG_S(INFO) << now.time_since_epoch().count() << "| closing period "
                << period_id_;

    json jreport{
      {"when", now},
      {"period_id", period_id_},
      {"servers", json::array()},
    };

    for (auto &server_el: results_)
    {
        json jserver{
          {"name", server_el.first.server_name},
          {"id", server_el.first.server_id},
          {"results", json::array()},
        };

        for (auto &result_el: server_el.second)
        {
            auto const &meas_name = result_el.first;
            auto &result          = result_el.second;

            json jresult;
            /** Fill result_t::descriptor **/
            json jdescriptor(result.descriptor);

            jresult["measure_name"] = meas_name;
            jresult["descriptor"]   = std::move(jdescriptor);
            /*******************************/

            /** Fill result_t::data **/
            json jdata{
              {"total_read_failures", result.data.total_read_failures},
              {"period_read_failures", result.data.period_read_failures},
              {"period_underflows", result.data.period_underflows},
              {"total_underflows", result.data.total_underflows},
              {"period_overflows", result.data.period_overflows},
              {"total_overflows", result.data.total_overflows},
            };
            jdata["num_samples"] = result.data.samples.size();

            if (!result.data.samples.empty())
            {
                result.data.statistics = calculate_stats(result.data.samples);

                jdata["statistics"] = {
                  {"min", fixed_digits(result.data.statistics.min, 3)},
                  {"max", fixed_digits(result.data.statistics.max, 3)},
                  {"mean", fixed_digits(result.data.statistics.mean, 3)},
                  {"stdev", fixed_digits(result.data.statistics.stdev, 3)}};
            }

            if (result.descriptor.report_raw_samples)
                jdata["samples"] = result.data.samples;

            jresult["data"] = std::move(jdata);
            /************************/

            jserver["results"].push_back(std::move(jresult));

            // Reset data, ready for next period
            result.data.reset();
        }

        jreport["servers"].push_back(std::move(jserver));
    }

    os << jreport.dump(2) << std::endl;
}

Reporter::stats_t
Reporter::calculate_stats(decltype(data_t::samples) const &samples)
{
    double sum   = 0;
    double min   = std::numeric_limits<double>::max();
    double max   = std::numeric_limits<double>::lowest();
    double mean  = std::numeric_limits<double>::quiet_NaN();
    double stdev = std::numeric_limits<double>::quiet_NaN();

    unsigned int valid_samples = 0;
    std::for_each(std::begin(samples),
                  std::end(samples),
                  [&](auto const &el)
                  {
                      if (!std::isnan(el.second))
                      {
                          ++valid_samples;
                          sum += el.second;
                          min = std::min(min, el.second);
                          max = std::max(max, el.second);
                      }
                  });

    if (valid_samples != 0)
    {
        mean = sum / static_cast<double>(valid_samples);

        // We calculated mean from the data, so we need at least 2 valid
        // samples since we divide by (valid_samples - 1). See
        // http://duramecho.com/Misc/WhyMinusOneInSd.html
        if (valid_samples > 1)
        {
            double accum = 0.0;
            std::for_each(std::begin(samples),
                          std::end(samples),
                          [&](auto const &el)
                          {
                              if (!std::isnan(el.second))
                                  accum +=
                                    (el.second - mean) * (el.second - mean);
                          });

            stdev = sqrt(accum / static_cast<double>(valid_samples - 1));
        }
        else // If there's just a single valid sample, stdev is simply 0
            stdev = 0;
    }
    else
    {
        // If we didn't find any valid sample, we need to set min and max
        // to NaN, otherwise we would incorrectly return the max() and
        // lowest() values used to search for min/max
        min = std::numeric_limits<double>::quiet_NaN();
        max = std::numeric_limits<double>::quiet_NaN();
    }

    return {min, max, mean, stdev};
}

} // namespace measure
