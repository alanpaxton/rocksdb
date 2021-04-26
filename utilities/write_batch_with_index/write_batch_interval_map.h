//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#pragma once

#ifndef ROCKSDB_LITE

#include <memory>

#include "memory/arena.h"
#include "memtable/skiplist.h"

namespace ROCKSDB_NAMESPACE {

//
// Range of values set or not set. Query or add ranges, but not delete them.
//
// Concurrency is guaranteed by the underlying skiplist.
//
// TODO how do we order and arrange updates so that concurrent readers always
// get a consistent view ?
//
template <class PointKey, class Comparator>
class IntervalMap {
 public:
  enum Marker { Start, Stop };
  class PointData {
   public:
    Marker marker;
    PointData(Marker marker) : marker(marker) {}
  };

 private:
  class PointEntry {
   public:
    uint32_t flagStart = 0xDEADBEEF;
    PointKey pointKey;
    PointData pointData;
    uint32_t flagEnd = 0xBADDCAFE;

    PointEntry(PointKey& key) : pointKey(key), pointData(Start) {}
    PointEntry(PointKey& key, Marker marker)
        : pointKey(key), pointData(marker) {}
  };

  class PointEntryComparator {
   public:
    PointEntryComparator(Comparator cmp) : comparator_(cmp){};
    int operator()(const PointEntry* entry1, const PointEntry* entry2) const;

   private:
    Comparator comparator_;
  };

  using PointEntrySkipList = SkipList<PointEntry*, PointEntryComparator>;

 public:
  // Create a new SkipList-based IntervalMap object that will allocate
  // memory using "*allocator".  Objects allocated in the allocator must
  // remain allocated for the lifetime of the range map object. The
  // IntervalMap is expected to share a lifetime with the write batch index,
  // and to share its allocator too.
  explicit IntervalMap(Comparator cmp, Allocator* allocator);

  // No copying allowed
  IntervalMap(const IntervalMap&) = delete;
  void operator=(const IntervalMap&) = delete;

  // Merge the semi-open interval [from_key, to_key) with the other intervals
  void AddInterval(const PointKey& from_key, const PointKey& to_key);

  // Check whether the key is in any of the intervals
  bool IsInInterval(const PointKey& key);

 public:
  void Clear();

 private:
  PointEntryComparator const comparator_;
  Allocator* const allocator_;  // Allocator used for allocations of nodes
  PointEntrySkipList skip_list;

  void FixIntervalFrom(const PointKey& from_key);
  void FixIntervalTo(const PointKey& to_key);
};

template <class PointKey, class Comparator>
IntervalMap<PointKey, Comparator>::IntervalMap(Comparator cmp,
                                               Allocator* allocator)
    : comparator_(cmp),
      allocator_(allocator),
      skip_list(comparator_, allocator) {}

template <class PointKey, class Comparator>
void IntervalMap<PointKey, Comparator>::AddInterval(const PointKey& from_key,
                                                    const PointKey& to_key) {
  FixIntervalFrom(from_key);
  FixIntervalTo(to_key);

  // Clear range
  // TODO check the underlying iter.Remove() is implemented
  typename PointEntrySkipList::Iterator iter(&skip_list);
  PointEntry from_entry(from_key, Start);
  PointEntry to_entry(to_key, Stop);

  // <= from_entry, meaning exactly the first entry we want to keep
  iter.SeekForPrev(&from_entry);
  iter.Next();
  while (iter.Valid()) {
    if (comparator_(&to_entry, iter.key()) <= 0) {
      break;
    }
    // iter should move on to Next() when we remove
    iter.Remove();
  }
}

template <class PointKey, class Comparator>
void IntervalMap<PointKey, Comparator>::FixIntervalFrom(
    const PointKey& from_key) {
  typename PointEntrySkipList::Iterator iter(&skip_list);
  PointEntry from_entry(from_key, Start);
  iter.SeekForPrev(&from_entry);
  if (iter.Valid()) {
    Marker marker = iter.key()->pointData.marker;
    if (marker == Start) return;  // an earlier start exists, so no start to add

    if (comparator_(&from_entry, iter.key()) == 0) {
      // There is a Stop where we want to have a start
      // Simply remove the Stop, and the preceding Start will cover ours
      iter.Remove();
      return;
    }
  }

  // We need a new Start marker in the index
  auto* mem = allocator_->Allocate(sizeof(PointEntry));
  auto* index_entry = new (mem) PointEntry(from_entry);
  skip_list.Insert(index_entry);
}

template <class PointKey, class Comparator>
void IntervalMap<PointKey, Comparator>::FixIntervalTo(const PointKey& to_key) {
  typename PointEntrySkipList::Iterator iter(&skip_list);
  PointEntry to_entry(to_key, Stop);
  iter.Seek(&to_entry);
  if (iter.Valid()) {
    Marker marker = iter.key()->pointData.marker;
    if (marker == Stop) return;  // a later stop exists, so no stop to add

    if (comparator_(&to_entry, iter.key()) == 0) {
      // There is a Start where we want to have a Stop
      // Simply remove the Start, and the succeeding Stop will cover ours
      iter.Remove();
      return;
    }
  }

  // We need a new Stop marker in the index
  auto* mem = allocator_->Allocate(sizeof(PointEntry));
  auto* index_entry = new (mem) PointEntry(to_entry);
  skip_list.Insert(index_entry);
}

template <class PointKey, class Comparator>
bool IntervalMap<PointKey, Comparator>::IsInInterval(const PointKey& key) {
  typename PointEntrySkipList::Iterator iter(&skip_list);
  PointEntry key_entry(key);
  iter.Seek(&key_entry);
  if (!iter.Valid()) {
    // No equal or greater entry exists, so key is outside any range
    return false;
  }
  Marker marker = iter.key()->pointData.marker;

  // Marker is strictly after this key
  if (comparator_(&key_entry, iter.key()) < 0) {
    if (marker == Stop) {
      return true;
    } else {
      assert(marker == Start);
      return false;
    }
  }

  // Marker at exactly this key
  if (marker == Start) {
    // the interval is always closed at Start
    return true;
  } else {
    assert(marker == Stop);
    // the interval is always open at Stop
    return false;
  }
}

template <class PointKey, class Comparator>
int IntervalMap<PointKey, Comparator>::PointEntryComparator::operator()(
    const IntervalMap<PointKey, Comparator>::PointEntry* entry1,
    const IntervalMap<PointKey, Comparator>::PointEntry* entry2) const {
  return comparator_(&entry1->pointKey, &entry2->pointKey);
}

}  // namespace ROCKSDB_NAMESPACE

#endif  // ROCKSDB_LITE
