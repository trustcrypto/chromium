// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBMIDI_MIDI_DISPATCHER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBMIDI_MIDI_DISPATCHER_H_

#include "media/midi/midi_service.mojom-blink.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class MIDIAccessor;

class MIDIDispatcher : public GarbageCollectedFinalized<MIDIDispatcher>,
                       public midi::mojom::blink::MidiSessionClient {
 public:
  static MIDIDispatcher& Instance();
  ~MIDIDispatcher() override;

  void Trace(Visitor* visitor);

  void AddAccessor(MIDIAccessor* accessor);
  void RemoveAccessor(MIDIAccessor* accessor);
  void SendMidiData(uint32_t port,
                    const uint8_t* data,
                    wtf_size_t length,
                    base::TimeTicks timestamp);

  // midi::mojom::blink::MidiSessionClient implementation.
  // All of the following methods are run on the main thread.
  void AddInputPort(midi::mojom::blink::PortInfoPtr info) override;
  void AddOutputPort(midi::mojom::blink::PortInfoPtr info) override;
  void SetInputPortState(uint32_t port,
                         midi::mojom::blink::PortState state) override;
  void SetOutputPortState(uint32_t port,
                          midi::mojom::blink::PortState state) override;
  void SessionStarted(midi::mojom::blink::Result result) override;
  void AcknowledgeSentData(uint32_t bytes) override;
  void DataReceived(uint32_t port,
                    const Vector<uint8_t>& data,
                    base::TimeTicks timestamp) override;

 private:
  friend class ConstructTrait<MIDIDispatcher>;

  MIDIDispatcher();

  midi::mojom::blink::MidiSessionProvider& GetMidiSessionProvider();
  midi::mojom::blink::MidiSession& GetMidiSession();

  // Keeps track of all MIDI accessors.
  typedef Vector<MIDIAccessor*> AccessorList;
  AccessorList accessors_;

  // Represents accessors that are waiting for a session being open.
  typedef Vector<MIDIAccessor*> AccessorQueue;
  AccessorQueue accessors_waiting_session_queue_;

  // Represents a result on starting a session.
  midi::mojom::blink::Result session_result_ =
      midi::mojom::Result::NOT_INITIALIZED;

  // Holds MidiPortInfoList for input ports and output ports.
  Vector<midi::mojom::blink::PortInfo> inputs_;
  Vector<midi::mojom::blink::PortInfo> outputs_;

  size_t unacknowledged_bytes_sent_ = 0u;

  midi::mojom::blink::MidiSessionProviderPtr midi_session_provider_;
  midi::mojom::blink::MidiSessionPtr midi_session_;

  mojo::Binding<midi::mojom::blink::MidiSessionClient> binding_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBMIDI_MIDI_DISPATCHER_H_
