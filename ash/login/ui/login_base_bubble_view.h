// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_BASE_BUBBLE_VIEW_H_
#define ASH_LOGIN_UI_LOGIN_BASE_BUBBLE_VIEW_H_

#include "ash/ash_export.h"
#include "ash/login/ui/login_button.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {

class LoginBubbleHandler;

// Base bubble view for login screen bubbles.
class ASH_EXPORT LoginBaseBubbleView : public views::BubbleDialogDelegateView,
                                       public ui::LayerAnimationObserver {
 public:
  // Without specifying a parent_window, the bubble will default to being in the
  // same container as anchor_view.
  explicit LoginBaseBubbleView(views::View* anchor_view);
  explicit LoginBaseBubbleView(views::View* anchor_view,
                               gfx::NativeView parent_window);
  ~LoginBaseBubbleView() override;

  void Show();
  void Hide();

  bool IsVisible();

  // Returns the button responsible for opening this bubble.
  virtual LoginButton* GetBubbleOpener() const;

  // Returns whether or not this bubble should show persistently.
  virtual bool IsPersistent() const;
  // Change the persistence of the bubble.
  virtual void SetPersistent(bool persistent);

  // views::BubbleDialogDelegateView:
  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override;
  int GetDialogButtons() const override;
  void SetAnchorView(views::View* anchor_view);

  // ui::LayerAnimationObserver:
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override{};
  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override{};

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

  // views::WidgetObserver:
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

 private:
  void ScheduleAnimation(bool visible);
  void EnsureInScreen();

  std::unique_ptr<LoginBubbleHandler> bubble_handler_;
  DISALLOW_COPY_AND_ASSIGN(LoginBaseBubbleView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_BASE_BUBBLE_VIEW_H_
