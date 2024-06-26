// Copyright Take Vos 2024.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "em_squares.hpp"
#include "pixels.hpp"
#include "points.hpp"
#include "dips.hpp"
#include "../macros.hpp"
#include <hikothird/au.hh>
#include <concepts>
#include <variant>

hi_export_module(hikogui.unit : length);

hi_export namespace hi {
inline namespace v1 { namespace unit {

template<typename T>
using length_variant =
    std::variant<au::Quantity<Points, T>, au::Quantity<Pixels, T>, au::Quantity<Dips, T>, au::Quantity<EmSquares, T>>;

template<typename T>
class length_quantity : public length_variant<T> {
    using super = length_variant<T>;

    using super::super;
};

using length_f = length_quantity<float>;
using length_s = length_quantity<short>;

}} // namespace v1::unit
}

template<std::integral T>
struct std::hash<hi::unit::length_quantity<T>> {
    [[nodiscard]] size_t operator()(hi::unit::length_quantity<T> const& rhs) const noexcept
    {
        auto h = std::hash<size_t>{}(rhs.index());
        h ^= std::visit(
            [](auto&& rhs_) {
                return std::hash<T>{}(rhs_);
            },
            rhs);
        return h;
    }
};
