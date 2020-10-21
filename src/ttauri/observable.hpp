// Copyright 2020 Pokitec
// All rights reserved.

#pragma once

#include "notifier.hpp"
#include "hires_utc_clock.hpp"
#include "numeric_cast.hpp"
#include "notifier.hpp"
#include "detail/observable_value.hpp"
#include "detail/observable_not.hpp"
#include "detail/observable_cast.hpp"
#include <memory>
#include <functional>
#include <algorithm>

namespace tt {

template<typename T>
class observable {
public:
    using value_type = T;
    using notifier_type = notifier<value_type>;
    using callback_type = typename notifier_type::callback_type;
    using callback_id_type = typename notifier_type::callback_id_type;
    using time_point = hires_utc_clock::time_point;
    using duration = hires_utc_clock::duration;

    observable(observable const &other) noexcept : pimpl(other.pimpl)
    {
        pimpl_callback = scoped_callback(*pimpl, [this](ttlet &tmp) {
            this->notifier(tmp);
        });
    }

    observable &operator=(observable const &other) noexcept
    {
        pimpl = other.pimpl;
        pimpl_callback = scoped_callback(*pimpl, [this](ttlet &tmp) {
            this->notifier(tmp);
        });
        this->notifier(this->load());
        return *this;
    }

    // Use the copy constructor and assignment operator.
    // observable(observable &&other) noexcept = delete;
    // observable &operator=(observable &&other) noexcept = delete;

    ~observable()
    {
        tt_assume(pimpl);
    }

    observable() noexcept :
        observable(std::static_pointer_cast<detail::observable_base<value_type>>(
            std::make_shared<detail::observable_value<value_type>>()))
    {
    }

    observable(value_type const &value) noexcept :
        observable(std::static_pointer_cast<detail::observable_base<value_type>>(
            std::make_shared<detail::observable_value<value_type>>(value)))
    {
    }

    template<typename Other>
    observable(observable<Other> const &other) noexcept :
        observable(std::static_pointer_cast<detail::observable_base<value_type>>(
            std::make_shared<detail::observable_cast<value_type, Other>>(other.pimpl)))
    {
    }

    template<typename Other>
    observable(Other const &other) noexcept :
        observable(std::static_pointer_cast<detail::observable_base<value_type>>(
            std::make_shared<detail::observable_cast<value_type, Other>>(
                std::make_shared<detail::observable_value<Other>>(other))))
    {
    }

    template<typename Other>
    observable &operator=(observable<Other> const &other) noexcept
    {
        return *this = std::static_pointer_cast<detail::observable_base<value_type>>(
                   std::make_shared<detail::observable_cast<value_type, Other>>(other.pimpl));
    }

    template<typename Other>
    observable &operator=(Other const &other) noexcept
    {
        return *this = std::static_pointer_cast<detail::observable_base<value_type>>(
                   std::make_shared<detail::observable_cast<value_type, Other>>(
                       std::make_shared<detail::observable_value<Other>>(other)));
    }

    observable &operator=(value_type const &value) noexcept
    {
        store(value);
        return *this;
    }

    observable &operator+=(value_type const &value) noexcept
    {
        store(load() + value);
        return *this;
    }

    [[nodiscard]] value_type previous_value() const noexcept
    {
        tt_assume(pimpl);
        return pimpl->previous_value();
    }

    /** Time when the value was modified last.
     */
    [[nodiscard]] time_point time_when_last_modified() const noexcept
    {
        tt_assume(pimpl);
        return pimpl->time_when_last_modified();
    }

    /** Duration since the value was last modified.
     */
    [[nodiscard]] duration duration_since_last_modified() const noexcept
    {
        tt_assume(pimpl);
        return pimpl->duration_since_last_modified();
    }

    /** The relative time since the start of the animation.
     * The relative time since the start of the animation according to the duration of the animation.
     *
     * @param A relative value between 0.0 and 1.0.
     */
    [[nodiscard]] float animation_progress(duration animation_duration) const noexcept
    {
        tt_assume(pimpl);
        return pimpl->animation_progress(animation_duration);
    }

