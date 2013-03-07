#ifndef GPD_QUOTE_HPP
#define GPD_QUOTE_HPP
#include <utility>
#endif

#ifndef GPD_QUOTE
#define GPD_QUOTE
#include "macros.hpp"

#define quotable(name)                                                  \
    struct unusable;                                                    \
    void name(unusable);                                                \
                                                                        \
    struct quoted_##name {                                              \
        template<class T0, class... Rest>                               \
        constexpr auto apply(long, T0&& t0, Rest&&... x)                \
            as(name(std::forward<T0>(t0), std::forward<Rest>(x)...));   \
                                                                        \
        template<class T>                                               \
        constexpr auto apply(int, T&& x) as(x.name);                    \
                                                                        \
        template<class T, class... Args>                                \
        constexpr auto apply(int, T&& x, Args&&... args)                \
            as(std::forward<T>(x).name(std::forward<Args>(args)...));   \
                                                                        \
        template<class... Args>                                         \
        constexpr auto operator()(Args&&...args)                        \
            as(apply(0L, std::forward<Args>(args)...));                 \
    };                                                                  \
    constexpr quoted_##name  $##name{};                                 \
/**/

#define quotable(name)                                                  \
    struct unusable;                                                    \
    void name(unusable);                                                \
                                                                        \
    struct quoted_##name {                                              \
        template<class T0, class... Rest>                               \
        constexpr auto apply(long, T0&& t0, Rest&&... x)                \
            as(name(std::forward<T0>(t0), std::forward<Rest>(x)...));   \
                                                                        \
        template<class T>                                               \
        constexpr auto apply(int, T&& x) as(x.name);                    \
                                                                        \
        template<class T, class... Args>                                \
        constexpr auto apply(int, T&& x, Args&&... args)                \
            as(std::forward<T>(x).name(std::forward<Args>(args)...));   \
                                                                        \
        template<class... Args>                                         \
        constexpr auto operator()(Args&&...args)                        \
            as(apply(0L, std::forward<Args>(args)...));                 \
    };                                                                  \
    constexpr quoted_##name  $##name{};                                 \
/**/

#else
#include "macros.hpp"

#undef GPD_QUOTE
#undef quotable
#endif
