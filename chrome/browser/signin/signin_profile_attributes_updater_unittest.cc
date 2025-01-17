// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_profile_attributes_updater.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/test_signin_client_builder.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

#if defined(OS_CHROMEOS)
// Returns a SigninManager that authenticated at creation.
std::unique_ptr<KeyedService> BuildAuthenticatedSigninManager(
    content::BrowserContext* context) {
  std::unique_ptr<KeyedService> signin_manager =
      BuildFakeSigninManagerForTesting(context);
  static_cast<SigninManagerBase*>(signin_manager.get())
      ->SetAuthenticatedAccountInfo("gaia", "example@email.com");
  return signin_manager;
}
#endif

}  // namespace

class SigninProfileAttributesUpdaterTest : public testing::Test {
 public:
  SigninProfileAttributesUpdaterTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    testing::Test::SetUp();

    ASSERT_TRUE(profile_manager_.SetUp());
    TestingProfile::TestingFactories testing_factories;
    testing_factories.emplace_back(
        ChromeSigninClientFactory::GetInstance(),
        base::BindRepeating(&signin::BuildTestSigninClient));
    testing_factories.emplace_back(
        ProfileOAuth2TokenServiceFactory::GetInstance(),
        base::BindRepeating(&BuildFakeProfileOAuth2TokenService));
#if defined(OS_CHROMEOS)
    testing_factories.emplace_back(
        SigninManagerFactory::GetInstance(),
        base::BindRepeating(BuildAuthenticatedSigninManager));
#endif
    std::string name = "profile_name";
    profile_ = profile_manager_.CreateTestingProfile(
        name, /*prefs=*/nullptr, base::UTF8ToUTF16(name), 0, std::string(),
        std::move(testing_factories));
    ASSERT_TRUE(profile_);
  }

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfileManager profile_manager_;
  TestingProfile* profile_ = nullptr;  // Owned by the profile manager.
};

#if !defined(OS_CHROMEOS)
// Tests that the browser state info is updated on signin and signout.
// ChromeOS does not support signout.
TEST_F(SigninProfileAttributesUpdaterTest, SigninSignout) {
  ProfileAttributesEntry* entry;
  ASSERT_TRUE(profile_manager_.profile_attributes_storage()
                  ->GetProfileAttributesWithPath(profile_->GetPath(), &entry));
  ASSERT_FALSE(entry->IsAuthenticated());

  // Signin.
  AccountTrackerService* account_tracker =
      AccountTrackerServiceFactory::GetForProfile(profile_);
  SigninManager* signin_manager = SigninManagerFactory::GetForProfile(profile_);
  std::string account_id =
      account_tracker->SeedAccountInfo("gaia", "example@email.com");
  signin_manager->OnExternalSigninCompleted("example@email.com");
  EXPECT_TRUE(entry->IsAuthenticated());
  EXPECT_EQ("gaia", entry->GetGAIAId());
  EXPECT_EQ("example@email.com", base::UTF16ToUTF8(entry->GetUserName()));

  // Signout.
  signin_manager->SignOut(signin_metrics::SIGNOUT_TEST,
                          signin_metrics::SignoutDelete::IGNORE_METRIC);
  EXPECT_FALSE(entry->IsAuthenticated());
}
#endif  // !defined(OS_CHROMEOS)

// Tests that the browser state info is updated on auth error change.
TEST_F(SigninProfileAttributesUpdaterTest, AuthError) {
  ProfileAttributesEntry* entry;
  ASSERT_TRUE(profile_manager_.profile_attributes_storage()
                  ->GetProfileAttributesWithPath(profile_->GetPath(), &entry));

  AccountTrackerService* account_tracker =
      AccountTrackerServiceFactory::GetForProfile(profile_);
  FakeProfileOAuth2TokenService* token_service =
      static_cast<FakeProfileOAuth2TokenService*>(
          ProfileOAuth2TokenServiceFactory::GetForProfile(profile_));
  std::string account_id =
      account_tracker->SeedAccountInfo("gaia", "example@email.com");
  token_service->UpdateCredentials(account_id, "token");
#if !defined(OS_CHROMEOS)
  // ChromeOS is signed in at creation.
  SigninManagerFactory::GetForProfile(profile_)->OnExternalSigninCompleted(
      "example@email.com");
#endif
  EXPECT_TRUE(entry->IsAuthenticated());
  EXPECT_FALSE(entry->IsAuthError());

  // Set auth error.
  token_service->UpdateAuthErrorForTesting(
      account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  EXPECT_TRUE(entry->IsAuthError());

  // Remove auth error.
  token_service->UpdateAuthErrorForTesting(
      account_id, GoogleServiceAuthError::AuthErrorNone());
  EXPECT_FALSE(entry->IsAuthError());
}
