#pragma once

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