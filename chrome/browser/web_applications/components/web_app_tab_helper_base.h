// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_TAB_HELPER_BASE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_TAB_HELPER_BASE_H_

#include "base/macros.h"
#include "base/unguessable_token.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

namespace web_app {

class WebAppAudioFocusIdMap;

// Per-tab web app helper. Allows to associate a tab (web page) with a web app
// (or legacy bookmark app).
class WebAppTabHelperBase
    : public content::WebContentsObserver,
      public content::WebContentsUserData<WebAppTabHelperBase> {
 public:
  ~WebAppTabHelperBase() override;

  // This provides a weak reference to the current audio focus id map instance
  // which is owned by WebAppProvider. This is used to ensure that all web
  // contents associated with a web app shared the same audio focus group id.
  void SetAudioFocusIdMap(WebAppAudioFocusIdMap* audio_focus_id_map);

  const AppId& app_id() const { return app_id_; }

  // Set app_id on web app installation or tab restore.
  void SetAppId(const AppId& app_id);
  // Clear app_id on web app uninstallation.
  void ResetAppId();

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidCloneToNewWebContents(
      content::WebContents* old_web_contents,
      content::WebContents* new_web_contents) override;

 protected:
  // See documentation in WebContentsUserData class comment.
  explicit WebAppTabHelperBase(content::WebContents* web_contents);
  friend class content::WebContentsUserData<WebAppTabHelperBase>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  // Clone |this| tab helper (preserving a derived type).
  virtual WebAppTabHelperBase* CloneForWebContents(
      content::WebContents* web_contents) const = 0;

  // Gets AppId from derived platform-specific TabHelper and updates
  // app_id_ with it.
  virtual AppId GetAppId(const GURL& url) = 0;

  // Returns whether the associated web contents belongs to an app window.
  virtual bool IsInAppWindow() const = 0;

 private:
  friend class WebAppAudioFocusBrowserTest;

  // Runs any logic when the associated app either changes or is removed.
  void OnAssociatedAppChanged();

  // Updates the audio focus group id based on the current web app.
  void UpdateAudioFocusGroupId();

  // WebApp associated with this tab. Empty string if no app associated.
  AppId app_id_;

  // The audio focus group id is used to group media sessions together for apps.
  // We store the applied group id locally on the helper for testing.
  base::UnguessableToken audio_focus_group_id_ = base::UnguessableToken::Null();

  // Weak reference to audio focus group id storage.
  WebAppAudioFocusIdMap* audio_focus_id_map_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WebAppTabHelperBase);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_TAB_HELPER_BASE_H_
