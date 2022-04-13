/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_VENDOR_BAT_NATIVE_ADS_SRC_BAT_ADS_INTERNAL_SERVER_GEO_SERVER_HOST_H_
#define BRAVE_VENDOR_BAT_NATIVE_ADS_SRC_BAT_ADS_INTERNAL_SERVER_GEO_SERVER_HOST_H_

#include <string>

#include "bat/ads/internal/server/server_host_base.h"

namespace ads {

class GeoServerHost final : public ServerHostBase {
 public:
  GeoServerHost();
  ~GeoServerHost() override;

  std::string Get() const override;
};

}  // namespace ads

#endif  // BRAVE_VENDOR_BAT_NATIVE_ADS_SRC_BAT_ADS_INTERNAL_SERVER_GEO_SERVER_HOST_H_