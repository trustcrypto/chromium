// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_LAYOUT_AND_PAINT_ASYNC_THEN_H_
#define CONTENT_SHELL_TEST_RUNNER_LAYOUT_AND_PAINT_ASYNC_THEN_H_

#include "base/callback_forward.h"
#include "content/shell/test_runner/test_runner_export.h"

namespace blink {
class WebPagePopup;
class WebWidget;
}  // namespace blink

namespace test_runner {

// Triggers a layout and paint of WebWidget and the active popup (if any).
// Calls |callback| after the layout and paint happens for both the
// widget and the popup.
TEST_RUNNER_EXPORT void LayoutAndPaintAsyncThen(blink::WebPagePopup* popup,
                                                blink::WebWidget* web_widget,
                                                base::OnceClosure callback);

}  // namespace test_runner

#endif  // CONTENT_SHELL_TEST_RUNNER_LAYOUT_AND_PAINT_ASYNC_THEN_H_
