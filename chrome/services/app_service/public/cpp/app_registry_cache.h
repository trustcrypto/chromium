// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_REGISTRY_CACHE_H_
#define CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_REGISTRY_CACHE_H_

#include <map>
#include <vector>

#include "base/macros.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "chrome/services/app_service/public/cpp/app_update.h"

namespace apps {

// Caches all of the apps::mojom::AppPtr's seen by an apps::mojom::Subscriber.
// A Subscriber sees a stream of "deltas", or changes in app state. This cache
// also keeps the "sum" of those previous deltas, so that observers of this
// object are presented with AppUpdate's, i.e. "state-and-delta"s.
//
// It can also be queried synchronously, providing answers from its in-memory
// cache, even though the underlying App Registry (and its App Publishers)
// communicate asynchronously, possibly across process boundaries, via Mojo
// IPC. Synchronous APIs can be more suitable for e.g. UI programming that
// should not block an event loop on I/O.
//
// This class is not thread-safe.
//
// See //chrome/services/app_service/README.md for more details.
class AppRegistryCache {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // The apps::AppUpdate argument shouldn't be accessed after OnAppUpdate
    // returns.
    virtual void OnAppUpdate(const AppUpdate& update) = 0;

   protected:
    // Use this constructor when the object is tied to a single
    // AppRegistryCache for its entire lifetime.
    explicit Observer(AppRegistryCache* cache);

    // Use this constructor when the object wants to observe a AppRegistryCache
    // for part of its lifetime. It can then call Observe() to start and stop
    // observing.
    Observer();

    ~Observer() override;

    // Start observing a different AppRegistryCache; used with the default
    // constructor.
    void Observe(AppRegistryCache* cache);

   private:
    AppRegistryCache* cache_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  AppRegistryCache();
  ~AppRegistryCache();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Notifies all observers of state-and-delta AppUpdate's (the state comes
  // from the internal cache, the delta comes from the argument) and then
  // merges the cached states with the deltas.
  //
  // The callee may modify the deltas.
  void OnApps(std::vector<apps::mojom::AppPtr> deltas);

  apps::mojom::AppType GetAppType(const std::string& app_id);

  // Calls f, a void-returning function whose arguments are (const
  // apps::AppUpdate&), on each app in the cache.
  //
  // f's argument is an apps::AppUpdate instead of an apps::mojom::AppPtr so
  // that callers can more easily share code with Observer::OnAppUpdate (which
  // also takes an apps::AppUpdate), and an apps::AppUpdate also has a
  // StateIsNull method.
  //
  // The apps::AppUpdate argument to f shouldn't be accessed after f returns.
  //
  // f must be synchronous, and if it asynchronously calls ForEachApp again,
  // it's not guaranteed to see a consistent state.
  template <typename FunctionType>
  void ForEachApp(FunctionType f) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

    for (const auto& s_iter : states_) {
      const apps::mojom::App* state = s_iter.second.get();

      auto d_iter = deltas_in_progress_.find(s_iter.first);
      const apps::mojom::App* delta =
          (d_iter != deltas_in_progress_.end()) ? d_iter->second : nullptr;

      f(apps::AppUpdate(state, delta));
    }

    for (const auto& d_iter : deltas_in_progress_) {
      const apps::mojom::App* delta = d_iter.second;

      auto s_iter = states_.find(d_iter.first);
      if (s_iter != states_.end()) {
        continue;
      }

      f(apps::AppUpdate(nullptr, delta));
    }
  }

  // Calls f, a void-returning function whose arguments are (const
  // apps::AppUpdate&), on the app in the cache with the given app_id. It will
  // return true (and call f) if there is such an app, otherwise it will return
  // false (and not call f). The AppUpdate argument to f has the same semantics
  // as for ForEachApp, above.
  //
  // f must be synchronous, and if it asynchronously calls ForEachApp again,
  // it's not guaranteed to see a consistent state.
  template <typename FunctionType>
  bool ForOneApp(const std::string& app_id, FunctionType f) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

    auto s_iter = states_.find(app_id);
    const apps::mojom::App* state =
        (s_iter != states_.end()) ? s_iter->second.get() : nullptr;

    auto d_iter = deltas_in_progress_.find(app_id);
    const apps::mojom::App* delta =
        (d_iter != deltas_in_progress_.end()) ? d_iter->second : nullptr;

    if (state || delta) {
      f(apps::AppUpdate(state, delta));
      return true;
    }
    return false;
  }

 private:
  base::ObserverList<Observer> observers_;

  // Maps from app_id to the latest state: the "sum" of all previous deltas.
  std::map<std::string, apps::mojom::AppPtr> states_;

  // Maps from app_id to the deltas being processed by OnApps.
  std::map<std::string, apps::mojom::App*> deltas_in_progress_;

  SEQUENCE_CHECKER(my_sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(AppRegistryCache);
};

}  // namespace apps

#endif  // CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_REGISTRY_CACHE_H_
