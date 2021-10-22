#pragma once

#include "function_detail.h"

template <typename F>
struct function;

template <typename R, typename... Args>
struct function<R(Args...)> {
  function() noexcept = default;
  ~function() noexcept = default;

  template <typename T>
  function(T func) : storage(std::move(func)) {}

  function(function const& other) {
    other.storage.ops->copy(other.storage, storage);
  }
  function(function&& other) noexcept {
    other.storage.ops->move(other.storage, storage);
  }

  function& operator=(function const& other) {
    if (this != &other) {
      function backup(std::move(*this));
      try {
        other.storage.ops->copy(other.storage, storage);
      } catch (...) {
        backup.storage.ops->move(backup.storage, storage);
        throw;
      }
    }
    return *this;
  }

  function& operator=(function&& other) noexcept {
    if (this != &other) {
      other.storage.ops->move(other.storage, storage);
    }
    return *this;
  }

  explicit operator bool() const noexcept {
    return storage.ops != function_detail::empty_ops<R, Args...>();
  }

  R operator()(Args&&... args) const {
    return storage.ops->invoke(storage, std::forward<Args>(args)...);
  }

  template <typename T>
  T* target() noexcept {
    if (storage.ops ==
        function_detail::ops_traits<T>::template get_ops<R, Args...>()) {
      return const_cast<T*>(storage.template get<T>());
    }
    return nullptr;
  }

  template <typename T>
  T const* target() const noexcept {
    if (storage.ops ==
        function_detail::ops_traits<T>::template get_ops<R, Args...>()) {
      return storage.template get<T>();
    }
    return nullptr;
  }

private:
  function_detail::storage<R, Args...> storage;
};
