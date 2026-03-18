#pragma once

#include <type_traits>

template<class T>
    struct first_arg;

template<class R, class A>
    struct first_arg<R(*)(A)> {
        using type = std::decay_t<A>;
    };

template<class R, class A>
    struct first_arg<R(A)> {
        using type = std::decay_t<A>;
    };

template<class C, class R, class A>
    struct first_arg<R(C::*)(A) const> {
        using type = std::decay_t<A>;
    };

template<class C, class R, class A>
    struct first_arg<R(C::*)(A)> {
        using type = std::decay_t<A>;
    };

template<class F>
    using first_arg_fn_t = typename first_arg<std::remove_cvref_t<F>>::type;

template<class F>
    using first_arg_functor_t =
            typename first_arg<decltype(&std::remove_reference_t<F>::operator())>::type;

template<class F>
    concept FunctionPointerLike =requires(F &&f) {
        { +f };
    } &&
                                 std::is_pointer_v<std::remove_cvref_t<decltype(+std::declval<F>())>> &&
                                 std::is_function_v<std::remove_pointer_t<std::remove_cvref_t<decltype(+std::declval<F>())>>>;

template<class T>
    struct method_traits;

template<class C, class R, class A>
    struct method_traits<R(C::*)(A)> {
        using class_type = C;
        using arg_type = std::decay_t<A>;
    };

template<class C, class R, class A>
    struct method_traits<R(C::*)(A) const> {
        using class_type = C;
        using arg_type = std::decay_t<A>;
    };
