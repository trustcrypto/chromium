// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CONTENT_BROWSER_LANGUAGE_CODE_LOCATOR_H_
#define COMPONENTS_LANGUAGE_CONTENT_BROWSER_LANGUAGE_CODE_LOCATOR_H_

#include <string>
#include <vector>

#include "base/macros.h"

namespace language {

class LanguageCodeLocator {
 public:
  virtual ~LanguageCodeLocator(){};
  // Get suitable language codes given a coordinate.
  // If the latitude, longitude pair is not found, will return an empty vector.
  virtual std::vector<std::string> GetLanguageCode(double latitude,
                                                   double longitude) const = 0;
};

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CONTENT_BROWSER_LANGUAGE_CODE_LOCATOR_H_
