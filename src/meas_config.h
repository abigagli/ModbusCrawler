#pragma once

#include "modbus_types.h"

#include <chrono>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#if defined(__has_include)
#    if __has_include(<optional>)
#        include <optional>
using std::optional;
#    else
#        include <experimental/optional>
using std::experimental::optional;
#    endif
#endif


namespace measure {
template <class T, class = void>
struct safe_signed;

struct safe_signed_tag
{
    struct min
    {};
    struct max
    {};
};
template <class S>
class safe_signed<S, typename std::enable_if<std::is_signed<S>::value>::type>:
  private safe_signed_tag
{
    S svalue;

    template <class TARGET>
    bool in_range_impl() const
    {
        return svalue >= static_cast<S>(std::numeric_limits<TARGET>::min()) &&
               svalue <= static_cast<S>(std::numeric_limits<TARGET>::max());
    }

    bool value_in_range(modbus::value_type vt)
    {
        switch (vt)
        {
        case modbus::value_type::INT16:
            return in_range_impl<int16_t>();
        case modbus::value_type::UINT16:
            return in_range_impl<uint16_t>();
        case modbus::value_type::INT32:
            return in_range_impl<int32_t>();
        case modbus::value_type::UINT32:
            return in_range_impl<uint32_t>();
        case modbus::value_type::INT64:
            return in_range_impl<int64_t>();
        case modbus::value_type::UINT64:
            return in_range_impl<uint64_t>();
        }
    }

public:
    safe_signed(min) : svalue(std::numeric_limits<S>::min()) {}

    safe_signed(max) : svalue(std::numeric_limits<S>::max()) {}

    // SFINAE-constrained constructor: can only construct from the exact same
    // type as S
    template <
      class S2,
      typename std::enable_if<std::is_same<S2, S>::value, int>::type = 0>
    safe_signed(S2 s, modbus::value_type vt) : svalue(s)
    {
        if (!value_in_range(vt))
            throw std::overflow_error(std::to_string(svalue));
    }

    // SFINAE-constrained constructor: can only construct from the unsigned
    // correspondent of S
    template <class U,
              typename std::enable_if<
                std::is_same<U, typename std::make_unsigned<S>::type>::value,
                int>::type = 0>
    safe_signed(U uvalue, modbus::value_type vt)
    {
        if (uvalue <= std::numeric_limits<S>::max())
        {
            // Easy path: we're in signed value's positive range, just cast
            // it
            svalue = static_cast<S>(uvalue);
        }
        else
        {
            // On the most common systems:
            // unsigned val -->
            //                  !(val <= INT_MAX) --> val >= INT_MIN.
            // (keep in mind INT_MIN gets converted to unsigned and hence
            // represents the mid-positive-range) But then, (val >= INT_MIN)
            // -->
            //                  (val - INT_MIN) <= INT_MAX, i.e.
            // (val - INT_MIN) can be safely represented as signed

            svalue = // (int)(val - INT_MIN) + INT_MIN
              static_cast<S>(uvalue -
                             static_cast<U>(std::numeric_limits<S>::min())) +
              std::numeric_limits<S>::min();
        }
        if (!value_in_range(vt))
            throw std::overflow_error(std::to_string(svalue));
    }

    template <class T,
              typename std::enable_if<
                std::is_same<T, S>::value ||
                  std::is_same<T, typename std::make_unsigned<S>::type>::value,
                int>::type = 0>
    T as() const noexcept
    {
        return static_cast<T>(svalue);
    }
};

struct modbus_server_t
{
    int modbus_id;
    std::string name;
    std::string serial_device;

    // Optionally present in json, so they have default values
    bool enabled            = true;
    std::string line_config = "9600:8:N:1";
    std::chrono::milliseconds answering_time{500};
    std::chrono::seconds sampling_period{5};
};
struct source_register_t
{
    int address;
    modbus::word_endianess endianess;
    modbus::regtype reg_type;
    modbus::value_type value_type;
    double scale_factor;
    // PROBLEM: We need to store the min/max values we are accepting when
    // reading from a device, and depending on the signedness of the read value,
    // we would need to accordingly store the thresholds as signed/unsigned. A
    // variant would do the trick, but the current gcc cross-compiler for arm
    // doesn't support variants, so here's the logic:
    // These threshold values are read from the configuration as uint64_t to
    // allow maximum positive range and if they are signed, we just leverage
    // the fact that signed->unsigned is a well-defined
    // wrap-around-based conversion (e.g. if the value is -100,
    // it will be read as (unsigned)(-100)).
    // But then they are stored as _signed_ int64_t (through
    // a "safe conversion" that avoids the non-portable, implementation-defined
    // unsigned->signed conversion). This is to optimize
    // the "runtime" comparisons with values read from the devices, because
    // when such values are unsigned we can just cast back again the thresholds
    // to unsigned (which is again the well-defined signed->unsigned conversion
    // we used when reading from the configuration). If we has stored them as
    // unsigned, we would then have to perform the "safe conversion" at runtime
    // each time we needed to compare with signed values read from the devices
    safe_signed<intmax_t> min_read_value{safe_signed_tag::min{}};
    safe_signed<intmax_t> max_read_value{safe_signed_tag::max{}};
};
struct measure_t
{
    std::string name;

    // Kind-of-optional... If present in the json, it overrides the server's
    // value, otherwise it will get set to the server's value when creating the
    // model object
    std::chrono::seconds sampling_period = std::chrono::seconds::zero();

    // These are not present when using a random source for testing
    optional<source_register_t> source;

    bool enabled            = true;
    bool accumulating       = false;
    bool report_raw_samples = false;
};

struct descriptor_t
{
    modbus_server_t server;
    std::vector<measure_t> measures;
};

using server_id_t         = int;
using configuration_map_t = std::map<server_id_t, descriptor_t>;

configuration_map_t
read_config(std::string const &measconfig_file);

} // namespace measure
