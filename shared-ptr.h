#pragma once

#include <cassert>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace sharedptr_details {
class control_block {
public:
  size_t get_strong() const;
  void inc_strong();
  void dec_strong();
  void inc_weak();
  void dec_weak();

protected:
  virtual void delete_data() = 0;
  virtual ~control_block() = default;

private:
  // NOTE: weak_cnt   = |strong| + |weak|
  //       strong_cnt = |strong|
  size_t strong_cnt{0};
  size_t weak_cnt{0};
};

template <typename T, typename D = std::default_delete<T>>
class ptr_block : public sharedptr_details::control_block, private D {
public:
  template <typename DeducedD = D>
  explicit ptr_block(T* ptr_, DeducedD&& deleter = D())
      : D(std::forward<DeducedD>(deleter)), ptr(ptr_) {
    // NOTE: we need DeducedD because if we just place D&& - it will mean not a
    // forwarding reference, but an r-value reference to D
    inc_strong();
  }

  T* get() {
    return ptr;
  }

protected:
  void delete_data() override {
    static_cast<D&> (*this)(ptr);
    ptr = nullptr;
  }

private:
  T* ptr{nullptr};
};

// no custom deleter since we manage the allocation/deallocation by ourselves
template <typename T>
class obj_block : public sharedptr_details::control_block {
public:
  template <typename... Args>
  explicit obj_block(Args&&... args) {
    new (&obj) T(std::forward<Args>(args)...);
  }

  T* get() {
    return reinterpret_cast<T*>(&obj);
  }

protected:
  void delete_data() override {
    get()->~T();
  }

private:
  std::aligned_storage_t<sizeof(T), alignof(T)> obj;
};
} // namespace sharedptr_details

template <typename T>
class weak_ptr;

template <typename T>
class shared_ptr {
  friend weak_ptr<T>;

  template <typename U, typename... Args>
  friend shared_ptr<U> make_shared(Args&&... args);

  // to retrieve control_block from shared_ptr of any type
  template <typename Y>
  friend class shared_ptr;

public:
  shared_ptr() noexcept = default;

  shared_ptr(std::nullptr_t) noexcept {}

  template <typename Y, typename D = std::default_delete<Y>,
            typename = std::enable_if_t<std::is_convertible_v<Y, T>>>
  explicit shared_ptr(Y* ptr_, D&& deleter = D()) {
    try {
      auto* p_block = new sharedptr_details::ptr_block<Y, D>(
          ptr_, std::forward<D>(deleter));
      // get the Y* before its type gets erased
      ptr = p_block->get();
      cb = p_block;
    } catch (...) {
      deleter(ptr_);
      throw;
    }
  }

  shared_ptr(const shared_ptr& other) noexcept : cb(other.cb), ptr(other.ptr) {
    safe_inc();
  }

  shared_ptr(shared_ptr&& other) noexcept : cb(other.cb), ptr(other.ptr) {
    other.nullify();
  }

  template <typename Y>
  shared_ptr(const shared_ptr<Y>& p, T* ptr_) : cb(p.cb), ptr(ptr_) {
    safe_inc();
  }

  template <typename Y>
  shared_ptr(const shared_ptr<Y>& p) : shared_ptr(p, p.get()) {}

  shared_ptr& operator=(const shared_ptr& other) noexcept {
    shared_ptr<T> tmp(other);
    swap(tmp);
    return *this;
  }

  shared_ptr& operator=(shared_ptr&& other) noexcept {
    shared_ptr<T> tmp(std::move(other));
    swap(tmp);
    return *this;
  }

  friend bool operator==(const shared_ptr<T>& lhs,
                         const shared_ptr<T>& rhs) noexcept {
    return lhs.ptr == rhs.ptr;
  }

  friend bool operator!=(const shared_ptr<T>& lhs,
                         const shared_ptr<T>& rhs) noexcept {
    return lhs.ptr != rhs.ptr;
  }

  T* get() const noexcept {
    return ptr;
  }

  operator bool() const noexcept {
    return get();
  }

  T& operator*() const noexcept {
    return *get();
  }

  T* operator->() const noexcept {
    return get();
  }

  std::size_t use_count() const noexcept {
    return cb ? cb->get_strong() : 0;
  }

  void reset() noexcept {
    if (cb) {
      cb->dec_strong();
    }
    nullify();
  }

  template <typename Y, typename D = std::default_delete<Y>,
            typename = std::enable_if_t<std::is_convertible_v<Y, T>>>
  void reset(Y* new_ptr, D&& deleter = D()) {
    *this = std::move(shared_ptr<Y>(new_ptr, std::forward<D>(deleter)));
  }

  ~shared_ptr() {
    reset();
  }

  void swap(shared_ptr<T>& other) {
    using std::swap;
    swap(cb, other.cb);
    swap(ptr, other.ptr);
  }

private:
  shared_ptr(sharedptr_details::control_block* cb_, T* ptr_)
      : cb(cb_), ptr(ptr_) {
    safe_inc();
  }

  void nullify() {
    ptr = nullptr;
    cb = nullptr;
  }

  void safe_inc() {
    if (cb) {
      cb->inc_strong();
    }
  }

private:
  sharedptr_details::control_block* cb{nullptr};
  T* ptr{nullptr};
};

template <typename T>
class weak_ptr {
public:
  weak_ptr() noexcept = default;

  weak_ptr(const shared_ptr<T>& other) noexcept : cb(other.cb), ptr(other.ptr) {
    safe_inc();
  }

  weak_ptr(const weak_ptr<T>& other) noexcept : cb(other.cb), ptr(other.ptr) {
    safe_inc();
  }

  weak_ptr(weak_ptr<T>&& other) noexcept : cb(other.cb), ptr(other.ptr) {
    other.cb = nullptr;
    other.ptr = nullptr;
  }

  weak_ptr& operator=(weak_ptr<T>&& other) noexcept {
    weak_ptr<T> tmp(std::move(other));
    swap(tmp);
    return *this;
  }

  weak_ptr& operator=(const weak_ptr<T>& other) noexcept {
    weak_ptr<T> tmp(other);
    swap(tmp);
    return *this;
  }

  weak_ptr& operator=(const shared_ptr<T>& other) noexcept {
    weak_ptr<T> tmp(other);
    swap(tmp);
    return *this;
  }

  shared_ptr<T> lock() const noexcept {
    if (cb && cb->get_strong() != 0)
      return shared_ptr<T>(cb, ptr);
    return shared_ptr<T>();
  }

  ~weak_ptr() {
    if (cb) {
      cb->dec_weak();
    }
    ptr = nullptr;
  }

  void swap(weak_ptr<T>& other) {
    using std::swap;
    swap(cb, other.cb);
    swap(ptr, other.ptr);
  }

private:
  void safe_inc() {
    if (cb) {
      cb->inc_weak();
    }
  }

private:
  sharedptr_details::control_block* cb{nullptr};
  T* ptr{nullptr};
};

template <typename T, typename... Args>
shared_ptr<T> make_shared(Args&&... args) {
  auto* o_block =
      new sharedptr_details::obj_block<T>(std::forward<Args>(args)...);
  shared_ptr<T> result(o_block, o_block->get());
  return result;
}
