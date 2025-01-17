// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_QUERY_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_QUERY_H_

#include <string>

#include "base/macros.h"

namespace ash {

// AssistantQueryType ----------------------------------------------------------

// Defines possible types of an Assistant query.
enum class AssistantQueryType {
  kNull,   // See AssistantNullQuery.
  kText,   // See AssistantTextQuery.
  kVoice,  // See AssistantVoiceQuery.
};

// Defines possible source of an Assistant query. These values are persisted
// to logs. Entries should not be renumbered and numeric values should never
// be reused. Only append to this enum is allowed if the possible source grows.
enum class AssistantQuerySource {
  kUnspecified = 0,
  kDeepLink = 1,
  kDialogPlateTextField = 2,
  kStylus = 3,
  kSuggestionChip = 4,
  kVoiceInput = 5,
  kMaxValue = kVoiceInput
};

// AssistantQuery --------------------------------------------------------------

// Base class for an Assistant query.
class AssistantQuery {
 public:
  virtual ~AssistantQuery() = default;

  // Returns the type for the query.
  AssistantQueryType type() const { return type_; }

  // Returns the input source for the query.
  AssistantQuerySource source() const { return source_; }

  // Returns true if the query is empty, false otherwise.
  virtual bool Empty() const = 0;

 protected:
  AssistantQuery(AssistantQueryType type, AssistantQuerySource source)
      : type_(type), source_(source) {}

 private:
  const AssistantQueryType type_;

  const AssistantQuerySource source_;

  DISALLOW_COPY_AND_ASSIGN(AssistantQuery);
};

// AssistantNullQuery ----------------------------------------------------------

// An null Assistant query used to signify the absence of an Assistant query.
class AssistantNullQuery : public AssistantQuery {
 public:
  AssistantNullQuery()
      : AssistantQuery(AssistantQueryType::kNull,
                       AssistantQuerySource::kUnspecified) {}

  ~AssistantNullQuery() override = default;

  // AssistantQuery:
  bool Empty() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantNullQuery);
};

// AssistantTextQuery ----------------------------------------------------------

// An Assistant text query.
class AssistantTextQuery : public AssistantQuery {
 public:
  AssistantTextQuery(
      const std::string& text = std::string(),
      AssistantQuerySource source = AssistantQuerySource::kUnspecified)
      : AssistantQuery(AssistantQueryType::kText, source), text_(text) {}

  ~AssistantTextQuery() override = default;

  // AssistantQuery:
  bool Empty() const override;

  // Returns the text for the query.
  const std::string& text() const { return text_; }

 private:
  const std::string text_;

  DISALLOW_COPY_AND_ASSIGN(AssistantTextQuery);
};

// AssistantVoiceQuery ---------------------------------------------------------

// An Assistant voice query. At the start of a voice query, both the high and
// low confidence speech portions will be empty. As speech recognition
// continues, the low confidence portion will become non-empty. As speech
// recognition improves, both the high and low confidence portions of the query
// will be non-empty. When speech is fully recognized, only the high confidence
// portion will be populated.
class AssistantVoiceQuery : public AssistantQuery {
 public:
  AssistantVoiceQuery() : AssistantVoiceQuery(std::string(), std::string()) {}

  AssistantVoiceQuery(const std::string& high_confidence_speech,
                      const std::string& low_confidence_speech = std::string())
      : AssistantQuery(AssistantQueryType::kVoice,
                       AssistantQuerySource::kVoiceInput),
        high_confidence_speech_(high_confidence_speech),
        low_confidence_speech_(low_confidence_speech) {}

  ~AssistantVoiceQuery() override = default;

  // AssistantQuery:
  bool Empty() const override;

  // Returns speech for which we have high confidence of recognition.
  const std::string& high_confidence_speech() const {
    return high_confidence_speech_;
  }

  // Returns speech for which we have low confidence of recognition.
  const std::string& low_confidence_speech() const {
    return low_confidence_speech_;
  }

 private:
  const std::string high_confidence_speech_;
  const std::string low_confidence_speech_;

  DISALLOW_COPY_AND_ASSIGN(AssistantVoiceQuery);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_QUERY_H_
