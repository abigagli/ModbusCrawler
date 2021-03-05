#include "json_support.h"

#include <nlohmann/json.hpp>

namespace nlohmann {
template <class Rep, class Period>
void
adl_serializer<std::chrono::duration<Rep, Period>>::to_json(
  json &j,
  std::chrono::duration<Rep, Period> const &d)
{
    j = d.count();
}
template <class Rep, class Period>
void
adl_serializer<std::chrono::duration<Rep, Period>>::from_json(
  json const &j,
  std::chrono::duration<Rep, Period> &d)
{
    d = std::chrono::duration<Rep, Period>(j.get<Rep>());
}

template <class Clock, class Duration>
void
adl_serializer<std::chrono::time_point<Clock, Duration>>::to_json(
  json &j,
  std::chrono::time_point<Clock, Duration> const &tp)
{
    j = tp.time_since_epoch().count();
}

// Explicitly instantiate for the types we use, so that
// we can keep all the adl_serializer specialization in (this) cpp file
template struct adl_serializer<
  std::chrono::duration<std::chrono::seconds::rep,
                        std::chrono::seconds::period>>;
template struct adl_serializer<
  std::chrono::duration<std::chrono::milliseconds::rep,
                        std::chrono::milliseconds::period>>;

template struct adl_serializer<
  std::chrono::time_point<std::chrono::system_clock,
                          std::chrono::seconds>>;
} // namespace nlohmann
