#pragma once

#if __cplusplus < 202002L
#error "C++20 is required for coroutine"
#endif

#include <coroutine>
#include <exception>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace jsp {

namespace impl {

template <typename... Type>
struct CppPromise;

template <typename... Type>
struct CppAwaiter;

template <typename... Type>
constexpr size_t count_v = std::tuple_size_v<std::tuple<Type...>>;

template <typename... Type>
    requires(count_v<Type...> >= 1)
using first_t = std::tuple_element_t<0, std::tuple<Type...>>;

template <typename... Type>
struct Context {
    std::mutex lock;
    std::optional<std::tuple<Type...>> data;
    std::list<std::function<void(const Type&...)>> thens;

    std::coroutine_handle<CppPromise<Type...>> handle;

    ~Context() {
        if (handle) {
            handle.destroy();
        }
    }
};

template <typename Func, typename Tuple, size_t... Ids>
auto flat_invoke_impl(Func&& func, const Tuple& tuple, std::index_sequence<Ids...>) {
    return func(std::get<Ids>(tuple)...);
}

template <typename Func, typename Tuple>
auto flat_invoke(Func&& func, const Tuple& tuple) {
    return flat_invoke_impl(func, tuple, std::make_index_sequence<std::tuple_size_v<Tuple>>());
}

}  // namespace impl

template <typename... Type>
class Promise {
public:
    Promise() : context_(std::make_shared<impl::Context<Type...>>()) {}
    Promise(const Promise&) = default;
    Promise(Promise&&) = default;
    ~Promise() = default;
    Promise& operator=(const Promise&) = default;
    Promise& operator=(Promise&&) = default;

    template <typename Func>
        requires(!std::is_same_v<std::invoke_result_t<Func, const Type&...>, void>)
    auto then(Func&& func) -> Promise<std::invoke_result_t<Func, const Type&...>> {
        std::unique_lock<std::mutex> lock(context_->lock);

        Promise<std::invoke_result_t<Func, const Type&...>> result;
        if (context_->data.has_value()) {
            result.resolve(impl::flat_invoke(func, context_->data.value()));
        } else {
            context_->thens.push_back([func = std::move(func), result](const Type&... data) mutable {
                result.resolve(func(data...));
            });
        }
        return result;
    }

    template <typename Func>
        requires(std::is_same_v<std::invoke_result_t<Func, const Type&...>, void>)
    auto then(Func&& func) -> Promise<> {
        std::unique_lock<std::mutex> lock(context_->lock);

        Promise<> result;
        if (context_->data.has_value()) {
            impl::flat_invoke(func, context_->data.value());
            result.resolve();
        } else {
            context_->thens.push_back([func = std::move(func), result](const Type&... data) mutable {
                func(data...);
                result.resolve();
            });
        }
        return result;
    }

    template <typename... Args>
        requires(impl::count_v<Args...> == impl::count_v<Type...>)
    void resolve(Args&&... data) {
        std::unique_lock<std::mutex> lock(context_->lock);

        if (context_->data.has_value()) {
            return;
        }
        context_->data = std::tuple<Type...>(std::forward<Args>(data)...);
        for (auto func : context_->thens) {
            impl::flat_invoke(func, context_->data.value());
        }
    }

    template <typename... Args>
    void resolve(Promise<Args...> pro) {
        pro.then([self = *this](const Args&... data) mutable {
            self.resolve_tuple(std::tuple<Type...>(data...));
        });
    }

    template <typename... Args>
    void resolve(Promise<Promise<Args...>> pro) {
        pro.then([self = *this](Promise<Args...> pro2) mutable {
            self.resolve(pro2);
        });
    }

    void resolve_tuple(std::tuple<Type...> data) {
        std::unique_lock<std::mutex> lock(context_->lock);

        if (context_->data.has_value()) {
            return;
        }
        context_->data = std::move(data);
        for (auto func : context_->thens) {
            impl::flat_invoke(func, context_->data.value());
        }
    }

    operator std::function<void(const Type&...)>() {
        return [self = *this](const Type&... args) mutable {
            auto data = std::tuple<Type...>(args...);
            self.resolve_tuple(data);
        };
    }

    // bool has_value() {
    //     std::unique_lock<std::mutex> lock(context_->lock);

    //     if (context_->data.has_value()) {
    //         return true;
    //     }
    //     if (!context_->handle || context_->handle.done()) {
    //         return false;
    //     }
    //     context_->handle.resume();
    //     return context_->data.has_value();
    // }

