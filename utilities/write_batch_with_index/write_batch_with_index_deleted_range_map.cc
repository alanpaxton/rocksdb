#ifndef ROCKSDB_LITE

#include "utilities/write_batch_with_index/write_batch_with_index_deleted_range_map.h"

#include "db/column_family.h"
#include "db/db_impl/db_impl.h"
#include "db/merge_context.h"
#include "db/merge_helper.h"
#include "rocksdb/comparator.h"
#include "rocksdb/db.h"
#include "rocksdb/utilities/write_batch_with_index.h"
#include "util/cast_util.h"
#include "util/coding.h"
#include "util/string_util.h"
#include "utilities/write_batch_with_index/write_batch_with_index_internal.h"

namespace ROCKSDB_NAMESPACE {

void DeletedRangeMap::AddInterval(const Slice& from_key, const Slice& to_key) {
  AddInterval(0, from_key, to_key);
}

void DeletedRangeMap::AddInterval(const uint32_t cf_id, const Slice& from_key,
                                  const Slice& to_key) {
  const WriteBatchIndexEntry from(0, cf_id, from_key.data(), from_key.size());
  const WriteBatchIndexEntry to(0, cf_id, to_key.data(), to_key.size());
  IntervalMap::AddInterval(from, to);
}

bool DeletedRangeMap::IsInInterval(const Slice& key) {
  return IsInInterval(0, key);
}

bool DeletedRangeMap::IsInInterval(const uint32_t cf_id, const Slice& key) {
  const WriteBatchIndexEntry keyEntry(0, cf_id, key.data(), key.size());
  return IntervalMap::IsInInterval(keyEntry);
}

}  // namespace ROCKSDB_NAMESPACE

#endif  // ROCKSDB_LITE