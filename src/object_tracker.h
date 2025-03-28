// Copyright 2022 The Chromium Authors
// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OBJECT_TRACKER_H_
#define OBJECT_TRACKER_H_

#include <algorithm>
#include <memory>
#include <mutex>
#include <vector>


#include "base/logging.h"

namespace libvavc8000d
{

// Class that manages and stores the objects that VSDriver needs to keep
// track of. All public methods are thread-safe.
template <class T> class ObjectTracker
{
    static_assert(std::is_integral<typename T::IdType>::value);

public:
    ObjectTracker() = default;
    ObjectTracker(const ObjectTracker &) = delete;
    ObjectTracker &operator=(const ObjectTracker &) = delete;
    ~ObjectTracker() = default;

    template <class... Args> typename T::IdType CreateObject(Args &&...args)
    {
        const std::lock_guard<std::mutex> lock(lock_);

        // This ConstructObject<Args...>() trick creates an object of type T
        // preferring its constructor. If the constructor is not available (e.g.,
        // it's private), it creates the object using a static T::Create() method.
        objects_.push_back(ConstructObject<Args...>(next_id_, std::forward<Args>(args)...));
        CHECK(objects_.back());

        do {
            CHECK_LT(next_id_, std::numeric_limits<typename T::IdType>::max());
            next_id_++;
        } while (std::find_if(objects_.begin(), objects_.end(),
                     [this](const std::unique_ptr<T> &it) { return it->GetID() == next_id_; })
            != objects_.end());

        return objects_.back()->GetID();
    }

    bool ObjectExists(typename T::IdType id)
    {
        const std::lock_guard<std::mutex> lock(lock_);

        return std::find_if(objects_.begin(), objects_.end(), [id](const std::unique_ptr<T> &it) {
            return it->GetID() == id;
        }) != objects_.end();
    }

    const T &GetObject(typename T::IdType id)
    {
        const std::lock_guard<std::mutex> lock(lock_);

        auto object = std::find_if(objects_.begin(), objects_.end(),
            [id](const std::unique_ptr<T> &it) { return it->GetID() == id; });

        CHECK(object != objects_.end());

        return **object;
    }

    void DestroyObject(typename T::IdType id)
    {
        const std::lock_guard<std::mutex> lock(lock_);
        auto object = std::find_if(objects_.begin(), objects_.end(),
            [id](const std::unique_ptr<T> &it) { return it->GetID() == id; });

        CHECK(object != objects_.end());

        objects_.erase(object);

        if (id < next_id_) { next_id_ = id; }
    }

private:
    // Constructs an object of type T through the regular constructor using |id|
    // and |args|.
    template <class... Args,
        typename std::enable_if<std::is_constructible_v<T, typename T::IdType, Args...>, bool>::type
        = true>
    static std::unique_ptr<T> ConstructObject(typename T::IdType id, Args &&...args)
    {
        return std::make_unique<T>(id, std::forward<Args>(args)...);
    }

    // Constructs an object of type T through the T::Create() static method using
    // |id| and |args|.
    template <class... Args,
        typename std::enable_if<!std::is_constructible_v<T, typename T::IdType, Args...>,
            bool>::type
        = true>
    static std::unique_ptr<T> ConstructObject(typename T::IdType id, Args &&...args)
    {
        return T::Create(id, std::forward<Args>(args)...);
    }

    std::mutex lock_;
    std::vector<std::unique_ptr<T>> objects_;
    typename T::IdType next_id_ = std::numeric_limits<typename T::IdType>::min();
};

} // namespace libvavc8000d

#endif // OBJECT_TRACKER_H_