// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/media_session_impl.h"

#include <map>
#include <memory>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/browser/media/session/media_session_player_observer.h"
#include "content/browser/media/session/media_session_service_impl.h"
#include "content/browser/media/session/mock_media_session_observer.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "media/base/media_content_type.h"
#include "services/media_session/public/cpp/media_metadata.h"
#include "services/media_session/public/cpp/test/mock_media_session.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "third_party/blink/public/platform/modules/mediasession/media_session.mojom.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Eq;
using ::testing::InvokeWithoutArgs;
using ::testing::NiceMock;

namespace content {

namespace {

constexpr base::TimeDelta kDefaultSeekTime =
    base::TimeDelta::FromSeconds(media_session::mojom::kDefaultSeekTimeSeconds);

static const int kPlayerId = 0;

class MockMediaSessionServiceImpl : public MediaSessionServiceImpl {
 public:
  explicit MockMediaSessionServiceImpl(RenderFrameHost* rfh)
      : MediaSessionServiceImpl(rfh) {}
  ~MockMediaSessionServiceImpl() override = default;
};

class MockMediaSessionClient : public blink::mojom::MediaSessionClient {
 public:
  MockMediaSessionClient() : binding_(this) {}

  blink::mojom::MediaSessionClientPtr CreateInterfacePtrAndBind() {
    blink::mojom::MediaSessionClientPtr client;
    binding_.Bind(mojo::MakeRequest(&client));
    return client;
  }

  MOCK_METHOD1(DidReceiveAction,
               void(media_session::mojom::MediaSessionAction action));

 private:
  mojo::Binding<blink::mojom::MediaSessionClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(MockMediaSessionClient);
};

class MockMediaSessionPlayerObserver : public MediaSessionPlayerObserver {
 public:
  explicit MockMediaSessionPlayerObserver(RenderFrameHost* rfh)
      : render_frame_host_(rfh) {}

  ~MockMediaSessionPlayerObserver() override = default;

  MOCK_METHOD1(OnSuspend, void(int player_id));
  MOCK_METHOD1(OnResume, void(int player_id));
  MOCK_METHOD2(OnSeekForward, void(int player_id, base::TimeDelta seek_time));
  MOCK_METHOD2(OnSeekBackward, void(int player_id, base::TimeDelta seek_time));
  MOCK_METHOD2(OnSetVolumeMultiplier,
               void(int player_id, double volume_multiplier));

  RenderFrameHost* render_frame_host() const override {
    return render_frame_host_;
  }

 private:
  RenderFrameHost* render_frame_host_;
};

}  // anonymous namespace

class MediaSessionImplServiceRoutingTest
    : public RenderViewHostImplTestHarness {
 public:
  MediaSessionImplServiceRoutingTest() = default;
  ~MediaSessionImplServiceRoutingTest() override = default;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    contents()->GetMainFrame()->InitializeRenderFrameIfNeeded();
    mock_media_session_observer_.reset(new NiceMock<MockMediaSessionObserver>(
        MediaSessionImpl::Get(contents())));
    main_frame_ = contents()->GetMainFrame();
    sub_frame_ = main_frame_->AppendChild("sub_frame");
  }

  void TearDown() override {
    mock_media_session_observer_.reset();
    services_.clear();
    clients_.clear();

    RenderViewHostImplTestHarness::TearDown();
  }

 protected:
  MockMediaSessionObserver* mock_media_session_observer() {
    return mock_media_session_observer_.get();
  }

  void CreateServiceForFrame(TestRenderFrameHost* frame) {
    services_[frame] =
        std::make_unique<NiceMock<MockMediaSessionServiceImpl>>(frame);
    clients_[frame] = std::make_unique<NiceMock<MockMediaSessionClient>>();
    services_[frame]->SetClient(clients_[frame]->CreateInterfacePtrAndBind());
  }

  void DestroyServiceForFrame(TestRenderFrameHost* frame) {
    services_.erase(frame);
    clients_.erase(frame);
  }

  MockMediaSessionClient* GetClientForFrame(TestRenderFrameHost* frame) {
    auto iter = clients_.find(frame);
    return (iter != clients_.end()) ? iter->second.get() : nullptr;
  }

  void StartPlayerForFrame(TestRenderFrameHost* frame) {
    players_[frame] =
        std::make_unique<NiceMock<MockMediaSessionPlayerObserver>>(frame);
    MediaSessionImpl::Get(contents())
        ->AddPlayer(players_[frame].get(), kPlayerId,
                    media::MediaContentType::Persistent);
  }

