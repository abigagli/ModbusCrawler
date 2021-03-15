#pragma once
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace modbus {

enum class word_endianess
{
    little,
    big,
    dontcare
};

enum class value_type
{
    INT16,
    UINT16,
    INT32,
    UINT32,
    INT64,
    UINT64
};

template <class T>
std::enable_if_t<std::is_signed_v<T>, T>
safe_to_signed(uint64_t val)
{
    int64_t signedval;
    if (val <= std::numeric_limits<int64_t>::max())
    {
        // Easy path: we're in signed value's positive range, just cast it
        signedval = static_cast<int64_t>(val);
    }
    else
    {
        // On the most common systems:
        // unsigned val -->
        //                  !(val <= INT_MAX) --> val >= INT_MIN.
        // (keep in mind INT_MIN gets converted to unsigned and hence represents
        // the mid-positive-range)
        // But then,
        // (val >= INT_MIN) -->
        //                  (val - INT_MIN) <= INT_MAX, i.e.
        // (val - INT_MIN) can be safely represented as signed

        signedval = // (int)(val - INT_MIN) + INT_MIN
          static_cast<int64_t>(
            val - static_cast<uint64_t>(std::numeric_limits<int64_t>::min())) +
          std::numeric_limits<int64_t>::min();
    }

    if (signedval < std::numeric_limits<T>::min() ||
        signedval > std::numeric_limits<T>::max())
        throw std::overflow_error("Value out of range: " +
                                  std::to_string(signedval));

    return static_cast<T>(signedval);
}


inline bool
value_signed(value_type vt)
{
    return vt == value_type::INT16 || vt == value_type::INT32 ||
           vt == value_type::INT64;
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
    }
}

enum class regtype
{
    holding,
    input
};
} // namespace modbus