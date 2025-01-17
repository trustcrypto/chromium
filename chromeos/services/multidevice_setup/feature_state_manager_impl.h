// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FEATURE_STATE_MANAGER_IMPL_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FEATURE_STATE_MANAGER_IMPL_H_

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/services/multidevice_setup/feature_state_manager.h"
#include "chromeos/services/multidevice_setup/host_status_provider.h"
#include "chromeos/services/multidevice_setup/public/cpp/android_sms_pairing_state_tracker.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace chromeos {

namespace multidevice_setup {

// Concrete FeatureStateManager implementation. This class relies on
// HostStatusProvider and DeviceSyncClient to determine if features are
// available at all (features are not available unless a verified host is set
// which has enabled the features). To track enabled/disabled/policy state, this
// class utilizes per-user preferences.
class FeatureStateManagerImpl : public FeatureStateManager,
                                public HostStatusProvider::Observer,
                                public device_sync::DeviceSyncClient::Observer,
                                public AndroidSmsPairingStateTracker::Observer {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<FeatureStateManager> BuildInstance(
        PrefService* pref_service,
        HostStatusProvider* host_status_provider,
        device_sync::DeviceSyncClient* device_sync_client,
        std::unique_ptr<AndroidSmsPairingStateTracker>
            android_sms_pairing_state_tracker);

   private:
    static Factory* test_factory_;
  };

  ~FeatureStateManagerImpl() override;

 private:
  FeatureStateManagerImpl(PrefService* pref_service,
                          HostStatusProvider* host_status_provider,
                          device_sync::DeviceSyncClient* device_sync_client,
                          std::unique_ptr<AndroidSmsPairingStateTracker>
                              android_sms_pairing_state_tracker);

  // FeatureStateManager:
  FeatureStatesMap GetFeatureStates() override;
  void PerformSetFeatureEnabledState(mojom::Feature feature,
                                     bool enabled) override;

  // HostStatusProvider::Observer,
  void OnHostStatusChange(const HostStatusProvider::HostStatusWithDevice&
                              host_status_with_device) override;

  // DeviceSyncClient::Observer:
  void OnNewDevicesSynced() override;

  // AndroidSmsPairingStateTracker::Observer:
  void OnPairingStateChanged() override;

  void OnPrefValueChanged();
  void UpdateFeatureStateCache(bool notify_observers_of_changes);
  mojom::FeatureState ComputeFeatureState(mojom::Feature feature);
  bool IsAllowedByPolicy(mojom::Feature feature);
  bool IsSupportedByChromebook(mojom::Feature feature);
  bool HasSufficientSecurity(mojom::Feature feature,
                             const multidevice::RemoteDeviceRef& host_device);
  bool HasBeenActivatedByPhone(mojom::Feature feature,
                               const multidevice::RemoteDeviceRef& host_device);
  bool RequiresFurtherSetup(mojom::Feature feature);
  mojom::FeatureState GetEnabledOrDisabledState(mojom::Feature feature);

  PrefService* pref_service_;
  HostStatusProvider* host_status_provider_;
  device_sync::DeviceSyncClient* device_sync_client_;
  std::unique_ptr<AndroidSmsPairingStateTracker>
      android_sms_pairing_state_tracker_;

  // Map from feature to the pref name which indicates the enabled/disabled
  // boolean state for the feature.
  base::flat_map<mojom::Feature, std::string> feature_to_enabled_pref_name_map_;

  // Same as above, except that the pref names represent whether the feature is
  // allowed by policy or not.
  base::flat_map<mojom::Feature, std::string> feature_to_allowed_pref_name_map_;

  // Map from feature to state, which is updated each time a feature's state
  // changes. This cache is used to determine when a feature's state has changed
  // so that observers can be notified.
  FeatureStatesMap cached_feature_state_map_;

  PrefChangeRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(FeatureStateManagerImpl);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FEATURE_STATE_MANAGER_IMPL_H_
