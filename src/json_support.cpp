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

template struct adl_serializer<
  std::chrono::duration<std::chrono::seconds::rep,
                        std::chrono::seconds::period>>;
template struct adl_serializer<
  std::chrono::duration<std::chrono::milliseconds::rep,
                        std::chrono::milliseconds::period>>;
} // namespace nlohmann
