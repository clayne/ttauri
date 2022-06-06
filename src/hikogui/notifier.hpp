// Copyright Take Vos 2020.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "required.hpp"
#include "generator.hpp"
#include "loop.hpp"
#include "callback_flags.hpp"
#include <vector>
#include <tuple>
#include <functional>
#include <coroutine>

namespace hi::inline v1 {

/** A notifier which can be used to call a set of registered callbacks.
 *
 * @tparam Result The result of calling the callback.
 * @tparam Args The argument types of the callback function.
 */
template<typename T = void()>
class notifier {
};

// The partial template specialization allows the use of a `std::function`-like template
// argument, that looks like a function prototype.
template<typename Result, typename... Args>
class notifier<Result(Args...)> {
public:
    static_assert(std::is_same_v<Result, void>, "Result of a notifier must be void.");

    using result_type = Result;
    using function_type = std::function<Result(Args const&...)>;

    using token_type = std::shared_ptr<function_type>;
    using weak_token_type = std::weak_ptr<function_type>;

    /** An awaiter object which can wait on a notifier.
     *
     * When this object is awaited on a callback to this object is created
     * with the associated notifier. This allows a co-routine that awaits
     * on this object to be resumed.
     */
    class awaiter_type {
    public:
        constexpr awaiter_type() noexcept = default;
        constexpr awaiter_type(awaiter_type const&) noexcept = default;
        constexpr awaiter_type(awaiter_type&&) noexcept = default;
        constexpr awaiter_type& operator=(awaiter_type const&) noexcept = default;
        constexpr awaiter_type& operator=(awaiter_type&&) noexcept = default;

        constexpr awaiter_type(notifier& notifier) noexcept : _notifier(&notifier) {}

        [[nodiscard]] constexpr bool await_ready() noexcept
        {
            return false;
        }

        void await_suspend(std::coroutine_handle<> handle) noexcept
        {
            hi_axiom(_notifier != nullptr);

            // We can use the this pointer in the callback, as `await_suspend()` is called by
            // the co-routine on the same object as `await_resume()`.
            _cbt = _notifier->subscribe(callback_flags::main | callback_flags::once, [this, handle](Args const&...args) {
                // Copy the arguments received from the notifier into the awaitable object
                // So that it can be read using `await_resume()`.
                _args = {args...};

                // Resume the co-routine.
                handle.resume();
            });
        }

        constexpr void await_resume() const noexcept requires(sizeof...(Args) == 0) {}

        constexpr auto await_resume() const noexcept requires(sizeof...(Args) == 1)
        {
            return std::get<0>(_args);
        }

        constexpr auto await_resume() const noexcept requires(sizeof...(Args) > 1)
        {
            return _args;
        }

    private : notifier *_notifier = nullptr;
        token_type _cbt;
        std::tuple<Args...> _args;
    };

    /** Create a notifier.
     */
    constexpr notifier() noexcept = default;
    notifier(notifier&&) = delete;
    notifier(notifier const&) = delete;
    notifier& operator=(notifier&&) = delete;
    notifier& operator=(notifier const&) = delete;

    /** Create an awaiter that can await on this notifier.
     */
    awaiter_type operator co_await() const noexcept
    {
        return awaiter_type{const_cast<notifier&>(*this)};
    }

    /** Add a callback to the notifier.
     * Ownership of the callback belongs with the caller of `subscribe()`. The
     * caller will receive a token, a move-only RAII object that will unsubscribe the callback
     * when the token is destroyed.
     *
     * @param callback_ptr A shared_ptr to a callback function.
     * @return A RAII object which when destroyed will unsubscribe the callback.
     */
    [[nodiscard]] token_type subscribe(callback_flags flags, std::invocable<Args...> auto&& callback) noexcept
    {
        auto token = std::make_shared<function_type>(hi_forward(callback));
        _callbacks.emplace_back(token, flags);
        return token;
    }

    [[nodiscard]] token_type subscribe(std::invocable<Args...> auto&& callback) noexcept
    {
        return subscribe(callback_flags::synchronous, hi_forward(callback));
    }

    /** Call the subscribed callbacks with the given arguments.
     *
     * @note This function is not reentrant.
     * @param args The arguments to pass with the invocation of the callback
     */
    void operator()(Args const&...args) const noexcept
    {
        for (auto& callback : _callbacks) {
            if (is_synchronous(callback.flags)) {
                if (auto func = callback.lock()) {
                    (*func)(args...);
                }

            } else if (is_local(callback.flags)) {
                loop::local().post_function([=] {
                    if (auto func = callback.lock()) {
                        (*func)(args...);
                    }
                });

            } else if (is_main(callback.flags)) {
                loop::main().post_function([=] {
                    if (auto func = callback.lock()) {
                        (*func)(args...);
                    }
                });

            } else if (is_timer(callback.flags)) {
                loop::timer().post_function([=] {
                    if (auto func = callback.lock()) {
                        (*func)(args...);
                    }
                });

            } else {
                hi_no_default();
            }

            // If the callback should only be triggered once, like inside an awaitable.
            // Then reset the weak_ptr in _callbacks so that it will be cleaned up.
            // In the lambda above the weak_ptr is copied first so that it callback will get executed
            // as long as the shared_ptr's use count does not go to zero.
            if (is_once(callback.flags)) {
                callback.reset();
            }
        }
        clean_up();
    }

private:
    struct callback_type {
        weak_token_type token;
        callback_flags flags;

        [[nodiscard]] bool expired() const noexcept
        {
            return token.expired();
        }

        void reset() noexcept
        {
            token.reset();
        }

        [[nodiscard]] token_type lock() const noexcept
        {
            return token.lock();
        }
    };

    /** A list of callbacks and it's associated token.
     */
    mutable std::vector<callback_type> _callbacks;

    void clean_up() const noexcept
    {
        // Cleanup all callbacks that have expired, or when they may only be triggered once.
        std::erase_if(_callbacks, [](hilet& item) {
            return item.expired();
        });
    }

#if HI_BUILD_TYPE == HI_BT_DEBUG
    /** The notifier is currently calling all the callbacks.
     */
    mutable bool _notifying = false;
#endif
};

} // namespace hi::inline v1