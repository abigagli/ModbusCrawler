#pragma once

#include "meas_config.h"

#include <limits>
#include <map>
#include <tuple>
#include <utility>

namespace measure {
class Reporter
{
public:
    enum class SampleType
    {
        regular,
        read_failure,
        underflow,
        overflow,
    };
    using when_t =
      std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>;
    struct server_key_t
    {
        std::string server_name;
        int server_id;
        [[nodiscard]] std::string to_string() const
        {
            return server_name + "@" + std::to_string(server_id);
        }
    };

    static std::string to_compact_string(when_t when);

    struct descriptor_t
    {
        std::chrono::seconds period;
        bool accumulating;
        bool report_raw_samples;
    };

private:
    struct stats_t
    {
        double min   = std::numeric_limits<double>::quiet_NaN();
        double max   = std::numeric_limits<double>::quiet_NaN();
        double mean  = std::numeric_limits<double>::quiet_NaN();
        double stdev = std::numeric_limits<double>::quiet_NaN();
    };

    struct data_t
    {
        std::vector<std::pair<when_t, double>> samples;
        size_t total_read_failures{};
        size_t period_read_failures{};
        size_t total_underflows{};
        size_t period_underflows{};
        size_t total_overflows{};
        size_t period_overflows{};
        stats_t statistics{};

        void reset()
        {
            samples.clear();
            period_read_failures = 0;
            period_underflows    = 0;
            period_overflows     = 0;
            statistics           = {};
        }
    };

    struct result_t
    {
        explicit result_t(descriptor_t desc) : descriptor(desc) {}
        descriptor_t descriptor;
        data_t data;
    };

    using meas_key_t = std::string;

    std::map<server_key_t, std::map<meas_key_t, result_t>> results_;
    unsigned int period_id_ = 0;
    std::string out_folder_;

    [[nodiscard]] static stats_t calculate_stats(decltype(data_t::samples)
                                                   const &samples);

public:
    Reporter(std::string out_folder);

    void configure_measurement(server_key_t const &sk,
                               std::string const &meas_name,
                               descriptor_t descriptor);

    void add_measurement(server_key_t const &sk,
                         std::string const &meas_name,
                         when_t when,
                         double value,
                         SampleType sample_type);

    void close_period();
};

inline bool
operator<(Reporter::server_key_t const &lhs, Reporter::server_key_t const &rhs)
{
    return std::tie(lhs.server_name, lhs.server_id) <
           std::tie(rhs.server_name, rhs.server_id);
}
} // namespace measure