#pragma once

struct bad_function_call : public std::exception {
  char const* what() const noexcept override {
    return "bad function call";
  }
};

namespace function_detail {
static int const SIZE = sizeof(void*);
static int const ALIGN = alignof(void*);

template <typename T>
constexpr bool fits_small_buf =
                                sizeof(T) <= SIZE
                                && ALIGN % alignof(T) == 0
                                && std::is_nothrow_move_constructible_v<T>;

template <typename R, typename... Args>
struct storage;

template <typename R, typename... Args>
struct operations {
  using storage = function_detail::storage<R, Args...>;

  R (*invoke)(storage const&, Args&&...);
  void (*destroy)(storage&) noexcept;
  void (*copy)(storage const& from, storage& to);
  void (*move)(storage& from, storage& to) noexcept;
};

template <typename R, typename... Args>
operations<R, Args...> const* empty_ops() {
  using operations = function_detail::operations<R, Args...>;
  using storage = function_detail::storage<R, Args...>;

  static constexpr operations instance = {
      /* invoke */
      [](storage const&, Args&&...) -> R {
        throw bad_function_call();
      },
      /* destroy */
      [](storage&) noexcept {},
      /* copy */
      [](storage const&, storage& to) {
        to.ops = empty_ops<R, Args...>();
      },
      /* move */
      [](storage&, storage& to) noexcept {
        to.ops = empty_ops<R, Args...>();
      }
  };
  return &instance;
}

template <typename T, typename = void>
struct ops_traits;

template <typename T>
struct ops_traits<T, std::enable_if_t<fits_small_buf<T>>> {
  template <typename R, typename... Args>
  static operations<R, Args...> const* get_ops() {
    using operations = function_detail::operations<R, Args...>;
    using storage = function_detail::storage<R, Args...>;

    static constexpr operations instance = {
        /* invoke */
        [](storage const& func, Args&&... args) -> R {
          return (*func.template get<T>())(std::forward<Args>(args)...);
        },
        /* destroy */
        [](storage& func) noexcept {
          func.ops = empty_ops<R, Args...>();
          func.template get<T>()->~T();
        },
        /* copy */
        [](storage const& from, storage& to) {
          to.ops->destroy(to);
          new (&to.buf) T(*from.template get<T>());
          to.ops = from.ops;
        },
        /* move */
        [](storage& from, storage& to) noexcept {
          to.ops->destroy(to);
          new (&to.buf) T(std::move(*from.template get<T>()));
          to.ops = from.ops;
          from.ops = empty_ops<R, Args...>();
        }
    };
    return &instance;
  }
};

template <typename T>
struct ops_traits<T, std::enable_if_t<!fits_small_buf<T>>> {
  template <typename R, typename... Args>
  static operations<R, Args...> const* get_ops() {
    using operations = function_detail::operations<R, Args...>;
    using storage = function_detail::storage<R, Args...>;

    static constexpr operations instance = {
        /* invoke */
        [](storage const& func, Args&&... args) -> R {
          return (*func.template get<T>())(std::forward<Args>(args)...);
        },
        /* destroy */
        [](storage& func) noexcept {
          func.ops = empty_ops<R, Args...>();
          delete func.template get<T>();
          func.template set_dynamic<T>(nullptr);
        },
        /* copy */
        [](storage const& from, storage& to) {
          to.ops->destroy(to);
          to.template set_dynamic<T>(new T(*from.template get<T>()));
          to.ops = from.ops;
        },
        /* move */
        [](storage& from, storage& to) noexcept {
          to.ops->destroy(to);
          to.template set_dynamic<T>(from.template get<T>());
          to.ops = from.ops;
          from.ops = empty_ops<R, Args...>();
        }
    };
    return &instance;
  }
};

template <typename R, typename... Args>
struct storage {
  storage() noexcept : buf(), ops(empty_ops<R, Args...>()) {}

  template <typename T>
  storage(T&& func) {
    if constexpr (fits_small_buf<T>) {
      new (&buf) T(std::move(func));
    } else {
      set_dynamic<T>(new T(std::move(func)));
    }
    ops = ops_traits<T>::template get_ops<R, Args...>();
  }

  ~storage() {
    ops->destroy(*this);
  }

  template <typename T>
  T const* get() const noexcept {
    if constexpr (fits_small_buf<T>) {
      return reinterpret_cast<T const*>(&buf);
    } else {
      return static_cast<T const*>(reinterpret_cast<void* const&>(buf));
    }
  }

  template <typename T>
  void set_dynamic(T const* obj) noexcept {
    reinterpret_cast<void const*&>(buf) = obj;
  }

  std::aligned_storage_t<SIZE, ALIGN> buf;
  operations<R, Args...> const* ops;
};

} // namespace function_detail
