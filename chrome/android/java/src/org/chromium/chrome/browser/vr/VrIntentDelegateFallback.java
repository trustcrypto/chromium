// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.content.Context;
import android.content.Intent;

/**
 * Fallback {@link VrIntentDelegate} implementation if the VR module is not available.
 */
public class VrIntentDelegateFallback extends VrIntentDelegate {
    @Override
    public Intent setupVrFreIntent(Context context, Intent freIntent) {
        // TODO(tiborg): Handle first run if VR module not installed.
        assert false;
        return freIntent;
    }

    @Override
    public void removeVrExtras(Intent intent) {
        assert false;
    }

    @Override
    public Intent setupVrIntent(Intent intent) {
        assert false;
        return intent;
    }
}
