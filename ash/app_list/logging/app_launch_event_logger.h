// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_LOGGING_APP_LAUNCH_EVENT_LOGGER_H_
#define ASH_APP_LIST_LOGGING_APP_LAUNCH_EVENT_LOGGER_H_

#include <vector>

#include "ash/app_list/logging/app_launch_event_logger.pb.h"
#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"

namespace app_list {

class AppListItem;
class SearchResult;

// This class uses UKM to log metrics for clicks that launch apps in ChromeOS.
// The UKM metrics are not keyed by navigational urls. Instead, for Chrome apps
// the keys are based upon the app id, for Play apps the keys are based upon a
// hash of the package name and for PWAs the keys are the urls associated with
// the PWA.
class AppLaunchEventLogger {
 public:
  AppLaunchEventLogger();
  ~AppLaunchEventLogger();

  // Create the task runner.
  void CreateTaskRunner();
  // Process a click on an app in the suggestion chip.
  void OnSuggestionChipClicked(const SearchResult& item,
                               int index_in_suggestion_chip_container);
  // Process a click on an app located in the grid of apps in the launcher.
  void OnGridClicked(const AppListItem& item);

  // Returns the single instance, creating it if necessary.
  static AppLaunchEventLogger& GetInstance();

 private:
  void OnAppLaunched(AppLaunchEvent app_launch_event);

  void PopulateIdAppTypeMap();
  void PopulatePwaIdUrlMap();

  AppLaunchEvent_AppType GetAppType(const std::string& id);
  const std::string& GetPwaUrl(const std::string& id);

  base::flat_map<std::string, AppLaunchEvent_AppType> id_app_type_map_;
  base::flat_map<std::string, std::string> pwa_id_url_map_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<AppLaunchEventLogger> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppLaunchEventLogger);
};

}  // namespace app_list

#endif  // ASH_APP_LIST_LOGGING_APP_LAUNCH_EVENT_LOGGER_H_
