#pragma once

#include <type_traits>

template<class F>
    struct first_arg;

template<class C, class R, class A>
    struct first_arg<R(C::*)(A) const> { using type = std::decay_t<A>; };

template<class C, class R, class A>
    struct first_arg<R(C::*)(A)> { using type = std::decay_t<A>; };

template<class F>
    using first_arg_t = typename first_arg<decltype(&F::operator())>::type;
