#pragma once

#include "modbus_types.hpp"

#include <cassert>
#include <chrono>
#include <cinttypes>
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
class suval
{
    enum class Tag
    {
        SIGNED,
        UNSIGNED
    } type;
    union
    {
        intmax_t sval;
        uintmax_t uval;
    };

public:
    suval() : type{Tag::SIGNED}, sval{0} {}

    intmax_t as_signed() const
    {
        if (type != Tag::SIGNED)
            throw std::runtime_error("Current value is not SIGNED");
        return sval;
    }

    uintmax_t as_unsigned() const
    {
        if (type != Tag::UNSIGNED)
            throw std::runtime_error("Current value is not UNSIGNED");
        return uval;
    }

    template <class S>
    std::enable_if_t<std::is_signed<S>::value, suval&> operator=(S sv)
    {
        sval = sv;
        type = Tag::SIGNED;
        return *this;
    }

    template <class U>
    std::enable_if_t<std::is_unsigned<U>::value, suval&> operator=(U uv)
    {
        uval = uv;
        type = Tag::UNSIGNED;
        return *this;
    }

    suval& assign_min(modbus::value_type vt)
    {
        switch (vt)
        {
        case modbus::value_type::INT16:
            return operator=(std::numeric_limits<int16_t>::min());
        case modbus::value_type::UINT16:
            return operator=(std::numeric_limits<uint16_t>::min());
        case modbus::value_type::INT32:
            return operator=(std::numeric_limits<int32_t>::min());
        case modbus::value_type::UINT32:
            return operator=(std::numeric_limits<uint32_t>::min());
        case modbus::value_type::INT64:
            return operator=(std::numeric_limits<int64_t>::min());
        case modbus::value_type::UINT64:
            return operator=(std::numeric_limits<uint64_t>::min());
        default:
            assert(!"Unreachable switch case");
        }
    }

    suval& assign_max(modbus::value_type vt)
    {
        switch (vt)
        {
        case modbus::value_type::INT16:
            return operator=(std::numeric_limits<int16_t>::max());
        case modbus::value_type::UINT16:
            return operator=(std::numeric_limits<uint16_t>::max());
        case modbus::value_type::INT32:
            return operator=(std::numeric_limits<int32_t>::max());
        case modbus::value_type::UINT32:
            return operator=(std::numeric_limits<uint32_t>::max());
        case modbus::value_type::INT64:
            return operator=(std::numeric_limits<int64_t>::max());
        case modbus::value_type::UINT64:
            return operator=(std::numeric_limits<uint64_t>::max());
        default:
            assert(!"Unreachable switch case");
        }
    }

    suval& assign_from_string(std::string const& s, modbus::value_type vt)
    {
        if (modbus::value_signed(vt))
        {
            intmax_t const signed_value = std::strtoimax(s.c_str(), nullptr, 0);

            if (errno == ERANGE || !modbus::value_in_range(signed_value, vt))
                throw std::range_error(s + " OOR for " + to_string(vt));

            return operator=(signed_value);
        }
        else
        {
            // In the unsigned case, negative values would be wrapped-around
            // while parsing with strtoumax. Such wrap-around would be
            // detectable for all types with strictly less bits than uintmax_t
            // as a -N value would be wrapped into 2 ^ 64 - N, which would be
            // greater than the max value of any type with less than 64 bits and
            // hence could only be generated by a negative value being
            // wrapped-around. But for types with the same number of bits as
            // uintmax_t (i.e. UINT64) the wrap around is non-detectable, as the
            // wrapped-around value is always still in the allowed range for
            // UINT64, so we just check for negative values at the string level,
            // before attempting to parse, to be able to detect negative values
            // for all supported types
            auto const first_nonspace = s.find_first_not_of(' ', 0);
            uintmax_t unsigned_value;
            if (first_nonspace != std::string::npos && s[first_nonspace] == '-')
            {
                errno = ERANGE;
            }
            else
            {
                unsigned_value = std::strtoumax(s.c_str(), nullptr, 0);
            }

            if (errno == ERANGE || !modbus::value_in_range(unsigned_value, vt))
                throw std::range_error(s + " OOR for " + to_string(vt));

            return operator=(unsigned_value);
        }
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
    suval min_read_value;
    suval max_read_value;
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
read_config(std::string const& measconfig_file);

} // namespace measure