    [[nodiscard]] bool animating(duration animation_duration) const noexcept
    {
        tt_assume(pimpl);
        return pimpl->animation_progress(animation_duration) < 1.0f;
    }

    [[nodiscard]] value_type load() const noexcept
    {
        tt_assume(pimpl);
        return pimpl->load();
    }

    [[nodiscard]] value_type operator*() const noexcept
    {
        tt_assume(pimpl);
        return pimpl->load();
    }

    [[nodiscard]] value_type load(duration animation_duration) const noexcept
    {
        tt_assume(pimpl);
        return pimpl->load(animation_duration);
    }

    bool store(value_type const &new_value) noexcept
    {
        tt_assume(pimpl);
        return pimpl->store(new_value);
    }

    [[nodiscard]] callback_id_type subscribe(callback_type callback) noexcept
    {
        return notifier.subscribe(callback);
    }

    void unsubscribe(callback_id_type id) noexcept
    {
        return notifier.unsubscribe(id);
    }

    [[nodiscard]] friend observable<bool> operator!(observable const &rhs) noexcept
    {
        return std::static_pointer_cast<detail::observable_base<bool>>(std::make_shared<detail::observable_not<bool>>(rhs.pimpl));
    }

    [[nodiscard]] friend bool operator==(observable const &lhs, observable const &rhs) noexcept
    {
        return *lhs == *rhs;
    }

    [[nodiscard]] friend bool operator==(observable const &lhs, value_type const &rhs) noexcept
    {
        return *lhs == rhs;
    }

    [[nodiscard]] friend bool operator==(value_type const &lhs, observable const &rhs) noexcept
    {
        return lhs == *rhs;
    }

    [[nodiscard]] friend bool operator!=(observable const &lhs, observable const &rhs) noexcept
    {
        return *lhs != *rhs;
    }

    [[nodiscard]] friend bool operator!=(observable const &lhs, value_type const &rhs) noexcept
    {
        return *lhs != rhs;
    }

    [[nodiscard]] friend bool operator!=(value_type const &lhs, observable const &rhs) noexcept
    {
        return lhs != *rhs;
    }

    [[nodiscard]] friend float to_float(observable const &rhs) noexcept
    {
        return numeric_cast<float>(rhs.load());
    }

    [[nodiscard]] friend float to_float(observable const &rhs, duration animation_duration) noexcept
    {
        ttlet previous_value = numeric_cast<float>(rhs.previous_value());
        ttlet current_value = numeric_cast<float>(rhs.load());
        ttlet animation_progress = rhs.animation_progress(animation_duration);
        return mix(animation_progress, previous_value, current_value);
    }

    [[nodiscard]] friend std::string to_string(observable const &rhs) noexcept
    {
        return to_string(rhs.load());
    }

    friend std::ostream &operator<<(std::ostream &lhs, observable const &rhs) noexcept
    {
        return lhs << rhs.load();
    }

private:
    using pimpl_type = detail::observable_base<value_type>;

    notifier_type notifier = {};
    std::shared_ptr<pimpl_type> pimpl = {};
    scoped_callback<pimpl_type> pimpl_callback = {};

    observable(std::shared_ptr<detail::observable_base<value_type>> const &value) noexcept : pimpl(value)
    {
        pimpl_callback = scoped_callback(*pimpl, [this](ttlet &tmp) {
            this->notifier(tmp);
        });
    }

    observable(std::shared_ptr<detail::observable_base<value_type>> &&value) noexcept : pimpl(std::move(value))
    {
        pimpl_callback = scoped_callback(*pimpl, [this](ttlet &tmp) {
            this->notifier(tmp);
        });
    }

    observable &operator=(std::shared_ptr<detail::observable_base<value_type>> const &value) noexcept
    {
        pimpl = value;
        pimpl_callback = scoped_callback(*pimpl, [this](ttlet &tmp) {
            this->notifier(tmp);
        });

        this->notifier(this->load());
        return *this;
    }
};

} // namespace tt
