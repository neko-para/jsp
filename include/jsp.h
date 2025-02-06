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
constexpr size_t count_v = std::tuple_size_v<std::tuple<Type...>>;

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
        context_->data = std::make_tuple<Type...>(std::forward<Args>(data)...);
        for (auto func : context_->thens) {
            impl::flat_invoke(func, context_->data.value());
        }
    }

private:
    std::shared_ptr<impl::Context<Type...>> context_;

public:
    using promise_type = impl::CppPromise<Type...>;

    friend class impl::CppPromise<Type...>;
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
    template <typename Value>
    void return_value(Value&& value)
        requires(count_v<Type...> == 1)
    {
        promise.resolve(std::forward<Value>(value));
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

}  // namespace impl

}  // namespace jsp