  void ClearPlayersForFrame(TestRenderFrameHost* frame) {
    if (!players_.count(frame))
      return;

    MediaSessionImpl::Get(contents())
        ->RemovePlayer(players_[frame].get(), kPlayerId);
  }

  MockMediaSessionPlayerObserver* GetPlayerForFrame(
      TestRenderFrameHost* frame) {
    auto iter = players_.find(frame);
    return (iter != players_.end()) ? iter->second.get() : nullptr;
  }

  MediaSessionServiceImpl* ComputeServiceForRouting() {
    return MediaSessionImpl::Get(contents())->ComputeServiceForRouting();
  }

  MediaSessionImpl* GetMediaSession() {
    return MediaSessionImpl::Get(contents());
  }

  TestRenderFrameHost* main_frame_;
  TestRenderFrameHost* sub_frame_;

  std::unique_ptr<MockMediaSessionObserver> mock_media_session_observer_;

  using ServiceMap = std::map<TestRenderFrameHost*,
                              std::unique_ptr<MockMediaSessionServiceImpl>>;
  ServiceMap services_;

  using ClientMap =
      std::map<TestRenderFrameHost*, std::unique_ptr<MockMediaSessionClient>>;
  ClientMap clients_;

  using PlayerMap = std::map<TestRenderFrameHost*,
                             std::unique_ptr<MockMediaSessionPlayerObserver>>;
  PlayerMap players_;
};

TEST_F(MediaSessionImplServiceRoutingTest, NoFrameProducesAudio) {
  CreateServiceForFrame(main_frame_);
  CreateServiceForFrame(sub_frame_);

  ASSERT_EQ(nullptr, ComputeServiceForRouting());
}

TEST_F(MediaSessionImplServiceRoutingTest,
       OnlyMainFrameProducesAudioButHasNoService) {
  StartPlayerForFrame(main_frame_);

  ASSERT_EQ(nullptr, ComputeServiceForRouting());
}

TEST_F(MediaSessionImplServiceRoutingTest,
       OnlySubFrameProducesAudioButHasNoService) {
  StartPlayerForFrame(sub_frame_);

  ASSERT_EQ(nullptr, ComputeServiceForRouting());
}

TEST_F(MediaSessionImplServiceRoutingTest,
       OnlyMainFrameProducesAudioButHasDestroyedService) {
  CreateServiceForFrame(main_frame_);
  StartPlayerForFrame(main_frame_);
  DestroyServiceForFrame(main_frame_);

  ASSERT_EQ(nullptr, ComputeServiceForRouting());
}

TEST_F(MediaSessionImplServiceRoutingTest,
       OnlySubFrameProducesAudioButHasDestroyedService) {
  CreateServiceForFrame(sub_frame_);
  StartPlayerForFrame(sub_frame_);
  DestroyServiceForFrame(sub_frame_);

  ASSERT_EQ(nullptr, ComputeServiceForRouting());
}

TEST_F(MediaSessionImplServiceRoutingTest,
       OnlyMainFrameProducesAudioAndServiceIsCreatedAfterwards) {
  StartPlayerForFrame(main_frame_);
  CreateServiceForFrame(main_frame_);

  ASSERT_EQ(services_[main_frame_].get(), ComputeServiceForRouting());
}

TEST_F(MediaSessionImplServiceRoutingTest,
       OnlySubFrameProducesAudioAndServiceIsCreatedAfterwards) {
  StartPlayerForFrame(sub_frame_);
  CreateServiceForFrame(sub_frame_);

  ASSERT_EQ(services_[sub_frame_].get(), ComputeServiceForRouting());
}

TEST_F(MediaSessionImplServiceRoutingTest,
       BothFrameProducesAudioButOnlySubFrameHasService) {
  StartPlayerForFrame(main_frame_);
  StartPlayerForFrame(sub_frame_);

  CreateServiceForFrame(sub_frame_);

  ASSERT_EQ(services_[sub_frame_].get(), ComputeServiceForRouting());
}

TEST_F(MediaSessionImplServiceRoutingTest, PreferTopMostFrame) {
  StartPlayerForFrame(main_frame_);
  StartPlayerForFrame(sub_frame_);

  CreateServiceForFrame(main_frame_);
  CreateServiceForFrame(sub_frame_);

  ASSERT_EQ(services_[main_frame_].get(), ComputeServiceForRouting());
}

