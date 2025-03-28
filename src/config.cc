// Copyright 2022 The Chromium Authors
// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

namespace libvavc8000d
{

VSConfig::VSConfig(VSConfig::IdType id, VAProfile profile, VAEntrypoint entrypoint,
    std::vector<VAConfigAttrib> attrib_list)
    : id_(id), profile_(profile), entrypoint_(entrypoint), attrib_list_(std::move(attrib_list))
{}
VSConfig::~VSConfig() = default;

VSConfig::IdType VSConfig::GetID() const { return id_; }

VAProfile VSConfig::GetProfile() const { return profile_; }

VAEntrypoint VSConfig::GetEntrypoint() const { return entrypoint_; }

const std::vector<VAConfigAttrib> &VSConfig::GetConfigAttribs() const { return attrib_list_; }

} // namespace libvavc8000d