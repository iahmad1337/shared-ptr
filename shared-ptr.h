#pragma once

#include <cstddef>

template <typename T>
class shared_ptr {
public:
  shared_ptr() noexcept;
  shared_ptr(nullptr_t) noexcept;
  shared_ptr(T* ptr_);

  shared_ptr(const shared_ptr& other) noexcept;
  shared_ptr(shared_ptr&& other) noexcept;
  shared_ptr& operator=(const shared_ptr& other) noexcept;
  shared_ptr& operator=(shared_ptr&& other) noexcept;

  T* get() const noexcept;
  operator bool() const noexcept;
  T& operator*() const noexcept;
  T* operator->() const noexcept;

  std::size_t use_count() const noexcept;
  void reset() noexcept;
  void reset(T* new_ptr);

private:
};

template <typename T>
class weak_ptr {
public:
  weak_ptr() noexcept;
  weak_ptr(const shared_ptr<T>& other) noexcept;
  weak_ptr& operator=(const shared_ptr<T>& other) noexcept;

  shared_ptr<T> lock() const noexcept;

private:
};

template <typename T, typename... Args>
shared_ptr<T> make_shared(Args&&... args);
