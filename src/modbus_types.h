#pragma once

namespace modbus {

enum class word_endianess
{
    little,
    big,
    dontcare
};


enum class regtype
{
    holding,
    input
};
}// namespace modbus