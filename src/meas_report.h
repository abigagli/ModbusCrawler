#pragma once

#include "meas_config.h"

#include <map>
#include <tuple>
#include <utility>

namespace measure {
class Report
{
public:
    using when_t =
      std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>;
    struct server_key_t
    {
        std::string server_name;
        int server_id;
        std::string to_string() const
        {
            return server_name + "@" + std::to_string(server_id);
        }
    };

    struct descriptor_t
    {
        std::chrono::seconds period;
        bool accumulating;
    };

private:
    struct stats_t
    {
        double min;
        double max;
        double mean;
        double stdev;
    };

    struct data_t
    {
        data_t() { reset(); }

        std::vector<std::pair<when_t, double>> samples;
        size_t num_failures;
        stats_t statistics;

        void reset()
        {
            samples.clear();
            num_failures = 0;
            statistics   = {};
        }
    };

    struct result_t
    {
        result_t(descriptor_t desc) : descriptor(desc) {}
        descriptor_t descriptor;
        data_t data;
    };

    using meas_key_t = std::string;

    std::map<server_key_t, std::map<meas_key_t, result_t>> results_;

    stats_t calculate_stats(decltype(data_t::samples) const &samples) const;

public:
    void add_entry(server_key_t const &sk,
                   std::string const &meas_name,
                   descriptor_t descriptor);

    void add_measurement(server_key_t const &sk,
                         std::string const &meas_name,
                         when_t when,
                         double value);

    void close_period();
};

inline bool
operator<(Report::server_key_t const &lhs, Report::server_key_t const &rhs)
{
    return std::tie(lhs.server_name, lhs.server_id) <
           std::tie(rhs.server_name, rhs.server_id);
}
} // namespace measure