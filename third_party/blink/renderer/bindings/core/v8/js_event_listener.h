// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_JS_EVENT_LISTENER_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_JS_EVENT_LISTENER_H_

#include "third_party/blink/renderer/bindings/core/v8/js_based_event_listener.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_event_listener.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_member.h"

namespace blink {

// |JSEventListener| implements EventListener in the DOM standard.
// https://dom.spec.whatwg.org/#callbackdef-eventlistener
class CORE_EXPORT JSEventListener final : public JSBasedEventListener {
 public:
  static JSEventListener* Create(ScriptState* script_state,
                                 v8::Local<v8::Object> listener,
                                 const V8PrivateProperty::Symbol& property) {
    return MakeGarbageCollected<JSEventListener>(script_state, listener,
                                                 property);
  }

  JSEventListener(ScriptState* script_state,
                  v8::Local<v8::Object> listener,
                  const V8PrivateProperty::Symbol& property)
      : event_listener_(V8EventListener::Create(listener)) {
    Attach(script_state, listener, property, this);
  }

  // blink::CustomWrappable overrides:
  void Trace(blink::Visitor*) override;

  // blink::EventListener overrides:
  //
  // Check the identity of |V8EventListener::callback_object_|. There can be
  // multiple CallbackInterfaceBase objects that have the same
  // |CallbackInterfaceBase::callback_object_| but have different
  // |CallbackInterfaceBase::incumbent_script_state_|s.
  bool operator==(const EventListener& other) const override {
    auto* other_listener = DynamicTo<JSEventListener>(other);
    return other_listener && event_listener_->HasTheSameCallbackObject(
                                 *other_listener->event_listener_);
  }

  // blink::JSBasedEventListener overrides:
  // TODO(crbug.com/881688): remove empty check for this method. This method
  // should return v8::Object or v8::Null.
  v8::Local<v8::Value> GetListenerObject(EventTarget&) override {
    return event_listener_->CallbackObject();
  }
  v8::Local<v8::Value> GetEffectiveFunction(EventTarget&) override;

  // Helper functions for DowncastTraits.
  bool IsJSEventListener() const override { return true; }

 protected:
  // blink::JSBasedEventListener overrides:
  v8::Isolate* GetIsolate() const override {
    return event_listener_->GetIsolate();
  }
  ScriptState* GetScriptState() const override {
    return event_listener_->CallbackRelevantScriptState();
  }
  ScriptState* GetScriptStateOrReportError(
      const char* operation) const override {
    return event_listener_->CallbackRelevantScriptStateOrReportError(
        "EventListener", operation);
  }
  DOMWrapperWorld& GetWorld() const override {
    return event_listener_->GetWorld();
  }

 private:
  // blink::JSBasedEventListener override:
  void InvokeInternal(EventTarget&,
                      Event&,
                      v8::Local<v8::Value> js_event) override;

  const TraceWrapperMember<V8EventListener> event_listener_;
};

template <>
struct DowncastTraits<JSEventListener> {
  static bool AllowFrom(const EventListener& event_listener) {
    auto* js_based = DynamicTo<JSBasedEventListener>(event_listener);
    return js_based && js_based->IsJSEventListener();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_JS_EVENT_LISTENER_H_
