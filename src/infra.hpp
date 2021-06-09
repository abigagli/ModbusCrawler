#pragma once
#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>

namespace infra {
using when_t =
  std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>;

[[nodiscard]] inline std::string
to_compact_string(when_t when)
{
    std::tm tm{};
    auto const tt = when_t::clock::to_time_t(when);
    gmtime_r(&tt, &tm);

    std::ostringstream name;
    name << std::setfill('0') << std::setw(2) << tm.tm_year - 100
         << std::setw(2) << tm.tm_mon + 1 << std::setw(2) << tm.tm_mday
         << std::setw(2) << tm.tm_hour << std::setw(2) << tm.tm_min;

    return name.str();
}

} // namespace infra