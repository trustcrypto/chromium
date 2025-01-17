// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_MEDIA_KEYS_LISTENER_H_
#define UI_BASE_ACCELERATORS_MEDIA_KEYS_LISTENER_H_

#include <memory>

#include "base/callback.h"
#include "ui/base/ui_base_export.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ui {

class Accelerator;

// Create MediaKeyListener to receive accelerators on media keys.
class UI_BASE_EXPORT MediaKeysListener {
 public:
  enum class Scope {
    kGlobal,   // Listener works whenever application in focus or not.
    kFocused,  // Listener only works whan application has focus.
  };

  // Media keys accelerators receiver.
  class UI_BASE_EXPORT Delegate {
   public:
    virtual ~Delegate();

    // Called on media key event.
    virtual void OnMediaKeysAccelerator(const Accelerator& accelerator) = 0;

    // Called after a call to StartWatchingMediaKey(), once the listener is
    // ready to receive key input. This will not be called after a call to
    // StartWatchingMediaKey() if the listener was already listening for any
    // media key. This may be called synchronously or asynchronously depending
    // on the underlying implementation.
    virtual void OnStartedWatchingMediaKeys() {}
  };

  // Can return nullptr if media keys listening is not implemented.
  // Currently implemented only on mac.
  static std::unique_ptr<MediaKeysListener> Create(Delegate* delegate,
                                                   Scope scope);

  static bool IsMediaKeycode(KeyboardCode key_code);

  virtual ~MediaKeysListener();

  // Start listening for a given media key.
  virtual void StartWatchingMediaKey(KeyboardCode key_code) = 0;
  // Stop listening for a given media key.
  virtual void StopWatchingMediaKey(KeyboardCode key_code) = 0;
};

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_MEDIA_KEYS_LISTENER_H_
