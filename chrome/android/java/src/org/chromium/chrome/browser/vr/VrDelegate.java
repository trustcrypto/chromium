// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.animation.Animator;
import android.animation.Animator.AnimatorListener;
import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.os.Build;
import android.os.Bundle;
import android.view.Display;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.ui.display.DisplayAndroid;

import java.lang.reflect.Method;

/** Delegate to call into VR. */
public abstract class VrDelegate {
    private static final String TAG = "VrDelegate";
    private static final String VR_BOOT_SYSTEM_PROPERTY = "ro.boot.vr";
    /* package */ static final boolean DEBUG_LOGS = false;
    /* package */ static final int VR_SYSTEM_UI_FLAGS = View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_FULLSCREEN
            | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
    // Android N doesn't allow us to dynamically control the preview window based on headset mode,
    // so we used an animation to hide the preview window instead.
    /* package */ static final boolean USE_HIDE_ANIMATION =
            Build.VERSION.SDK_INT < Build.VERSION_CODES.O;

    private static Boolean sBootsToVr;

    public abstract void forceExitVrImmediately();
    public abstract boolean onActivityResultWithNative(int requestCode, int resultCode);
    public abstract void onNativeLibraryAvailable();
    public abstract boolean isInVr();
    public abstract boolean canLaunch2DIntents();
    public abstract boolean onBackPressed();
    public abstract boolean enterVrIfNecessary();
    public abstract void maybeRegisterVrEntryHook(final ChromeActivity activity);
    public abstract void maybeUnregisterVrEntryHook();
    public abstract void onMultiWindowModeChanged(boolean isInMultiWindowMode);
    public abstract void requestToExitVrForSearchEnginePromoDialog(
            OnExitVrRequestListener listener, Activity activity);
    public abstract void requestToExitVr(OnExitVrRequestListener listener);
    public abstract void requestToExitVr(
            OnExitVrRequestListener listener, @UiUnsupportedMode int reason);
    public abstract void requestToExitVrAndRunOnSuccess(Runnable onSuccess);
    public abstract void requestToExitVrAndRunOnSuccess(
            Runnable onSuccess, @UiUnsupportedMode int reason);
    public abstract void onActivityShown(ChromeActivity activity);
    public abstract void onActivityHidden(ChromeActivity activity);
    public abstract boolean onDensityChanged(int oldDpi, int newDpi);
    public abstract void rawTopContentOffsetChanged(float topContentOffset);
    public abstract void onNewIntentWithNative(ChromeActivity activity, Intent intent);
    public abstract void maybeHandleVrIntentPreNative(ChromeActivity activity, Intent intent);

    public abstract void setVrModeEnabled(Activity activity, boolean enabled);
    public abstract void doPreInflationStartup(ChromeActivity activity, Bundle savedInstanceState);

    public boolean bootsToVr() {
        if (sBootsToVr == null) {
            // TODO(mthiesse): Replace this with a Daydream API call when supported.
            // Note that System.GetProperty is unable to read system ro properties, so we have to
            // resort to reflection as seen below. This method of reading system properties has been
            // available since API level 1.
            sBootsToVr = getIntSystemProperty(VR_BOOT_SYSTEM_PROPERTY, 0) == 1;
        }
        return sBootsToVr;
    }

    public abstract boolean isDaydreamReadyDevice();
    public abstract boolean isDaydreamCurrentViewer();
    public abstract boolean willChangeDensityInVr(ChromeActivity activity);
    public abstract void onSaveInstanceState(Bundle outState);

    /* package */ void setSystemUiVisibilityForVr(Activity activity) {
        activity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        int flags = activity.getWindow().getDecorView().getSystemUiVisibility();
        activity.getWindow().getDecorView().setSystemUiVisibility(flags | VR_SYSTEM_UI_FLAGS);
    }

    /* package */ void addBlackOverlayViewForActivity(ChromeActivity activity) {
        View overlay = activity.getWindow().findViewById(R.id.vr_overlay_view);
        if (overlay != null) return;
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                WindowManager.LayoutParams.MATCH_PARENT, WindowManager.LayoutParams.MATCH_PARENT);
        View v = new View(activity);
        v.setId(R.id.vr_overlay_view);
        v.setBackgroundColor(Color.BLACK);
        FrameLayout decor = (FrameLayout) activity.getWindow().getDecorView();
        decor.addView(v, params);
    }

    /* package */ void removeBlackOverlayView(Activity activity, boolean animate) {
        View overlay = activity.getWindow().findViewById(R.id.vr_overlay_view);
        if (overlay == null) return;
        FrameLayout decor = (FrameLayout) activity.getWindow().getDecorView();
        if (!animate) {
            decor.removeView(overlay);
        } else {
            overlay.animate()
                    .alpha(0)
                    .setDuration(activity.getResources().getInteger(
                            android.R.integer.config_mediumAnimTime))
                    .setListener(new AnimatorListener() {
                        @Override
                        public void onAnimationStart(Animator arg0) {}

                        @Override
                        public void onAnimationRepeat(Animator arg0) {}

                        @Override
                        public void onAnimationEnd(Animator arg0) {
                            decor.removeView(overlay);
                        }

                        @Override
                        public void onAnimationCancel(Animator arg0) {}
                    });
        }
    }

    /* package */ boolean activitySupportsVrBrowsing(Activity activity) {
        if (activity instanceof ChromeTabbedActivity) return true;
        return false;
    }

    /* package */ boolean relaunchOnMainDisplayIfNecessary(Activity activity, Intent intent) {
        boolean onMainDisplay = DisplayAndroid.getNonMultiDisplay(activity).getDisplayId()
                == Display.DEFAULT_DISPLAY;
        // TODO(mthiesse): There's a known race when switching displays on Android O/P that can
        // lead us to actually be on the main display, but our context still thinks it's on
        // the virtual display. This is intended to be fixed for Android Q+, but we can work
        // around the race by explicitly relaunching ourselves to the main display.
        if (!onMainDisplay) {
            Log.i(TAG, "Relaunching Chrome onto the main display.");
            activity.finish();
            activity.startActivity(intent,
                    ApiCompatibilityUtils.createLaunchDisplayIdActivityOptions(
                            Display.DEFAULT_DISPLAY));
            return true;
        }
        return false;
    }

    private int getIntSystemProperty(String key, int defaultValue) {
        try {
            final Class<?> systemProperties = Class.forName("android.os.SystemProperties");
            final Method getInt = systemProperties.getMethod("getInt", String.class, int.class);
            return (Integer) getInt.invoke(null, key, defaultValue);
        } catch (Exception e) {
            Log.e("Exception while getting system property %s. Using default %s.", key,
                    defaultValue, e);
            return defaultValue;
        }
    }
}
