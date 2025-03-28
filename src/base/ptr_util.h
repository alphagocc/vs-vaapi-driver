// Copyright 2015 The Chromium Authors
// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PTR_UTIL_H_
#define BASE_PTR_UTIL_H_

#include <memory>
#include <type_traits>

namespace libvavc8000d::base
{

// Helper to transfer ownership of a raw pointer to a std::unique_ptr<T>.
// Note that std::unique_ptr<T> has very different semantics from
// std::unique_ptr<T[]>: do not use this helper for array allocations.
template <typename T>
    requires(std::is_object_v<T> && !std::is_array_v<T>)
std::unique_ptr<T> WrapUnique(T *ptr)
{
    return std::unique_ptr<T>(ptr);
}

} // namespace libvavc8000d::base

#endif // BASE_PTR_UTIL_H_