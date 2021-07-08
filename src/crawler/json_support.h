#pragma once

#include <chrono>
#include <nlohmann/json_fwd.hpp>

namespace nlohmann {
template <class Rep, class Period>
struct adl_serializer<std::chrono::duration<Rep, Period>>
{
    static void to_json(json &j, std::chrono::duration<Rep, Period> const &d);
    static void from_json(json const &j, std::chrono::duration<Rep, Period> &d);
};

template <class Clock, class Duration>
struct adl_serializer<std::chrono::time_point<Clock, Duration>>
{
    static void to_json(json &j,
                        std::chrono::time_point<Clock, Duration> const &tp);
};
} // namespace nlohmann
