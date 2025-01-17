// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBRUNNER_APP_CAST_APPLICATION_CONFIG_MANAGER_TEST_FAKE_APPLICATION_CONFIG_MANAGER_H_
#define WEBRUNNER_APP_CAST_APPLICATION_CONFIG_MANAGER_TEST_FAKE_APPLICATION_CONFIG_MANAGER_H_

#include <webrunner/fidl/chromium/cast/cpp/fidl.h>

#include "base/macros.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace castrunner {
namespace test {

// Test cast.ApplicationConfigManager implementation which maps a test Cast
// AppId to an embedded test server address.
class FakeApplicationConfigManager
    : public chromium::cast::ApplicationConfigManager {
 public:
  static const char kTestCastAppId[];

  explicit FakeApplicationConfigManager(
      net::EmbeddedTestServer* embedded_test_server);
  ~FakeApplicationConfigManager() override;

  // chromium::cast::ApplicationConfigManager interface.
  void GetConfig(fidl::StringPtr id,
                 GetConfigCallback config_callback) override;

 private:
  net::EmbeddedTestServer* embedded_test_server_;

  DISALLOW_COPY_AND_ASSIGN(FakeApplicationConfigManager);
};

}  // namespace test
}  // namespace castrunner

#endif  // WEBRUNNER_APP_CAST_APPLICATION_CONFIG_MANAGER_TEST_FAKE_APPLICATION_CONFIG_MANAGER_H_
