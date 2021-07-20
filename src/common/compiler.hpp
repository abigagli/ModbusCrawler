#pragma once

namespace compiler {

#if (__cplusplus >= 202002L)
#    include <type_traits>
template <class T>
using undeduced = std::type_identity;

#else
template <class T>
struct undeduced
{
    using type = T;
};

#endif

template <class T>
using undeduced_t = typename undeduced<T>::type;
} // namespace compiler