TEST_F(MediaSessionImplServiceRoutingTest,
       RoutedServiceUpdatedAfterRemovingPlayer) {
  StartPlayerForFrame(main_frame_);
  StartPlayerForFrame(sub_frame_);

  CreateServiceForFrame(main_frame_);
  CreateServiceForFrame(sub_frame_);

  ClearPlayersForFrame(main_frame_);

  ASSERT_EQ(services_[sub_frame_].get(), ComputeServiceForRouting());
}

TEST_F(MediaSessionImplServiceRoutingTest,
       DontNotifyMetadataAndActionsChangeWhenUncontrollable) {
  EXPECT_CALL(*mock_media_session_observer(), MediaSessionMetadataChanged(_))
      .Times(0);
  EXPECT_CALL(*mock_media_session_observer(), MediaSessionActionsChanged(_))
      .Times(0);

  CreateServiceForFrame(main_frame_);

  services_[main_frame_]->SetMetadata(media_session::MediaMetadata());
  services_[main_frame_]->EnableAction(
      media_session::mojom::MediaSessionAction::kPlay);
}

TEST_F(MediaSessionImplServiceRoutingTest,
       NotifyMetadataAndActionsChangeWhenControllable) {
  media_session::MediaMetadata expected_metadata;
  expected_metadata.title = base::ASCIIToUTF16("title");
  expected_metadata.artist = base::ASCIIToUTF16("artist");
  expected_metadata.album = base::ASCIIToUTF16("album");

  std::set<media_session::mojom::MediaSessionAction> empty_actions;
  std::set<media_session::mojom::MediaSessionAction> expected_actions;
  expected_actions.insert(media_session::mojom::MediaSessionAction::kPlay);

  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionMetadataChanged(Eq(base::nullopt)))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionActionsChanged(Eq(empty_actions)))
      .Times(AnyNumber());

  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionMetadataChanged(Eq(expected_metadata)))
      .Times(1);
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionActionsChanged(Eq(expected_actions)))
      .Times(1);

  CreateServiceForFrame(main_frame_);
  StartPlayerForFrame(main_frame_);

  services_[main_frame_]->SetMetadata(expected_metadata);
  services_[main_frame_]->EnableAction(
      media_session::mojom::MediaSessionAction::kPlay);
}

TEST_F(MediaSessionImplServiceRoutingTest,
       NotifyMetadataAndActionsChangeWhenTurningControllable) {
  media_session::MediaMetadata expected_metadata;
  expected_metadata.title = base::ASCIIToUTF16("title");
  expected_metadata.artist = base::ASCIIToUTF16("artist");
  expected_metadata.album = base::ASCIIToUTF16("album");

  std::set<media_session::mojom::MediaSessionAction> expected_actions;
  expected_actions.insert(media_session::mojom::MediaSessionAction::kPlay);

  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionMetadataChanged(Eq(expected_metadata)))
      .Times(1);
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionActionsChanged(Eq(expected_actions)))
      .Times(1);

  CreateServiceForFrame(main_frame_);

  services_[main_frame_]->SetMetadata(expected_metadata);
  services_[main_frame_]->EnableAction(
      media_session::mojom::MediaSessionAction::kPlay);

  StartPlayerForFrame(main_frame_);
}

