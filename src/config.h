// Copyright 2022 The Chromium Authors
// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FAKE_CONFIG_H_
#define FAKE_CONFIG_H_

#include <va/va.h>

#include <vector>

namespace libvavc8000d
{

// Class used for tracking a VAConfig and all information relevant to it.
// All objects of this class are immutable and thread safe.
class VSConfig
{
public:
    using IdType = VAConfigID;

    VSConfig(IdType id, VAProfile profile, VAEntrypoint entrypoint,
        std::vector<VAConfigAttrib> attrib_list);
    VSConfig(const VSConfig &) = delete;
    VSConfig &operator=(const VSConfig &) = delete;
    ~VSConfig();

    IdType GetID() const;
    VAProfile GetProfile() const;
    VAEntrypoint GetEntrypoint() const;
    const std::vector<VAConfigAttrib> &GetConfigAttribs() const;

private:
    const IdType id_;
    const VAProfile profile_;
    const VAEntrypoint entrypoint_;
    const std::vector<VAConfigAttrib> attrib_list_;
};

} // namespace libvavc8000d

#endif // FAKE_CONFIG_H_