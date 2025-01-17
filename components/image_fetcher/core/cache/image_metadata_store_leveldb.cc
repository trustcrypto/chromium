// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/cache/image_metadata_store_leveldb.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "base/system/sys_info.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/image_fetcher/core/cache/proto/cached_image_metadata.pb.h"
#include "components/leveldb_proto/proto_database_impl.h"

using image_fetcher::CachedImageMetadataProto;

// TODO(wylieb): Emit a histogram for various failure cases. Find a way to
// share it with other implementations.
namespace image_fetcher {

namespace {

int64_t ToDatabaseTime(base::Time time) {
  return time.since_origin().InMicroseconds();
}

// Statistics are logged to UMA with this string as part of histogram name. They
// can all be found under LevelDB.*.ImageDatabase. Changing this needs to
// synchronize with histograms.xml, AND will also become incompatible with older
// browsers still reporting the previous values.
const char kImageDatabaseUMAClientName[] = "CachedImageFetcherDatabase";

// The folder where the data will be stored on disk.
const char kImageDatabaseFolder[] = "cached_image_fetcher_images";

// The amount of data to build up in memory before converting to a sorted on-
// disk file.
const size_t kDatabaseWriteBufferSizeBytes = 64 * 1024;                 // 64KB
const size_t kDatabaseWriteBufferSizeBytesForLowEndDevice = 32 * 1024;  // 32KB

bool KeyMatcherFilter(std::string key, const std::string& other_key) {
  return key.compare(other_key) == 0;
}

bool SortByLastUsedTime(const CachedImageMetadataProto& a,
                        const CachedImageMetadataProto& b) {
  return a.last_used_time() < b.last_used_time();
}

}  // namespace

using MetadataKeyEntryVector =
    leveldb_proto::ProtoDatabase<CachedImageMetadataProto>::KeyEntryVector;

ImageMetadataStoreLevelDB::ImageMetadataStoreLevelDB(
    const base::FilePath& database_dir,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::Clock* clock)
    : ImageMetadataStoreLevelDB(
          database_dir,
          std::make_unique<
              leveldb_proto::ProtoDatabaseImpl<CachedImageMetadataProto>>(
              task_runner),
          clock) {}

ImageMetadataStoreLevelDB::ImageMetadataStoreLevelDB(
    const base::FilePath& database_dir,
    std::unique_ptr<leveldb_proto::ProtoDatabase<CachedImageMetadataProto>>
        database,
    base::Clock* clock)
    : estimated_size_(0),
      initialization_status_(InitializationStatus::UNINITIALIZED),
      database_dir_(database_dir),
      database_(std::move(database)),
      clock_(clock),
      weak_ptr_factory_(this) {}

ImageMetadataStoreLevelDB::~ImageMetadataStoreLevelDB() = default;

void ImageMetadataStoreLevelDB::Initialize(base::OnceClosure callback) {
  leveldb_env::Options options = leveldb_proto::CreateSimpleOptions();
  if (base::SysInfo::IsLowEndDevice()) {
    options.write_buffer_size = kDatabaseWriteBufferSizeBytesForLowEndDevice;
  } else {
    options.write_buffer_size = kDatabaseWriteBufferSizeBytes;
  }

  base::FilePath image_dir = database_dir_.AppendASCII(kImageDatabaseFolder);
  database_->Init(
      kImageDatabaseUMAClientName, image_dir, options,
      base::BindOnce(&ImageMetadataStoreLevelDB::OnDatabaseInitialized,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

bool ImageMetadataStoreLevelDB::IsInitialized() {
  return initialization_status_ == InitializationStatus::INITIALIZED ||
         initialization_status_ == InitializationStatus::INIT_FAILURE;
}

void ImageMetadataStoreLevelDB::SaveImageMetadata(const std::string& key,
                                                  const size_t data_size) {
  // If the database is not initialized yet, ignore the request.
  if (!IsInitialized()) {
    return;
  }
  estimated_size_ += data_size;

  int64_t current_time = ToDatabaseTime(clock_->Now());
  CachedImageMetadataProto metadata_proto;
  metadata_proto.set_key(key);
  metadata_proto.set_data_size(data_size);
  metadata_proto.set_creation_time(current_time);
  metadata_proto.set_last_used_time(current_time);

  auto entries_to_save = std::make_unique<MetadataKeyEntryVector>();
  entries_to_save->emplace_back(key, metadata_proto);
  database_->UpdateEntries(
      std::move(entries_to_save), std::make_unique<std::vector<std::string>>(),
      base::BindOnce(&ImageMetadataStoreLevelDB::OnImageUpdated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ImageMetadataStoreLevelDB::DeleteImageMetadata(const std::string& key) {
  // If the database is not initialized yet, ignore the request.
  if (!IsInitialized()) {
    return;
  }

  database_->UpdateEntries(
      std::make_unique<MetadataKeyEntryVector>(),
      std::make_unique<std::vector<std::string>>(
          std::initializer_list<std::string>({key})),
      base::BindOnce(&ImageMetadataStoreLevelDB::OnImageUpdated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ImageMetadataStoreLevelDB::UpdateImageMetadata(const std::string& key) {
  // If the database is not initialized yet, ignore the request.
  if (!IsInitialized()) {
    return;
  }

  database_->LoadEntriesWithFilter(
      base::BindRepeating(&KeyMatcherFilter, key),
      base::BindOnce(&ImageMetadataStoreLevelDB::UpdateImageMetadataImpl,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ImageMetadataStoreLevelDB::GetAllKeys(KeysCallback callback) {
  // If the database is not initialized yet, ignore the request.
  if (!IsInitialized()) {
    std::move(callback).Run({});
    return;
  }

  database_->LoadKeys(base::BindOnce(&ImageMetadataStoreLevelDB::GetAllKeysImpl,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     std::move(callback)));
}

int ImageMetadataStoreLevelDB::GetEstimatedSize() {
  return estimated_size_;
}

void ImageMetadataStoreLevelDB::EvictImageMetadata(base::Time expiration_time,
                                                   const size_t bytes_left,
                                                   KeysCallback callback) {
  // If the database is not initialized yet, ignore the request.
  if (!IsInitialized()) {
    std::move(callback).Run({});
    return;
  }

  database_->LoadEntries(
      base::BindOnce(&ImageMetadataStoreLevelDB::EvictImageMetadataImpl,
                     weak_ptr_factory_.GetWeakPtr(), expiration_time,
                     bytes_left, std::move(callback)));
}

void ImageMetadataStoreLevelDB::OnDatabaseInitialized(
    base::OnceClosure callback,
    bool success) {
  initialization_status_ = success ? InitializationStatus::INITIALIZED
                                   : InitializationStatus::INIT_FAILURE;
  std::move(callback).Run();
}

void ImageMetadataStoreLevelDB::OnImageUpdated(bool success) {
  DVLOG_IF(1, !success) << "ImageMetadataStoreLevelDB update failed.";
}

void ImageMetadataStoreLevelDB::UpdateImageMetadataImpl(
    bool success,
    std::unique_ptr<std::vector<CachedImageMetadataProto>> entries) {
  // If there was a failure, or an entry wasn't found then there's nothing left
  // to do.
  if (!success || entries->size() != 1) {
    return;
  }

  CachedImageMetadataProto metadata_proto = (*entries)[0];
  metadata_proto.set_last_used_time(ToDatabaseTime(clock_->Now()));

  auto entries_to_save = std::make_unique<MetadataKeyEntryVector>();
  entries_to_save->emplace_back(metadata_proto.key(), metadata_proto);
  database_->UpdateEntries(
      std::move(entries_to_save), std::make_unique<std::vector<std::string>>(),
      base::BindOnce(&ImageMetadataStoreLevelDB::OnImageUpdated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ImageMetadataStoreLevelDB::GetAllKeysImpl(
    KeysCallback callback,
    bool success,
    std::unique_ptr<std::vector<std::string>> keys) {
  if (!success) {
    std::move(callback).Run({});
    return;
  }

  std::move(callback).Run(*keys.get());
}

void ImageMetadataStoreLevelDB::EvictImageMetadataImpl(
    base::Time expiration_time,
    const size_t bytes_left,
    KeysCallback callback,
    bool success,
    std::unique_ptr<std::vector<CachedImageMetadataProto>> entries) {
  // In the case where the entries failed to load (LevelDB error), then do
  // nothing.
  if (!success) {
    std::move(callback).Run({});
    return;
  }

  size_t total_bytes_stored = 0;
  int64_t expiration_database_time = ToDatabaseTime(expiration_time);
  std::vector<std::string> keys_to_remove;
  for (const CachedImageMetadataProto& entry : *entries) {
    if (entry.creation_time() <= expiration_database_time) {
      keys_to_remove.emplace_back(entry.key());
    } else {
      total_bytes_stored += entry.data_size();
    }
  }

  // Only sort and remove more if the byte limit isn't satisfied.
  if (total_bytes_stored > bytes_left) {
    std::sort(entries->begin(), entries->end(), SortByLastUsedTime);
    for (const CachedImageMetadataProto& entry : *entries) {
      if (total_bytes_stored <= bytes_left) {
        break;
      }

      keys_to_remove.emplace_back(entry.key());
      total_bytes_stored -= entry.data_size();
    }
  }

  estimated_size_ = total_bytes_stored;

  if (keys_to_remove.empty()) {
    std::move(callback).Run({});
    return;
  }

  database_->UpdateEntries(
      std::make_unique<MetadataKeyEntryVector>(),
      std::make_unique<std::vector<std::string>>(keys_to_remove),
      base::BindOnce(&ImageMetadataStoreLevelDB::OnEvictImageMetadataDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     keys_to_remove));
}

void ImageMetadataStoreLevelDB::OnEvictImageMetadataDone(
    KeysCallback callback,
    std::vector<std::string> deleted_keys,
    bool success) {
  // If the entries have failed to update due to some LevelDB error, return
  // an empty list.
  if (!success) {
    std::move(callback).Run({});
    return;
  }

  std::move(callback).Run(deleted_keys);
}

}  // namespace image_fetcher
