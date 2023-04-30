/*
 * Copyright 2019 ThousandEyes, Inc.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 *
 * @author Giannis Georgalis, https://github.com/ggeorgalis
 */

#pragma once

#include <type_traits>

namespace thousandeyes {
namespace futures {
namespace detail {

// is_template

template <class C>
struct is_template : std::false_type {};

template <template <typename...> class C, typename... P>
struct is_template<C<P...>> : std::true_type {};

// nth_param

template <int N, typename T, typename... P>
struct nth_param {
    using type = typename nth_param<N - 1, P...>::type;
};

template <typename T, typename... P>
struct nth_param<0, T, P...> {
    using type = T;
};

// nth_template_param

template <int N, class C>
struct nth_template_param {
    using type = void;
};

template <int N, template <typename, typename...> class C, typename T, typename... P>
struct nth_template_param<N, C<T, P...>> {
    using type = typename nth_template_param<N - 1, C<P...>>::type;
};

template <template <typename, typename...> class C, typename T, typename... P>
struct nth_template_param<0, C<T, P...>> {
    using type = T;
};

} // namespace detail
} // namespace futures
} // namespace thousandeyes
