#pragma once
#include <cassert>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace modbus {
namespace detail {

    template <class TARGET, class T>
    bool in_range_impl(T value)
    {
        auto const low  = static_cast<T>(std::numeric_limits<TARGET>::min());
        auto const high = static_cast<T>(std::numeric_limits<TARGET>::max());
        return value >= low && value <= high;
    }
} // namespace detail

enum class value_type
{
    INVALID = 0,
    INT16,
    UINT16,
    INT32,
    UINT32,
    INT64,
    UINT64
};

inline std::string
to_string(value_type vt)
{
    switch (vt)
    {
    case modbus::value_type::INT16:
        return "INT16";
    case modbus::value_type::UINT16:
        return "UINT16";
    case modbus::value_type::INT32:
        return "INT32";
    case modbus::value_type::UINT32:
        return "UINT32";
    case modbus::value_type::INT64:
        return "INT64";
    case modbus::value_type::UINT64:
        return "UINT64";
    default:
        assert(!"Unreachable switch case");
    }
}

enum class word_endianess
{
    INVALID = 0,
    little,
    big,
};

inline bool
value_signed(value_type vt)
{
    return vt == value_type::INT16 || vt == value_type::INT32 ||
           vt == value_type::INT64;
}


template <class T>
bool
value_in_range(T value, value_type vt)
{
    switch (vt)
    {
    case modbus::value_type::INT16:
        return detail::in_range_impl<int16_t>(value);
    case modbus::value_type::UINT16:
        return detail::in_range_impl<uint16_t>(value);
    case modbus::value_type::INT32:
        return detail::in_range_impl<int32_t>(value);
    case modbus::value_type::UINT32:
        return detail::in_range_impl<uint32_t>(value);
    case modbus::value_type::INT64:
        return detail::in_range_impl<int64_t>(value);
    case modbus::value_type::UINT64:
        return detail::in_range_impl<uint64_t>(value);
    default:
        assert(!"Unreachable switch case");
    }
}
inline int
reg_size(value_type vt)
{
    switch (vt)
    {
    case value_type::INT16:
    case value_type::UINT16:
        return 1;
    case value_type::INT32:
    case value_type::UINT32:
        return 2;
    case value_type::INT64:
    case value_type::UINT64:
        return 4;
    default:
        assert(!"Unreachable switch case");
    }
}

enum class regtype
{
    INVALID = 0,
    holding,
    input
};

template <class E>
void
check_enum(E e)
{
    if (static_cast<int>(e) == 0)
        throw std::runtime_error("Invalid enum");
}
} // namespace modbus