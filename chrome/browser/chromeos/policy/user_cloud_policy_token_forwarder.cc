// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/user_cloud_policy_token_forwarder.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/timer/timer.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "content/public/browser/notification_source.h"
#include "google_apis/gaia/gaia_constants.h"
#include "services/identity/public/cpp/primary_account_access_token_fetcher.h"

namespace policy {

// static
constexpr base::TimeDelta UserCloudPolicyTokenForwarder::kFetchTokenRetryDelay;

UserCloudPolicyTokenForwarder::UserCloudPolicyTokenForwarder(
    UserCloudPolicyManagerChromeOS* manager,
    identity::IdentityManager* identity_manager)
    : manager_(manager),
      identity_manager_(identity_manager),
      refresh_oauth_token_timer_(std::make_unique<base::RepeatingTimer>()),
      clock_(base::DefaultClock::GetInstance()),
      weak_ptr_factory_(this) {
  // Start by waiting for the CloudPolicyService to be initialized, so that
  // we can check if it already has a DMToken or not.
  if (manager_->core()->service()->IsInitializationComplete()) {
    StartRequest();
  } else {
    manager_->core()->service()->AddObserver(this);
  }
}

UserCloudPolicyTokenForwarder::~UserCloudPolicyTokenForwarder() {}

void UserCloudPolicyTokenForwarder::Shutdown() {
  access_token_fetcher_.reset();
  refresh_oauth_token_timer_.reset();
  manager_->core()->service()->RemoveObserver(this);
}

void UserCloudPolicyTokenForwarder::
    OnCloudPolicyServiceInitializationCompleted() {
  StartRequest();
}

bool UserCloudPolicyTokenForwarder::IsTokenFetchInProgressForTesting() const {
  return access_token_fetcher_.get() != nullptr;
}

bool UserCloudPolicyTokenForwarder::IsTokenRefreshScheduledForTesting() const {
  return refresh_oauth_token_timer_ && refresh_oauth_token_timer_->IsRunning();
}

base::Optional<base::TimeDelta>
UserCloudPolicyTokenForwarder::GetTokenRefreshDelayForTesting() const {
  return IsTokenRefreshScheduledForTesting()
             ? refresh_oauth_token_timer_->GetCurrentDelay()
             : base::Optional<base::TimeDelta>();
}

void UserCloudPolicyTokenForwarder::OverrideTimeForTesting(
    const base::Clock* clock,
    const base::TickClock* tick_clock,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  clock_ = clock;
  refresh_oauth_token_timer_ =
      std::make_unique<base::RepeatingTimer>(tick_clock);
  refresh_oauth_token_timer_->SetTaskRunner(task_runner);
}

void UserCloudPolicyTokenForwarder::StartRequest() {
  refresh_oauth_token_timer_->Stop();
  // TODO(mnissler): Once a better way to reconfirm whether a user is on the
  // login whitelist is available there is no reason to fetch the OAuth2 token
  // for regular user here if the client is already registered. If it is not
  // recurring token fetch for child user check and bail out here.
  identity::ScopeSet scopes;
  scopes.insert(GaiaConstants::kDeviceManagementServiceOAuth);
  scopes.insert(GaiaConstants::kOAuthWrapBridgeUserInfoScope);
  access_token_fetcher_ = std::make_unique<
      identity::PrimaryAccountAccessTokenFetcher>(
      "policy_token_forwarder", identity_manager_, scopes,
      base::BindOnce(
          &UserCloudPolicyTokenForwarder::OnAccessTokenFetchCompleted,
          base::Unretained(this)),
      identity::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);
}

void UserCloudPolicyTokenForwarder::OnAccessTokenFetchCompleted(
    GoogleServiceAuthError error,
    identity::AccessTokenInfo token_info) {
  DCHECK(access_token_fetcher_);

  if (error.state() == GoogleServiceAuthError::NONE) {
    oauth_token_ = std::move(token_info);
    manager_->OnAccessTokenAvailable(oauth_token_->token);
  } else {
    // This should seldom happen for initial policy fetch for regular user: if
    // the user is signing in for the first time then this was an online signin
    // and network errors are unlikely; if the user had already signed in before
    // then they should have policy cached, and StartRequest() wouldn't have
    // been invoked. Still, something just went wrong (server 500, or
    // something). Currently we don't recover in this case, and we'll just try
    // to register for policy again on the next signin.
    // TODO(joaodasilva, atwilson): consider blocking signin when this happens,
    // so that the user has to try again before getting into the session. That
    // would guarantee that a session always has fresh policy, or at least
    // enforces a cached policy.
    // In case of child user we keep refreshing the token and failed fetch will
    // be retried after some delay.
  }

  if (!manager_->RequiresOAuthTokenForChildUser()) {
    Shutdown();
    return;
  }

  // Schedule fetching fresh OAuth token after current token expiration, if
  // UserCloudPolicyManagerChromeOS needs valid OAuth token all the time.
  access_token_fetcher_.reset();
  // Retry after delay, if token request fails or the new token is expired.
  const base::Time now = clock_->Now();
  base::TimeDelta time_to_next_refresh =
      (oauth_token_ && oauth_token_->expiration_time >= now)
          ? oauth_token_->expiration_time - now
          : UserCloudPolicyTokenForwarder::kFetchTokenRetryDelay;
  VLOG(1) << "Next OAuth token refresh for DMServer auth in: "
          << time_to_next_refresh;
  refresh_oauth_token_timer_->Start(
      FROM_HERE, time_to_next_refresh,
      base::BindRepeating(&UserCloudPolicyTokenForwarder::StartRequest,
                          weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace policy
