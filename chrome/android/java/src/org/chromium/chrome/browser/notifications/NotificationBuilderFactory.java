// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import static org.chromium.chrome.browser.ChromeFeatureList.ALLOW_REMOTE_CONTEXT_FOR_NOTIFICATIONS;

import android.content.Context;
import android.content.pm.PackageManager;
import android.support.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.channels.ChannelsInitializer;

/**
 * Factory which supplies the appropriate type of notification builder based on Android version.
 * Should be used for all notifications we create, to ensure a notification channel is set on O.
 */
public class NotificationBuilderFactory {
    /**
     * Creates either a Notification.Builder or NotificationCompat.Builder under the hood, wrapped
     * in our own common interface, and ensures the notification channel has been initialized.
     *
     * @param preferCompat true if a NotificationCompat.Builder is preferred. You should pick true
     *                     unless you know NotificationCompat.Builder doesn't support a feature you
     *                     require.
     * @param channelId The ID of the channel the notification should be posted to. This channel
     *                  will be created if it did not already exist. Must be a known channel within
     *                  {@link ChannelsInitializer#ensureInitialized(String)}.
     */
    public static ChromeNotificationBuilder createChromeNotificationBuilder(
            boolean preferCompat, String channelId) {
        return createChromeNotificationBuilder(preferCompat, channelId, null);
    }

    /**
     * Same as above, with additional parameter:
     * @param remoteAppPackageName if not null, tries to create a Context from the package name
     * and passes it to the builder.
     */
    public static ChromeNotificationBuilder createChromeNotificationBuilder(
            boolean preferCompat, String channelId, @Nullable String remoteAppPackageName) {
        Context context = ContextUtils.getApplicationContext();
        if (remoteAppPackageName != null) {
            assert ChromeFeatureList.isEnabled(ALLOW_REMOTE_CONTEXT_FOR_NOTIFICATIONS);
            try {
                context = context.createPackageContext(remoteAppPackageName, 0);
            } catch (PackageManager.NameNotFoundException e) {
                throw new RuntimeException("Failed to create context for package "
                        + remoteAppPackageName, e);
            }
        }

        NotificationManagerProxyImpl notificationManagerProxy =
                new NotificationManagerProxyImpl(context);

        ChannelsInitializer channelsInitializer =
                new ChannelsInitializer(notificationManagerProxy, context.getResources());

        return preferCompat ? new NotificationCompatBuilder(context, channelId, channelsInitializer)
                            : new NotificationBuilder(context, channelId, channelsInitializer);
    }
}