TEST_F(MediaSessionImplServiceRoutingTest,
       DontNotifyMetadataAndActionsChangeWhenTurningUncontrollable) {
  media_session::MediaMetadata expected_metadata;
  expected_metadata.title = base::ASCIIToUTF16("title");
  expected_metadata.artist = base::ASCIIToUTF16("artist");
  expected_metadata.album = base::ASCIIToUTF16("album");

  std::set<media_session::mojom::MediaSessionAction> empty_actions;
  std::set<media_session::mojom::MediaSessionAction> expected_actions;
  expected_actions.insert(media_session::mojom::MediaSessionAction::kPlay);

  EXPECT_CALL(*mock_media_session_observer(), MediaSessionMetadataChanged(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_media_session_observer(), MediaSessionActionsChanged(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionMetadataChanged(Eq(base::nullopt)))
      .Times(0);
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionActionsChanged(Eq(empty_actions)))
      .Times(0);

  CreateServiceForFrame(main_frame_);

  services_[main_frame_]->SetMetadata(expected_metadata);
  services_[main_frame_]->EnableAction(
      media_session::mojom::MediaSessionAction::kPlay);

  StartPlayerForFrame(main_frame_);
  ClearPlayersForFrame(main_frame_);
}

TEST_F(MediaSessionImplServiceRoutingTest,
       TestPauseBehaviorWhenMainFrameIsRouted) {
  base::RunLoop run_loop;

  StartPlayerForFrame(main_frame_);
  StartPlayerForFrame(sub_frame_);

  CreateServiceForFrame(main_frame_);

  EXPECT_CALL(*GetPlayerForFrame(sub_frame_), OnSuspend(_));
  EXPECT_CALL(
      *GetClientForFrame(main_frame_),
      DidReceiveAction(media_session::mojom::MediaSessionAction::kPause))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  services_[main_frame_]->EnableAction(
      media_session::mojom::MediaSessionAction::kPause);

  MediaSessionImpl::Get(contents())
      ->DidReceiveAction(media_session::mojom::MediaSessionAction::kPause);

  run_loop.Run();
}

TEST_F(MediaSessionImplServiceRoutingTest,
       TestPauseBehaviorWhenSubFrameIsRouted) {
  base::RunLoop run_loop;

  StartPlayerForFrame(main_frame_);
  StartPlayerForFrame(sub_frame_);

  CreateServiceForFrame(sub_frame_);

  EXPECT_CALL(*GetPlayerForFrame(main_frame_), OnSuspend(_));
  EXPECT_CALL(
      *GetClientForFrame(sub_frame_),
      DidReceiveAction(media_session::mojom::MediaSessionAction::kPause))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  services_[sub_frame_]->EnableAction(
      media_session::mojom::MediaSessionAction::kPause);

  MediaSessionImpl::Get(contents())
      ->DidReceiveAction(media_session::mojom::MediaSessionAction::kPause);

  run_loop.Run();
}

TEST_F(MediaSessionImplServiceRoutingTest,
       TestReceivingPauseActionWhenNoServiceRouted) {
  CreateServiceForFrame(main_frame_);
  CreateServiceForFrame(sub_frame_);

  EXPECT_EQ(nullptr, ComputeServiceForRouting());

  // This should not crash.
  MediaSessionImpl::Get(contents())
      ->DidReceiveAction(media_session::mojom::MediaSessionAction::kPause);
}

TEST_F(MediaSessionImplServiceRoutingTest,
       TestPreviousTrackBehaviorWhenMainFrameIsRouted) {
  base::RunLoop run_loop;

  StartPlayerForFrame(main_frame_);
  StartPlayerForFrame(sub_frame_);

  CreateServiceForFrame(main_frame_);

  EXPECT_CALL(*GetClientForFrame(main_frame_),
              DidReceiveAction(
                  media_session::mojom::MediaSessionAction::kPreviousTrack))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  services_[main_frame_]->EnableAction(
      media_session::mojom::MediaSessionAction::kPreviousTrack);

  MediaSessionImpl::Get(contents())->PreviousTrack();
  run_loop.Run();
}

TEST_F(MediaSessionImplServiceRoutingTest,
       TestNextTrackBehaviorWhenMainFrameIsRouted) {
  base::RunLoop run_loop;

  StartPlayerForFrame(main_frame_);
  StartPlayerForFrame(sub_frame_);

  CreateServiceForFrame(main_frame_);

  EXPECT_CALL(
      *GetClientForFrame(main_frame_),
      DidReceiveAction(media_session::mojom::MediaSessionAction::kNextTrack))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  services_[main_frame_]->EnableAction(
      media_session::mojom::MediaSessionAction::kNextTrack);

  MediaSessionImpl::Get(contents())->NextTrack();
  run_loop.Run();
}

TEST_F(MediaSessionImplServiceRoutingTest, TestSeekBackwardBehaviourDefault) {
  base::RunLoop run_loop;

  StartPlayerForFrame(main_frame_);
  CreateServiceForFrame(main_frame_);

  EXPECT_CALL(*GetPlayerForFrame(main_frame_),
              OnSeekBackward(_, kDefaultSeekTime))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_CALL(
      *GetClientForFrame(main_frame_),
      DidReceiveAction(media_session::mojom::MediaSessionAction::kSeekBackward))
      .Times(0);

  MediaSessionImpl::Get(contents())->Seek(kDefaultSeekTime * -1);
  run_loop.Run();
}

TEST_F(MediaSessionImplServiceRoutingTest,
       TestSeekBackwardBehaviourWhenActionEnabled) {
  base::RunLoop run_loop;

  StartPlayerForFrame(main_frame_);
  CreateServiceForFrame(main_frame_);

  EXPECT_CALL(*GetPlayerForFrame(main_frame_), OnSeekBackward(_, _)).Times(0);
  EXPECT_CALL(
      *GetClientForFrame(main_frame_),
      DidReceiveAction(media_session::mojom::MediaSessionAction::kSeekBackward))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  services_[main_frame_]->EnableAction(
      media_session::mojom::MediaSessionAction::kSeekBackward);

  MediaSessionImpl::Get(contents())->Seek(kDefaultSeekTime * -1);
  run_loop.Run();
}

TEST_F(MediaSessionImplServiceRoutingTest, TestSeekForwardBehaviourDefault) {
  base::RunLoop run_loop;

  StartPlayerForFrame(main_frame_);
  CreateServiceForFrame(main_frame_);

  EXPECT_CALL(*GetPlayerForFrame(main_frame_),
              OnSeekForward(_, kDefaultSeekTime))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  EXPECT_CALL(
      *GetClientForFrame(main_frame_),
      DidReceiveAction(media_session::mojom::MediaSessionAction::kSeekForward))
      .Times(0);

  MediaSessionImpl::Get(contents())->Seek(kDefaultSeekTime);
  run_loop.Run();
}

TEST_F(MediaSessionImplServiceRoutingTest,
       TestSeekForwardBehaviourWhenActionEnabled) {
  base::RunLoop run_loop;

  StartPlayerForFrame(main_frame_);
  CreateServiceForFrame(main_frame_);

  EXPECT_CALL(*GetPlayerForFrame(main_frame_), OnSeekForward(_, _)).Times(0);
  EXPECT_CALL(
      *GetClientForFrame(main_frame_),
      DidReceiveAction(media_session::mojom::MediaSessionAction::kSeekForward))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  services_[main_frame_]->EnableAction(
      media_session::mojom::MediaSessionAction::kSeekForward);

  MediaSessionImpl::Get(contents())->Seek(kDefaultSeekTime);
  run_loop.Run();
}

TEST_F(MediaSessionImplServiceRoutingTest,
       NotifyMojoObserverMetadataWhenControllable) {
  media_session::MediaMetadata expected_metadata;
  expected_metadata.title = base::ASCIIToUTF16("title");
  expected_metadata.artist = base::ASCIIToUTF16("artist");
  expected_metadata.album = base::ASCIIToUTF16("album");

  CreateServiceForFrame(main_frame_);
  StartPlayerForFrame(main_frame_);

  {
    media_session::test::MockMediaSessionMojoObserver observer(
        *GetMediaSession());
    services_[main_frame_]->SetMetadata(expected_metadata);
    EXPECT_EQ(expected_metadata, observer.WaitForNonEmptyMetadata());
  }
}

TEST_F(MediaSessionImplServiceRoutingTest,
       NotifyMojoObserverMetadataEmptyWhenControllable) {
  CreateServiceForFrame(main_frame_);
  StartPlayerForFrame(main_frame_);

  {
    media_session::test::MockMediaSessionMojoObserver observer(
        *GetMediaSession());
    services_[main_frame_]->SetMetadata(base::nullopt);

    // When the session becomes controllable we should receive empty metadata
    // because we have not set any. The |is_controllable| boolean will also
    // become true.
    EXPECT_FALSE(observer.WaitForMetadata());
    EXPECT_TRUE(observer.session_info()->is_controllable);
  }
}

TEST_F(MediaSessionImplServiceRoutingTest,
       NotifyMojoObserverWhenTurningUncontrollable) {
  CreateServiceForFrame(main_frame_);
  StartPlayerForFrame(main_frame_);

  {
    media_session::test::MockMediaSessionMojoObserver observer(
        *GetMediaSession());
    ClearPlayersForFrame(main_frame_);

    // When the session becomes inactive it will also become uncontrollable so
    // we should check the |is_controllable| boolean.
    observer.WaitForState(
        media_session::mojom::MediaSessionInfo::SessionState::kInactive);
    EXPECT_FALSE(observer.session_info()->is_controllable);
  }
}

}  // namespace content