    operator bool() { return fulfill(); }

    std::optional<std::tuple<Type...>> take() {
        fulfill();

        std::unique_lock<std::mutex> lock(context_->lock);
        auto result = std::move(context_->data);
        context_->data = std::nullopt;
        return result;
    }

private:
    std::shared_ptr<impl::Context<Type...>> context_;

    bool fulfill() {
        std::unique_lock<std::mutex> lock(context_->lock);

        if (context_->data.has_value()) {
            return true;
        }

        if (context_->handle && !context_->handle.done()) {
            lock.unlock();
            context_->handle.resume();
            lock.lock();
        }
        return context_->data.has_value();
    }

public:
    using promise_type = impl::CppPromise<Type...>;

    friend class impl::CppPromise<Type...>;
    friend class impl::CppAwaiter<Type...>;

    impl::CppAwaiter<Type...> operator co_await();
};

template <typename Type>
constexpr bool is_promise_v = false;

template <typename... Type>
constexpr bool is_promise_v<Promise<Type...>> = true;

template <typename Type>
concept is_promise = is_promise_v<Type>;

namespace impl {

template <typename... Type>
constexpr bool has_promise_impl() {
    return (is_promise_v<Type> || ...);
}

}  // namespace impl

template <typename... Type>
constexpr bool has_promise_v = impl::has_promise_impl<Type...>();

template <typename... Type>
    requires(has_promise_v<Type...>)
class Promise<Type...> {
    static_assert(false, "Promise shouldn't recursive");
};

namespace impl {

template <typename... Type>
struct CppPromise {
    Promise<Type...> promise;

    CppPromise() = default;

    Promise<Type...> get_return_object() {
        promise.context_->handle = std::coroutine_handle<CppPromise<Type...>>::from_promise(*this);
        return promise;
    }
    std::suspend_never initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void return_value(std::tuple<Type...> value) { promise.resolve_tuple(std::move(value)); }
    void return_value(first_t<Type...> value)
        requires(count_v<Type...> == 1)
    {
        promise.resolve(std::move(value));
    }
    template <typename... Args>
    void return_value(Promise<Args...> pro) {
        promise.resolve(pro);
    }
    std::suspend_always yield_value(std::tuple<Type...> value) {
        promise.resolve_tuple(std::move(value));
        return {};
    }
    std::suspend_always yield_value(first_t<Type...>&& value)
        requires(count_v<Type...> == 1)
    {
        promise.resolve(std::move(value));
        return {};
    }
    void unhandled_exception() { std::rethrow_exception(std::current_exception()); }
};

template <>
struct CppPromise<> {
    Promise<> promise;

    CppPromise() = default;

    Promise<> get_return_object() {
        promise.context_->handle = std::coroutine_handle<CppPromise<>>::from_promise(*this);
        return promise;
    }
    std::suspend_never initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void return_void() { promise.resolve(); }
    void unhandled_exception() { std::rethrow_exception(std::current_exception()); }
};

template <typename... Type>
struct CppAwaiter {
    Promise<Type...> promise;

    bool await_ready() {
        std::unique_lock<std::mutex> lock(promise.context_->lock);

        return promise.context_->data.has_value();
    }
    void await_suspend(std::coroutine_handle<> h) {
        promise.then([h](const Type&...) {
            h.resume();
        });
    }
    std::tuple<Type...> await_resume()
        requires(count_v<Type...> > 1)
    {
        std::unique_lock<std::mutex> lock(promise.context_->lock);

        return promise.context_->data.value();
    }
    first_t<Type...> await_resume()
        requires(count_v<Type...> == 1)
    {
        std::unique_lock<std::mutex> lock(promise.context_->lock);

        return std::get<0>(promise.context_->data.value());
    }
};

template <>
struct CppAwaiter<> {
    Promise<> promise;

    bool await_ready() {
        std::unique_lock<std::mutex> lock(promise.context_->lock);
        return promise.context_->data.has_value();
    }
    void await_suspend(std::coroutine_handle<> h) {
        promise.then([h]() {
            h.resume();
        });
    }
    void await_resume() {}
};

}  // namespace impl

template <typename... Type>
impl::CppAwaiter<Type...> Promise<Type...>::operator co_await() {
    return {*this};
}

}  // namespace jsp
