// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//
// This file implements the "bridge" between Java and C++
// for ROCKSDB_NAMESPACE::Transaction.

#include "rocksdb/utilities/transaction.h"

#include <jni.h>

#include <functional>

#include "api_columnfamilyhandle.h"
#include "api_rocksdb.h"
#include "api_transaction.h"
#include "api_iterator.h"
#include "include/org_rocksdb_Transaction.h"
#include "rocksjni/cplusplus_to_java_convert.h"
#include "rocksjni/portal.h"

using API_TXN = APITransaction<ROCKSDB_NAMESPACE::StackableDB>;

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4503)  // identifier' : decorated name length
                                 // exceeded, name was truncated
#endif

/*
 * Class:     org_rocksdb_Transaction
 * Method:    setSnapshot
 * Signature: (J)V
 */
void Java_org_rocksdb_Transaction_setSnapshot(JNIEnv* /*env*/, jobject /*jobj*/,
                                              jlong jhandle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);

  apiTxn->SetSnapshot();
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    setSnapshotOnNextOperation
 * Signature: (J)V
 */
void Java_org_rocksdb_Transaction_setSnapshotOnNextOperation__J(
    JNIEnv* /*env*/, jobject /*jobj*/, jlong jhandle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  apiTxn->SetSnapshotOnNextOperation(nullptr);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    setSnapshotOnNextOperation
 * Signature: (JJ)V
 */
void Java_org_rocksdb_Transaction_setSnapshotOnNextOperation__JJ(
    JNIEnv* /*env*/, jobject /*jobj*/, jlong jhandle,
    jlong jtxn_notifier_handle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  auto* txn_notifier = reinterpret_cast<
      std::shared_ptr<ROCKSDB_NAMESPACE::TransactionNotifierJniCallback>*>(
      jtxn_notifier_handle);
  apiTxn->SetSnapshotOnNextOperation(*txn_notifier);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    getSnapshot
 * Signature: (J)J
 */
jlong Java_org_rocksdb_Transaction_getSnapshot(JNIEnv* /*env*/,
                                               jobject /*jobj*/,
                                               jlong jhandle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  const ROCKSDB_NAMESPACE::Snapshot* snapshot = apiTxn->GetSnapshot();
  return GET_CPLUSPLUS_POINTER(snapshot);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    clearSnapshot
 * Signature: (J)V
 */
void Java_org_rocksdb_Transaction_clearSnapshot(JNIEnv* /*env*/,
                                                jobject /*jobj*/,
                                                jlong jhandle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  apiTxn->ClearSnapshot();
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    prepare
 * Signature: (J)V
 */
void Java_org_rocksdb_Transaction_prepare(JNIEnv* env, jobject /*jobj*/,
                                          jlong jhandle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  ROCKSDB_NAMESPACE::Status s = apiTxn->Prepare();
  if (!s.ok()) {
    ROCKSDB_NAMESPACE::RocksDBExceptionJni::ThrowNew(env, s);
  }
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    commit
 * Signature: (J)V
 */
void Java_org_rocksdb_Transaction_commit(JNIEnv* env, jobject /*jobj*/,
                                         jlong jhandle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  ROCKSDB_NAMESPACE::Status s = apiTxn->Commit();
  if (!s.ok()) {
    ROCKSDB_NAMESPACE::RocksDBExceptionJni::ThrowNew(env, s);
  }
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    rollback
 * Signature: (J)V
 */
void Java_org_rocksdb_Transaction_rollback(JNIEnv* env, jobject /*jobj*/,
                                           jlong jhandle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  ROCKSDB_NAMESPACE::Status s = apiTxn->Rollback();
  if (!s.ok()) {
    ROCKSDB_NAMESPACE::RocksDBExceptionJni::ThrowNew(env, s);
  }
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    setSavePoint
 * Signature: (J)V
 */
void Java_org_rocksdb_Transaction_setSavePoint(JNIEnv* /*env*/,
                                               jobject /*jobj*/,
                                               jlong jhandle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  apiTxn->SetSavePoint();
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    rollbackToSavePoint
 * Signature: (J)V
 */
void Java_org_rocksdb_Transaction_rollbackToSavePoint(JNIEnv* env,
                                                      jobject /*jobj*/,
                                                      jlong jhandle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  ROCKSDB_NAMESPACE::Status s = apiTxn->RollbackToSavePoint();
  if (!s.ok()) {
    ROCKSDB_NAMESPACE::RocksDBExceptionJni::ThrowNew(env, s);
  }
}

typedef std::function<ROCKSDB_NAMESPACE::Status(
    const ROCKSDB_NAMESPACE::ReadOptions&, const ROCKSDB_NAMESPACE::Slice&,
    std::string*)>
    FnGet;

// TODO(AR) consider refactoring to share this between here and rocksjni.cc
jbyteArray txn_get_helper(JNIEnv* env, const FnGet& fn_get,
                          const jlong& jread_options_handle,
                          const jbyteArray& jkey, const jint& jkey_part_len) {
  jbyte* key = env->GetByteArrayElements(jkey, nullptr);
  if (key == nullptr) {
    // exception thrown: OutOfMemoryError
    return nullptr;
  }
  ROCKSDB_NAMESPACE::Slice key_slice(reinterpret_cast<char*>(key),
                                     jkey_part_len);

  auto* read_options =
      reinterpret_cast<ROCKSDB_NAMESPACE::ReadOptions*>(jread_options_handle);
  std::string value;
  ROCKSDB_NAMESPACE::Status s = fn_get(*read_options, key_slice, &value);

  // trigger java unref on key.
  // by passing JNI_ABORT, it will simply release the reference without
  // copying the result back to the java byte array.
  env->ReleaseByteArrayElements(jkey, key, JNI_ABORT);

  if (s.IsNotFound()) {
    return nullptr;
  }

  if (s.ok()) {
    jbyteArray jret_value = env->NewByteArray(static_cast<jsize>(value.size()));
    if (jret_value == nullptr) {
      // exception thrown: OutOfMemoryError
      return nullptr;
    }
    env->SetByteArrayRegion(jret_value, 0, static_cast<jsize>(value.size()),
                            const_cast<jbyte*>(reinterpret_cast<const jbyte*>(value.c_str())));
    if (env->ExceptionCheck()) {
      // exception thrown: ArrayIndexOutOfBoundsException
      return nullptr;
    }
    return jret_value;
  }

  ROCKSDB_NAMESPACE::RocksDBExceptionJni::ThrowNew(env, s);
  return nullptr;
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    get
 * Signature: (JJ[BIJ)[B
 */
jbyteArray Java_org_rocksdb_Transaction_get__JJ_3BIJ(
    JNIEnv* env, jobject /*jobj*/, jlong jhandle, jlong jread_options_handle,
    jbyteArray jkey, jint jkey_part_len, jlong jcolumn_family_handle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  auto cfh = APIColumnFamilyHandle<ROCKSDB_NAMESPACE::DB>::lock(
      env, jcolumn_family_handle);
  if (!cfh) {
    // CFH exception
    return nullptr;
  }
  FnGet fn_get =
      std::bind<ROCKSDB_NAMESPACE::Status (ROCKSDB_NAMESPACE::Transaction::*)(
          const ROCKSDB_NAMESPACE::ReadOptions&,
          ROCKSDB_NAMESPACE::ColumnFamilyHandle*,
          const ROCKSDB_NAMESPACE::Slice&, std::string*)>(
          &ROCKSDB_NAMESPACE::Transaction::Get, apiTxn.get(),
          std::placeholders::_1, cfh.get(), std::placeholders::_2,
          std::placeholders::_3);
  return txn_get_helper(env, fn_get, jread_options_handle, jkey, jkey_part_len);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    get
 * Signature: (JJ[BI)[B
 */
jbyteArray Java_org_rocksdb_Transaction_get__JJ_3BI(
    JNIEnv* env, jobject /*jobj*/, jlong jhandle, jlong jread_options_handle,
    jbyteArray jkey, jint jkey_part_len) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  FnGet fn_get =
      std::bind<ROCKSDB_NAMESPACE::Status (ROCKSDB_NAMESPACE::Transaction::*)(
          const ROCKSDB_NAMESPACE::ReadOptions&,
          const ROCKSDB_NAMESPACE::Slice&, std::string*)>(
          &ROCKSDB_NAMESPACE::Transaction::Get, apiTxn.get(),
          std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
  return txn_get_helper(env, fn_get, jread_options_handle, jkey, jkey_part_len);
}

// TODO(AR) consider refactoring to share this between here and rocksjni.cc
// used by txn_multi_get_helper below
std::vector<ROCKSDB_NAMESPACE::ColumnFamilyHandle*> txn_column_families_helper(
    JNIEnv* env, jlongArray jcolumn_family_handles, bool* has_exception) {
  std::vector<ROCKSDB_NAMESPACE::ColumnFamilyHandle*> cf_handles;
  if (jcolumn_family_handles != nullptr) {
    const jsize len_cols = env->GetArrayLength(jcolumn_family_handles);
    if (len_cols > 0) {
      if (env->EnsureLocalCapacity(len_cols) != 0) {
        // out of memory
        *has_exception = JNI_TRUE;
        return std::vector<ROCKSDB_NAMESPACE::ColumnFamilyHandle*>();
      }

      jlong* jcfh = env->GetLongArrayElements(jcolumn_family_handles, nullptr);
      if (jcfh == nullptr) {
        // exception thrown: OutOfMemoryError
        *has_exception = JNI_TRUE;
        return std::vector<ROCKSDB_NAMESPACE::ColumnFamilyHandle*>();
      }
      for (int i = 0; i < len_cols; i++) {
        auto cfh =
            APIColumnFamilyHandle<ROCKSDB_NAMESPACE::DB>::lock(env, jcfh[i]);
        if (!cfh) {
          // CFH exception
          *has_exception = JNI_TRUE;
          return std::vector<ROCKSDB_NAMESPACE::ColumnFamilyHandle*>();
        }
        cf_handles.push_back(cfh.get());
      }
      env->ReleaseLongArrayElements(jcolumn_family_handles, jcfh, JNI_ABORT);
    }
  }
  return cf_handles;
}

typedef std::function<std::vector<ROCKSDB_NAMESPACE::Status>(
    const ROCKSDB_NAMESPACE::ReadOptions&,
    const std::vector<ROCKSDB_NAMESPACE::Slice>&, std::vector<std::string>*)>
    FnMultiGet;

void free_parts(
    JNIEnv* env,
    std::vector<std::tuple<jbyteArray, jbyte*, jobject>>& parts_to_free) {
  for (auto& value : parts_to_free) {
    jobject jk;
    jbyteArray jk_ba;
    jbyte* jk_val;
    std::tie(jk_ba, jk_val, jk) = value;
    env->ReleaseByteArrayElements(jk_ba, jk_val, JNI_ABORT);
    env->DeleteLocalRef(jk);
  }
}

// TODO(AR) consider refactoring to share this between here and rocksjni.cc
// cf multi get
jobjectArray txn_multi_get_helper(JNIEnv* env, const FnMultiGet& fn_multi_get,
                                  const jlong& jread_options_handle,
                                  const jobjectArray& jkey_parts) {
  const jsize len_key_parts = env->GetArrayLength(jkey_parts);
  if (env->EnsureLocalCapacity(len_key_parts) != 0) {
    // out of memory
    return nullptr;
  }

  std::vector<ROCKSDB_NAMESPACE::Slice> key_parts;
  std::vector<std::tuple<jbyteArray, jbyte*, jobject>> key_parts_to_free;
  for (int i = 0; i < len_key_parts; i++) {
    const jobject jk = env->GetObjectArrayElement(jkey_parts, i);
    if (env->ExceptionCheck()) {
      // exception thrown: ArrayIndexOutOfBoundsException
      free_parts(env, key_parts_to_free);
      return nullptr;
    }
    jbyteArray jk_ba = reinterpret_cast<jbyteArray>(jk);
    const jsize len_key = env->GetArrayLength(jk_ba);
    if (env->EnsureLocalCapacity(len_key) != 0) {
      // out of memory
      env->DeleteLocalRef(jk);
      free_parts(env, key_parts_to_free);
      return nullptr;
    }
    jbyte* jk_val = env->GetByteArrayElements(jk_ba, nullptr);
    if (jk_val == nullptr) {
      // exception thrown: OutOfMemoryError
      env->DeleteLocalRef(jk);
      free_parts(env, key_parts_to_free);
      return nullptr;
    }

    ROCKSDB_NAMESPACE::Slice key_slice(reinterpret_cast<char*>(jk_val),
                                       len_key);
    key_parts.push_back(key_slice);

    key_parts_to_free.push_back(std::make_tuple(jk_ba, jk_val, jk));
  }

  auto* read_options =
      reinterpret_cast<ROCKSDB_NAMESPACE::ReadOptions*>(jread_options_handle);
  std::vector<std::string> value_parts;
  std::vector<ROCKSDB_NAMESPACE::Status> s =
      fn_multi_get(*read_options, key_parts, &value_parts);

  // free up allocated byte arrays
  free_parts(env, key_parts_to_free);

  // prepare the results
  const jclass jcls_ba = env->FindClass("[B");
  jobjectArray jresults =
      env->NewObjectArray(static_cast<jsize>(s.size()), jcls_ba, nullptr);
  if (jresults == nullptr) {
    // exception thrown: OutOfMemoryError
    return nullptr;
  }

  // add to the jresults
  for (std::vector<ROCKSDB_NAMESPACE::Status>::size_type i = 0; i != s.size();
       i++) {
    if (s[i].ok()) {
      jbyteArray jentry_value =
          env->NewByteArray(static_cast<jsize>(value_parts[i].size()));
      if (jentry_value == nullptr) {
        // exception thrown: OutOfMemoryError
        return nullptr;
      }

      env->SetByteArrayRegion(
          jentry_value, 0, static_cast<jsize>(value_parts[i].size()),
          const_cast<jbyte*>(reinterpret_cast<const jbyte*>(value_parts[i].c_str())));
      if (env->ExceptionCheck()) {
        // exception thrown: ArrayIndexOutOfBoundsException
        env->DeleteLocalRef(jentry_value);
        return nullptr;
      }

      env->SetObjectArrayElement(jresults, static_cast<jsize>(i), jentry_value);
      env->DeleteLocalRef(jentry_value);
    }
  }

  return jresults;
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    multiGet
 * Signature: (JJ[[B[J)[[B
 */
jobjectArray Java_org_rocksdb_Transaction_multiGet__JJ_3_3B_3J(
    JNIEnv* env, jobject /*jobj*/, jlong jhandle, jlong jread_options_handle,
    jobjectArray jkey_parts, jlongArray jcolumn_family_handles) {
  bool has_exception = false;
  const std::vector<ROCKSDB_NAMESPACE::ColumnFamilyHandle*>
      column_family_handles = txn_column_families_helper(
          env, jcolumn_family_handles, &has_exception);
  if (has_exception) {
    // exception thrown: OutOfMemoryError
    return nullptr;
  }
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  FnMultiGet fn_multi_get = std::bind<std::vector<ROCKSDB_NAMESPACE::Status> (
      ROCKSDB_NAMESPACE::Transaction::*)(
      const ROCKSDB_NAMESPACE::ReadOptions&,
      const std::vector<ROCKSDB_NAMESPACE::ColumnFamilyHandle*>&,
      const std::vector<ROCKSDB_NAMESPACE::Slice>&, std::vector<std::string>*)>(
      &ROCKSDB_NAMESPACE::Transaction::MultiGet, apiTxn.get(),
      std::placeholders::_1, column_family_handles, std::placeholders::_2,
      std::placeholders::_3);
  return txn_multi_get_helper(env, fn_multi_get, jread_options_handle,
                              jkey_parts);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    multiGet
 * Signature: (JJ[[B)[[B
 */
jobjectArray Java_org_rocksdb_Transaction_multiGet__JJ_3_3B(
    JNIEnv* env, jobject /*jobj*/, jlong jhandle, jlong jread_options_handle,
    jobjectArray jkey_parts) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  FnMultiGet fn_multi_get = std::bind<std::vector<ROCKSDB_NAMESPACE::Status> (
      ROCKSDB_NAMESPACE::Transaction::*)(
      const ROCKSDB_NAMESPACE::ReadOptions&,
      const std::vector<ROCKSDB_NAMESPACE::Slice>&, std::vector<std::string>*)>(
      &ROCKSDB_NAMESPACE::Transaction::MultiGet, apiTxn.get(),
      std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
  return txn_multi_get_helper(env, fn_multi_get, jread_options_handle,
                              jkey_parts);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    getForUpdate
 * Signature: (JJ[BIJZZ)[B
 */
jbyteArray Java_org_rocksdb_Transaction_getForUpdate__JJ_3BIJZZ(
    JNIEnv* env, jobject /*jobj*/, jlong jhandle, jlong jread_options_handle,
    jbyteArray jkey, jint jkey_part_len, jlong jcolumn_family_handle,
    jboolean jexclusive, jboolean jdo_validate) {
  auto cfh = APIColumnFamilyHandle<ROCKSDB_NAMESPACE::DB>::lock(
      env, jcolumn_family_handle);
  if (!cfh) {
    // CFH exception
    return nullptr;
  }
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  FnGet fn_get_for_update =
      std::bind<ROCKSDB_NAMESPACE::Status (ROCKSDB_NAMESPACE::Transaction::*)(
          const ROCKSDB_NAMESPACE::ReadOptions&,
          ROCKSDB_NAMESPACE::ColumnFamilyHandle*,
          const ROCKSDB_NAMESPACE::Slice&, std::string*, bool, bool)>(
          &ROCKSDB_NAMESPACE::Transaction::GetForUpdate, apiTxn.get(),
          std::placeholders::_1, cfh.get(), std::placeholders::_2,
          std::placeholders::_3, jexclusive, jdo_validate);
  return txn_get_helper(env, fn_get_for_update, jread_options_handle, jkey,
                        jkey_part_len);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    getForUpdate
 * Signature: (JJ[BIZZ)[B
 */
jbyteArray Java_org_rocksdb_Transaction_getForUpdate__JJ_3BIZZ(
    JNIEnv* env, jobject /*jobj*/, jlong jhandle, jlong jread_options_handle,
    jbyteArray jkey, jint jkey_part_len, jboolean jexclusive,
    jboolean jdo_validate) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  FnGet fn_get_for_update =
      std::bind<ROCKSDB_NAMESPACE::Status (ROCKSDB_NAMESPACE::Transaction::*)(
          const ROCKSDB_NAMESPACE::ReadOptions&,
          const ROCKSDB_NAMESPACE::Slice&, std::string*, bool, bool)>(
          &ROCKSDB_NAMESPACE::Transaction::GetForUpdate, apiTxn.get(),
          std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
          jexclusive, jdo_validate);
  return txn_get_helper(env, fn_get_for_update, jread_options_handle, jkey,
                        jkey_part_len);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    multiGetForUpdate
 * Signature: (JJ[[B[J)[[B
 */
jobjectArray Java_org_rocksdb_Transaction_multiGetForUpdate__JJ_3_3B_3J(
    JNIEnv* env, jobject /*jobj*/, jlong jhandle, jlong jread_options_handle,
    jobjectArray jkey_parts, jlongArray jcolumn_family_handles) {
  bool has_exception = false;
  const std::vector<ROCKSDB_NAMESPACE::ColumnFamilyHandle*>
      column_family_handles = txn_column_families_helper(
          env, jcolumn_family_handles, &has_exception);
  if (has_exception) {
    // exception thrown: OutOfMemoryError
    return nullptr;
  }
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  FnMultiGet fn_multi_get_for_update = std::bind<std::vector<
      ROCKSDB_NAMESPACE::Status> (ROCKSDB_NAMESPACE::Transaction::*)(
      const ROCKSDB_NAMESPACE::ReadOptions&,
      const std::vector<ROCKSDB_NAMESPACE::ColumnFamilyHandle*>&,
      const std::vector<ROCKSDB_NAMESPACE::Slice>&, std::vector<std::string>*)>(
      &ROCKSDB_NAMESPACE::Transaction::MultiGetForUpdate, apiTxn.get(),
      std::placeholders::_1, column_family_handles, std::placeholders::_2,
      std::placeholders::_3);
  return txn_multi_get_helper(env, fn_multi_get_for_update,
                              jread_options_handle, jkey_parts);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    multiGetForUpdate
 * Signature: (JJ[[B)[[B
 */
jobjectArray Java_org_rocksdb_Transaction_multiGetForUpdate__JJ_3_3B(
    JNIEnv* env, jobject /*jobj*/, jlong jhandle, jlong jread_options_handle,
    jobjectArray jkey_parts) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  FnMultiGet fn_multi_get_for_update = std::bind<std::vector<
      ROCKSDB_NAMESPACE::Status> (ROCKSDB_NAMESPACE::Transaction::*)(
      const ROCKSDB_NAMESPACE::ReadOptions&,
      const std::vector<ROCKSDB_NAMESPACE::Slice>&, std::vector<std::string>*)>(
      &ROCKSDB_NAMESPACE::Transaction::MultiGetForUpdate, apiTxn.get(),
      std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
  return txn_multi_get_helper(env, fn_multi_get_for_update,
                              jread_options_handle, jkey_parts);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    getIterator
 * Signature: (JJ)J
 */
jlong Java_org_rocksdb_Transaction_getIterator__JJ(JNIEnv* /*env*/,
                                                   jobject /*jobj*/,
                                                   jlong jhandle,
                                                   jlong jread_options_handle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  auto* read_options =
      reinterpret_cast<ROCKSDB_NAMESPACE::ReadOptions*>(jread_options_handle);
  auto* it = apiTxn->GetIterator(*read_options);
  auto* apiIt = new APIIterator<ROCKSDB_NAMESPACE::StackableDB>(
      apiTxn.db, std::shared_ptr<ROCKSDB_NAMESPACE::Iterator>(it));
  return GET_CPLUSPLUS_POINTER(apiIt);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    getIterator
 * Signature: (JJJ)J
 */
jlong Java_org_rocksdb_Transaction_getIterator__JJJ(
    JNIEnv* env, jobject /*jobj*/, jlong jhandle, jlong jread_options_handle,
    jlong jcolumn_family_handle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  auto* read_options =
      reinterpret_cast<ROCKSDB_NAMESPACE::ReadOptions*>(jread_options_handle);
  auto cfh = APIColumnFamilyHandle<ROCKSDB_NAMESPACE::DB>::lock(
      env, jcolumn_family_handle);
  if (!cfh) {
    // CFH exception
    return 0L;
  }
  auto* it = apiTxn->GetIterator(*read_options, cfh.get());
  auto* apiIt = new APIIterator<ROCKSDB_NAMESPACE::StackableDB>(
      apiTxn.db, std::shared_ptr<ROCKSDB_NAMESPACE::Iterator>(it), cfh);
  return GET_CPLUSPLUS_POINTER(apiIt);
}

typedef std::function<ROCKSDB_NAMESPACE::Status(
    const ROCKSDB_NAMESPACE::Slice&, const ROCKSDB_NAMESPACE::Slice&)>
    FnWriteKV;

// TODO(AR) consider refactoring to share this between here and rocksjni.cc
void txn_write_kv_helper(JNIEnv* env, const FnWriteKV& fn_write_kv,
                         const jbyteArray& jkey, const jint& jkey_part_len,
                         const jbyteArray& jval, const jint& jval_len) {
  jbyte* key = env->GetByteArrayElements(jkey, nullptr);
  if (key == nullptr) {
    // exception thrown: OutOfMemoryError
    return;
  }
  jbyte* value = env->GetByteArrayElements(jval, nullptr);
  if (value == nullptr) {
    // exception thrown: OutOfMemoryError
    env->ReleaseByteArrayElements(jkey, key, JNI_ABORT);
    return;
  }
  ROCKSDB_NAMESPACE::Slice key_slice(reinterpret_cast<char*>(key),
                                     jkey_part_len);
  ROCKSDB_NAMESPACE::Slice value_slice(reinterpret_cast<char*>(value),
                                       jval_len);

  ROCKSDB_NAMESPACE::Status s = fn_write_kv(key_slice, value_slice);

  // trigger java unref on key.
  // by passing JNI_ABORT, it will simply release the reference without
  // copying the result back to the java byte array.
  env->ReleaseByteArrayElements(jval, value, JNI_ABORT);
  env->ReleaseByteArrayElements(jkey, key, JNI_ABORT);

  if (s.ok()) {
    return;
  }
  ROCKSDB_NAMESPACE::RocksDBExceptionJni::ThrowNew(env, s);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    put
 * Signature: (J[BI[BIJZ)V
 */
void Java_org_rocksdb_Transaction_put__J_3BI_3BIJZ(
    JNIEnv* env, jobject /*jobj*/, jlong jhandle, jbyteArray jkey,
    jint jkey_part_len, jbyteArray jval, jint jval_len,
    jlong jcolumn_family_handle, jboolean jassume_tracked) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  auto cfh = APIColumnFamilyHandle<ROCKSDB_NAMESPACE::DB>::lock(
      env, jcolumn_family_handle);
  if (!cfh) {
    // CFH exception
    return;
  }
  FnWriteKV fn_put =
      std::bind<ROCKSDB_NAMESPACE::Status (ROCKSDB_NAMESPACE::Transaction::*)(
          ROCKSDB_NAMESPACE::ColumnFamilyHandle*,
          const ROCKSDB_NAMESPACE::Slice&, const ROCKSDB_NAMESPACE::Slice&,
          bool)>(&ROCKSDB_NAMESPACE::Transaction::Put, apiTxn.get(), cfh.get(),
                 std::placeholders::_1, std::placeholders::_2, jassume_tracked);
  txn_write_kv_helper(env, fn_put, jkey, jkey_part_len, jval, jval_len);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    put
 * Signature: (J[BI[BI)V
 */
void Java_org_rocksdb_Transaction_put__J_3BI_3BI(JNIEnv* env, jobject /*jobj*/,
                                                 jlong jhandle, jbyteArray jkey,
                                                 jint jkey_part_len,
                                                 jbyteArray jval,
                                                 jint jval_len) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  FnWriteKV fn_put =
      std::bind<ROCKSDB_NAMESPACE::Status (ROCKSDB_NAMESPACE::Transaction::*)(
          const ROCKSDB_NAMESPACE::Slice&, const ROCKSDB_NAMESPACE::Slice&)>(
          &ROCKSDB_NAMESPACE::Transaction::Put, apiTxn.get(),
          std::placeholders::_1, std::placeholders::_2);
  txn_write_kv_helper(env, fn_put, jkey, jkey_part_len, jval, jval_len);
}

typedef std::function<ROCKSDB_NAMESPACE::Status(
    const ROCKSDB_NAMESPACE::SliceParts&, const ROCKSDB_NAMESPACE::SliceParts&)>
    FnWriteKVParts;

// TODO(AR) consider refactoring to share this between here and rocksjni.cc
void txn_write_kv_parts_helper(JNIEnv* env,
                               const FnWriteKVParts& fn_write_kv_parts,
                               const jobjectArray& jkey_parts,
                               const jint& jkey_parts_len,
                               const jobjectArray& jvalue_parts,
                               const jint& jvalue_parts_len) {
#ifndef DEBUG
  (void) jvalue_parts_len;
#else
  assert(jkey_parts_len == jvalue_parts_len);
#endif

  auto key_parts = std::vector<ROCKSDB_NAMESPACE::Slice>();
  auto value_parts = std::vector<ROCKSDB_NAMESPACE::Slice>();
  auto jparts_to_free = std::vector<std::tuple<jbyteArray, jbyte*, jobject>>();

  // convert java key_parts/value_parts byte[][] to Slice(s)
  for (jsize i = 0; i < jkey_parts_len; ++i) {
    const jobject jobj_key_part = env->GetObjectArrayElement(jkey_parts, i);
    if (env->ExceptionCheck()) {
      // exception thrown: ArrayIndexOutOfBoundsException
      free_parts(env, jparts_to_free);
      return;
    }
    const jobject jobj_value_part = env->GetObjectArrayElement(jvalue_parts, i);
    if (env->ExceptionCheck()) {
      // exception thrown: ArrayIndexOutOfBoundsException
      env->DeleteLocalRef(jobj_key_part);
      free_parts(env, jparts_to_free);
      return;
    }

    const jbyteArray jba_key_part = reinterpret_cast<jbyteArray>(jobj_key_part);
    const jsize jkey_part_len = env->GetArrayLength(jba_key_part);
    if (env->EnsureLocalCapacity(jkey_part_len) != 0) {
      // out of memory
      env->DeleteLocalRef(jobj_value_part);
      env->DeleteLocalRef(jobj_key_part);
      free_parts(env, jparts_to_free);
      return;
    }
    jbyte* jkey_part = env->GetByteArrayElements(jba_key_part, nullptr);
    if (jkey_part == nullptr) {
      // exception thrown: OutOfMemoryError
      env->DeleteLocalRef(jobj_value_part);
      env->DeleteLocalRef(jobj_key_part);
      free_parts(env, jparts_to_free);
      return;
    }

    const jbyteArray jba_value_part =
        reinterpret_cast<jbyteArray>(jobj_value_part);
    const jsize jvalue_part_len = env->GetArrayLength(jba_value_part);
    if (env->EnsureLocalCapacity(jvalue_part_len) != 0) {
      // out of memory
      env->DeleteLocalRef(jobj_value_part);
      env->DeleteLocalRef(jobj_key_part);
      env->ReleaseByteArrayElements(jba_key_part, jkey_part, JNI_ABORT);
      free_parts(env, jparts_to_free);
      return;
    }
    jbyte* jvalue_part = env->GetByteArrayElements(jba_value_part, nullptr);
    if (jvalue_part == nullptr) {
      // exception thrown: OutOfMemoryError
      env->ReleaseByteArrayElements(jba_value_part, jvalue_part, JNI_ABORT);
      env->DeleteLocalRef(jobj_value_part);
      env->DeleteLocalRef(jobj_key_part);
      env->ReleaseByteArrayElements(jba_key_part, jkey_part, JNI_ABORT);
      free_parts(env, jparts_to_free);
      return;
    }

    jparts_to_free.push_back(
        std::make_tuple(jba_key_part, jkey_part, jobj_key_part));
    jparts_to_free.push_back(
        std::make_tuple(jba_value_part, jvalue_part, jobj_value_part));

    key_parts.push_back(ROCKSDB_NAMESPACE::Slice(
        reinterpret_cast<char*>(jkey_part), jkey_part_len));
    value_parts.push_back(ROCKSDB_NAMESPACE::Slice(
        reinterpret_cast<char*>(jvalue_part), jvalue_part_len));
  }

  // call the write_multi function
  ROCKSDB_NAMESPACE::Status s = fn_write_kv_parts(
      ROCKSDB_NAMESPACE::SliceParts(key_parts.data(), (int)key_parts.size()),
      ROCKSDB_NAMESPACE::SliceParts(value_parts.data(),
                                    (int)value_parts.size()));

  // cleanup temporary memory
  free_parts(env, jparts_to_free);

  // return
  if (s.ok()) {
    return;
  }

  ROCKSDB_NAMESPACE::RocksDBExceptionJni::ThrowNew(env, s);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    put
 * Signature: (J[[BI[[BIJZ)V
 */
void Java_org_rocksdb_Transaction_put__J_3_3BI_3_3BIJZ(
    JNIEnv* env, jobject /*jobj*/, jlong jhandle, jobjectArray jkey_parts,
    jint jkey_parts_len, jobjectArray jvalue_parts, jint jvalue_parts_len,
    jlong jcolumn_family_handle, jboolean jassume_tracked) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  auto cfh = APIColumnFamilyHandle<ROCKSDB_NAMESPACE::DB>::lock(
      env, jcolumn_family_handle);
  if (!cfh) {
    // CFH exception
    return;
  }
  FnWriteKVParts fn_put_parts =
      std::bind<ROCKSDB_NAMESPACE::Status (ROCKSDB_NAMESPACE::Transaction::*)(
          ROCKSDB_NAMESPACE::ColumnFamilyHandle*,
          const ROCKSDB_NAMESPACE::SliceParts&,
          const ROCKSDB_NAMESPACE::SliceParts&, bool)>(
          &ROCKSDB_NAMESPACE::Transaction::Put, apiTxn.get(), cfh.get(),
          std::placeholders::_1, std::placeholders::_2, jassume_tracked);
  txn_write_kv_parts_helper(env, fn_put_parts, jkey_parts, jkey_parts_len,
                            jvalue_parts, jvalue_parts_len);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    put
 * Signature: (J[[BI[[BI)V
 */
void Java_org_rocksdb_Transaction_put__J_3_3BI_3_3BI(
    JNIEnv* env, jobject /*jobj*/, jlong jhandle, jobjectArray jkey_parts,
    jint jkey_parts_len, jobjectArray jvalue_parts, jint jvalue_parts_len) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  FnWriteKVParts fn_put_parts = std::bind<ROCKSDB_NAMESPACE::Status (
      ROCKSDB_NAMESPACE::Transaction::*)(const ROCKSDB_NAMESPACE::SliceParts&,
                                         const ROCKSDB_NAMESPACE::SliceParts&)>(
      &ROCKSDB_NAMESPACE::Transaction::Put, apiTxn.get(), std::placeholders::_1,
      std::placeholders::_2);
  txn_write_kv_parts_helper(env, fn_put_parts, jkey_parts, jkey_parts_len,
                            jvalue_parts, jvalue_parts_len);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    merge
 * Signature: (J[BI[BIJZ)V
 */
void Java_org_rocksdb_Transaction_merge__J_3BI_3BIJZ(
    JNIEnv* env, jobject /*jobj*/, jlong jhandle, jbyteArray jkey,
    jint jkey_part_len, jbyteArray jval, jint jval_len,
    jlong jcolumn_family_handle, jboolean jassume_tracked) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  auto cfh = APIColumnFamilyHandle<ROCKSDB_NAMESPACE::DB>::lock(
      env, jcolumn_family_handle);
  if (!cfh) {
    // CFH exception
    return;
  }
  FnWriteKV fn_merge =
      std::bind<ROCKSDB_NAMESPACE::Status (ROCKSDB_NAMESPACE::Transaction::*)(
          ROCKSDB_NAMESPACE::ColumnFamilyHandle*,
          const ROCKSDB_NAMESPACE::Slice&, const ROCKSDB_NAMESPACE::Slice&,
          bool)>(&ROCKSDB_NAMESPACE::Transaction::Merge, apiTxn.get(),
                 cfh.get(), std::placeholders::_1, std::placeholders::_2,
                 jassume_tracked);
  txn_write_kv_helper(env, fn_merge, jkey, jkey_part_len, jval, jval_len);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    merge
 * Signature: (J[BI[BI)V
 */
void Java_org_rocksdb_Transaction_merge__J_3BI_3BI(
    JNIEnv* env, jobject /*jobj*/, jlong jhandle, jbyteArray jkey,
    jint jkey_part_len, jbyteArray jval, jint jval_len) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  FnWriteKV fn_merge =
      std::bind<ROCKSDB_NAMESPACE::Status (ROCKSDB_NAMESPACE::Transaction::*)(
          const ROCKSDB_NAMESPACE::Slice&, const ROCKSDB_NAMESPACE::Slice&)>(
          &ROCKSDB_NAMESPACE::Transaction::Merge, apiTxn.get(),
          std::placeholders::_1, std::placeholders::_2);
  txn_write_kv_helper(env, fn_merge, jkey, jkey_part_len, jval, jval_len);
}

typedef std::function<ROCKSDB_NAMESPACE::Status(
    const ROCKSDB_NAMESPACE::Slice&)>
    FnWriteK;

// TODO(AR) consider refactoring to share this between here and rocksjni.cc
void txn_write_k_helper(JNIEnv* env, const FnWriteK& fn_write_k,
                        const jbyteArray& jkey, const jint& jkey_part_len) {
  jbyte* key = env->GetByteArrayElements(jkey, nullptr);
  if (key == nullptr) {
    // exception thrown: OutOfMemoryError
    return;
  }
  ROCKSDB_NAMESPACE::Slice key_slice(reinterpret_cast<char*>(key),
                                     jkey_part_len);

  ROCKSDB_NAMESPACE::Status s = fn_write_k(key_slice);

  // trigger java unref on key.
  // by passing JNI_ABORT, it will simply release the reference without
  // copying the result back to the java byte array.
  env->ReleaseByteArrayElements(jkey, key, JNI_ABORT);

  if (s.ok()) {
    return;
  }
  ROCKSDB_NAMESPACE::RocksDBExceptionJni::ThrowNew(env, s);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    delete
 * Signature: (J[BIJZ)V
 */
void Java_org_rocksdb_Transaction_delete__J_3BIJZ(
    JNIEnv* env, jobject /*jobj*/, jlong jhandle, jbyteArray jkey,
    jint jkey_part_len, jlong jcolumn_family_handle, jboolean jassume_tracked) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  auto cfh = APIColumnFamilyHandle<ROCKSDB_NAMESPACE::DB>::lock(
      env, jcolumn_family_handle);
  if (!cfh) {
    // CFH exception
    return;
  }
  FnWriteK fn_delete =
      std::bind<ROCKSDB_NAMESPACE::Status (ROCKSDB_NAMESPACE::Transaction::*)(
          ROCKSDB_NAMESPACE::ColumnFamilyHandle*,
          const ROCKSDB_NAMESPACE::Slice&, bool)>(
          &ROCKSDB_NAMESPACE::Transaction::Delete, apiTxn.get(), cfh.get(),
          std::placeholders::_1, jassume_tracked);
  txn_write_k_helper(env, fn_delete, jkey, jkey_part_len);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    delete
 * Signature: (J[BI)V
 */
void Java_org_rocksdb_Transaction_delete__J_3BI(JNIEnv* env, jobject /*jobj*/,
                                                jlong jhandle, jbyteArray jkey,
                                                jint jkey_part_len) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  FnWriteK fn_delete = std::bind<ROCKSDB_NAMESPACE::Status (
      ROCKSDB_NAMESPACE::Transaction::*)(const ROCKSDB_NAMESPACE::Slice&)>(
      &ROCKSDB_NAMESPACE::Transaction::Delete, apiTxn.get(),
      std::placeholders::_1);
  txn_write_k_helper(env, fn_delete, jkey, jkey_part_len);
}

typedef std::function<ROCKSDB_NAMESPACE::Status(
    const ROCKSDB_NAMESPACE::SliceParts&)>
    FnWriteKParts;

// TODO(AR) consider refactoring to share this between here and rocksjni.cc
void txn_write_k_parts_helper(JNIEnv* env,
                              const FnWriteKParts& fn_write_k_parts,
                              const jobjectArray& jkey_parts,
                              const jint& jkey_parts_len) {
  std::vector<ROCKSDB_NAMESPACE::Slice> key_parts;
  std::vector<std::tuple<jbyteArray, jbyte*, jobject>> jkey_parts_to_free;

  // convert java key_parts byte[][] to Slice(s)
  for (jint i = 0; i < jkey_parts_len; ++i) {
    const jobject jobj_key_part = env->GetObjectArrayElement(jkey_parts, i);
    if (env->ExceptionCheck()) {
      // exception thrown: ArrayIndexOutOfBoundsException
      free_parts(env, jkey_parts_to_free);
      return;
    }

    const jbyteArray jba_key_part = reinterpret_cast<jbyteArray>(jobj_key_part);
    const jsize jkey_part_len = env->GetArrayLength(jba_key_part);
    if (env->EnsureLocalCapacity(jkey_part_len) != 0) {
      // out of memory
      env->DeleteLocalRef(jobj_key_part);
      free_parts(env, jkey_parts_to_free);
      return;
    }
    jbyte* jkey_part = env->GetByteArrayElements(jba_key_part, nullptr);
    if (jkey_part == nullptr) {
      // exception thrown: OutOfMemoryError
      env->DeleteLocalRef(jobj_key_part);
      free_parts(env, jkey_parts_to_free);
      return;
    }

    jkey_parts_to_free.push_back(std::tuple<jbyteArray, jbyte*, jobject>(
        jba_key_part, jkey_part, jobj_key_part));

    key_parts.push_back(ROCKSDB_NAMESPACE::Slice(
        reinterpret_cast<char*>(jkey_part), jkey_part_len));
  }

  // call the write_multi function
  ROCKSDB_NAMESPACE::Status s = fn_write_k_parts(
      ROCKSDB_NAMESPACE::SliceParts(key_parts.data(), (int)key_parts.size()));

  // cleanup temporary memory
  free_parts(env, jkey_parts_to_free);

  // return
  if (s.ok()) {
    return;
  }
  ROCKSDB_NAMESPACE::RocksDBExceptionJni::ThrowNew(env, s);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    delete
 * Signature: (J[[BIJZ)V
 */
void Java_org_rocksdb_Transaction_delete__J_3_3BIJZ(
    JNIEnv* env, jobject /*jobj*/, jlong jhandle, jobjectArray jkey_parts,
    jint jkey_parts_len, jlong jcolumn_family_handle,
    jboolean jassume_tracked) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  auto cfh = APIColumnFamilyHandle<ROCKSDB_NAMESPACE::DB>::lock(
      env, jcolumn_family_handle);
  if (!cfh) {
    // CFH exception
    return;
  }
  FnWriteKParts fn_delete_parts =
      std::bind<ROCKSDB_NAMESPACE::Status (ROCKSDB_NAMESPACE::Transaction::*)(
          ROCKSDB_NAMESPACE::ColumnFamilyHandle*,
          const ROCKSDB_NAMESPACE::SliceParts&, bool)>(
          &ROCKSDB_NAMESPACE::Transaction::Delete, apiTxn.get(), cfh.get(),
          std::placeholders::_1, jassume_tracked);
  txn_write_k_parts_helper(env, fn_delete_parts, jkey_parts, jkey_parts_len);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    delete
 * Signature: (J[[BI)V
 */
void Java_org_rocksdb_Transaction_delete__J_3_3BI(JNIEnv* env, jobject /*jobj*/,
                                                  jlong jhandle,
                                                  jobjectArray jkey_parts,
                                                  jint jkey_parts_len) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  FnWriteKParts fn_delete_parts = std::bind<ROCKSDB_NAMESPACE::Status (
      ROCKSDB_NAMESPACE::Transaction::*)(const ROCKSDB_NAMESPACE::SliceParts&)>(
      &ROCKSDB_NAMESPACE::Transaction::Delete, apiTxn.get(),
      std::placeholders::_1);
  txn_write_k_parts_helper(env, fn_delete_parts, jkey_parts, jkey_parts_len);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    singleDelete
 * Signature: (J[BIJZ)V
 */
void Java_org_rocksdb_Transaction_singleDelete__J_3BIJZ(
    JNIEnv* env, jobject /*jobj*/, jlong jhandle, jbyteArray jkey,
    jint jkey_part_len, jlong jcolumn_family_handle, jboolean jassume_tracked) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  auto cfh = APIColumnFamilyHandle<ROCKSDB_NAMESPACE::DB>::lock(
      env, jcolumn_family_handle);
  if (!cfh) {
    // CFH exception
    return;
  }
  FnWriteK fn_single_delete =
      std::bind<ROCKSDB_NAMESPACE::Status (ROCKSDB_NAMESPACE::Transaction::*)(
          ROCKSDB_NAMESPACE::ColumnFamilyHandle*,
          const ROCKSDB_NAMESPACE::Slice&, bool)>(
          &ROCKSDB_NAMESPACE::Transaction::SingleDelete, apiTxn.get(),
          cfh.get(), std::placeholders::_1, jassume_tracked);
  txn_write_k_helper(env, fn_single_delete, jkey, jkey_part_len);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    singleDelete
 * Signature: (J[BI)V
 */
void Java_org_rocksdb_Transaction_singleDelete__J_3BI(JNIEnv* env,
                                                      jobject /*jobj*/,
                                                      jlong jhandle,
                                                      jbyteArray jkey,
                                                      jint jkey_part_len) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  FnWriteK fn_single_delete = std::bind<ROCKSDB_NAMESPACE::Status (
      ROCKSDB_NAMESPACE::Transaction::*)(const ROCKSDB_NAMESPACE::Slice&)>(
      &ROCKSDB_NAMESPACE::Transaction::SingleDelete, apiTxn.get(),
      std::placeholders::_1);
  txn_write_k_helper(env, fn_single_delete, jkey, jkey_part_len);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    singleDelete
 * Signature: (J[[BIJZ)V
 */
void Java_org_rocksdb_Transaction_singleDelete__J_3_3BIJZ(
    JNIEnv* env, jobject /*jobj*/, jlong jhandle, jobjectArray jkey_parts,
    jint jkey_parts_len, jlong jcolumn_family_handle,
    jboolean jassume_tracked) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  auto cfh = APIColumnFamilyHandle<ROCKSDB_NAMESPACE::DB>::lock(
      env, jcolumn_family_handle);
  if (!cfh) {
    // CFH exception
    return;
  }
  FnWriteKParts fn_single_delete_parts =
      std::bind<ROCKSDB_NAMESPACE::Status (ROCKSDB_NAMESPACE::Transaction::*)(
          ROCKSDB_NAMESPACE::ColumnFamilyHandle*,
          const ROCKSDB_NAMESPACE::SliceParts&, bool)>(
          &ROCKSDB_NAMESPACE::Transaction::SingleDelete, apiTxn.get(),
          cfh.get(), std::placeholders::_1, jassume_tracked);
  txn_write_k_parts_helper(env, fn_single_delete_parts, jkey_parts,
                           jkey_parts_len);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    singleDelete
 * Signature: (J[[BI)V
 */
void Java_org_rocksdb_Transaction_singleDelete__J_3_3BI(JNIEnv* env,
                                                        jobject /*jobj*/,
                                                        jlong jhandle,
                                                        jobjectArray jkey_parts,
                                                        jint jkey_parts_len) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  FnWriteKParts fn_single_delete_parts = std::bind<ROCKSDB_NAMESPACE::Status (
      ROCKSDB_NAMESPACE::Transaction::*)(const ROCKSDB_NAMESPACE::SliceParts&)>(
      &ROCKSDB_NAMESPACE::Transaction::SingleDelete, apiTxn.get(),
      std::placeholders::_1);
  txn_write_k_parts_helper(env, fn_single_delete_parts, jkey_parts,
                           jkey_parts_len);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    putUntracked
 * Signature: (J[BI[BIJ)V
 */
void Java_org_rocksdb_Transaction_putUntracked__J_3BI_3BIJ(
    JNIEnv* env, jobject /*jobj*/, jlong jhandle, jbyteArray jkey,
    jint jkey_part_len, jbyteArray jval, jint jval_len,
    jlong jcolumn_family_handle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  auto cfh = APIColumnFamilyHandle<ROCKSDB_NAMESPACE::DB>::lock(
      env, jcolumn_family_handle);
  if (!cfh) {
    // CFH exception
    return;
  }
  FnWriteKV fn_put_untracked =
      std::bind<ROCKSDB_NAMESPACE::Status (ROCKSDB_NAMESPACE::Transaction::*)(
          ROCKSDB_NAMESPACE::ColumnFamilyHandle*,
          const ROCKSDB_NAMESPACE::Slice&, const ROCKSDB_NAMESPACE::Slice&)>(
          &ROCKSDB_NAMESPACE::Transaction::PutUntracked, apiTxn.get(),
          cfh.get(), std::placeholders::_1, std::placeholders::_2);
  txn_write_kv_helper(env, fn_put_untracked, jkey, jkey_part_len, jval,
                      jval_len);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    putUntracked
 * Signature: (J[BI[BI)V
 */
void Java_org_rocksdb_Transaction_putUntracked__J_3BI_3BI(
    JNIEnv* env, jobject /*jobj*/, jlong jhandle, jbyteArray jkey,
    jint jkey_part_len, jbyteArray jval, jint jval_len) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  FnWriteKV fn_put_untracked =
      std::bind<ROCKSDB_NAMESPACE::Status (ROCKSDB_NAMESPACE::Transaction::*)(
          const ROCKSDB_NAMESPACE::Slice&, const ROCKSDB_NAMESPACE::Slice&)>(
          &ROCKSDB_NAMESPACE::Transaction::PutUntracked, apiTxn.get(),
          std::placeholders::_1, std::placeholders::_2);
  txn_write_kv_helper(env, fn_put_untracked, jkey, jkey_part_len, jval,
                      jval_len);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    putUntracked
 * Signature: (J[[BI[[BIJ)V
 */
void Java_org_rocksdb_Transaction_putUntracked__J_3_3BI_3_3BIJ(
    JNIEnv* env, jobject /*jobj*/, jlong jhandle, jobjectArray jkey_parts,
    jint jkey_parts_len, jobjectArray jvalue_parts, jint jvalue_parts_len,
    jlong jcolumn_family_handle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  auto cfh = APIColumnFamilyHandle<ROCKSDB_NAMESPACE::DB>::lock(
      env, jcolumn_family_handle);
  if (!cfh) {
    // CFH exception
    return;
  }
  FnWriteKVParts fn_put_parts_untracked = std::bind<ROCKSDB_NAMESPACE::Status (
      ROCKSDB_NAMESPACE::Transaction::*)(ROCKSDB_NAMESPACE::ColumnFamilyHandle*,
                                         const ROCKSDB_NAMESPACE::SliceParts&,
                                         const ROCKSDB_NAMESPACE::SliceParts&)>(
      &ROCKSDB_NAMESPACE::Transaction::PutUntracked, apiTxn.get(), cfh.get(),
      std::placeholders::_1, std::placeholders::_2);
  txn_write_kv_parts_helper(env, fn_put_parts_untracked, jkey_parts,
                            jkey_parts_len, jvalue_parts, jvalue_parts_len);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    putUntracked
 * Signature: (J[[BI[[BI)V
 */
void Java_org_rocksdb_Transaction_putUntracked__J_3_3BI_3_3BI(
    JNIEnv* env, jobject /*jobj*/, jlong jhandle, jobjectArray jkey_parts,
    jint jkey_parts_len, jobjectArray jvalue_parts, jint jvalue_parts_len) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  FnWriteKVParts fn_put_parts_untracked = std::bind<ROCKSDB_NAMESPACE::Status (
      ROCKSDB_NAMESPACE::Transaction::*)(const ROCKSDB_NAMESPACE::SliceParts&,
                                         const ROCKSDB_NAMESPACE::SliceParts&)>(
      &ROCKSDB_NAMESPACE::Transaction::PutUntracked, apiTxn.get(),
      std::placeholders::_1, std::placeholders::_2);
  txn_write_kv_parts_helper(env, fn_put_parts_untracked, jkey_parts,
                            jkey_parts_len, jvalue_parts, jvalue_parts_len);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    mergeUntracked
 * Signature: (J[BI[BIJ)V
 */
void Java_org_rocksdb_Transaction_mergeUntracked__J_3BI_3BIJ(
    JNIEnv* env, jobject /*jobj*/, jlong jhandle, jbyteArray jkey,
    jint jkey_part_len, jbyteArray jval, jint jval_len,
    jlong jcolumn_family_handle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  auto cfh = APIColumnFamilyHandle<ROCKSDB_NAMESPACE::DB>::lock(
      env, jcolumn_family_handle);
  if (!cfh) {
    // CFH exception
    return;
  }
  FnWriteKV fn_merge_untracked =
      std::bind<ROCKSDB_NAMESPACE::Status (ROCKSDB_NAMESPACE::Transaction::*)(
          ROCKSDB_NAMESPACE::ColumnFamilyHandle*,
          const ROCKSDB_NAMESPACE::Slice&, const ROCKSDB_NAMESPACE::Slice&)>(
          &ROCKSDB_NAMESPACE::Transaction::MergeUntracked, apiTxn.get(),
          cfh.get(), std::placeholders::_1, std::placeholders::_2);
  txn_write_kv_helper(env, fn_merge_untracked, jkey, jkey_part_len, jval,
                      jval_len);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    mergeUntracked
 * Signature: (J[BI[BI)V
 */
void Java_org_rocksdb_Transaction_mergeUntracked__J_3BI_3BI(
    JNIEnv* env, jobject /*jobj*/, jlong jhandle, jbyteArray jkey,
    jint jkey_part_len, jbyteArray jval, jint jval_len) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  FnWriteKV fn_merge_untracked =
      std::bind<ROCKSDB_NAMESPACE::Status (ROCKSDB_NAMESPACE::Transaction::*)(
          const ROCKSDB_NAMESPACE::Slice&, const ROCKSDB_NAMESPACE::Slice&)>(
          &ROCKSDB_NAMESPACE::Transaction::MergeUntracked, apiTxn.get(),
          std::placeholders::_1, std::placeholders::_2);
  txn_write_kv_helper(env, fn_merge_untracked, jkey, jkey_part_len, jval,
                      jval_len);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    deleteUntracked
 * Signature: (J[BIJ)V
 */
void Java_org_rocksdb_Transaction_deleteUntracked__J_3BIJ(
    JNIEnv* env, jobject /*jobj*/, jlong jhandle, jbyteArray jkey,
    jint jkey_part_len, jlong jcolumn_family_handle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  auto cfh = APIColumnFamilyHandle<ROCKSDB_NAMESPACE::DB>::lock(
      env, jcolumn_family_handle);
  if (!cfh) {
    // CFH exception
    return;
  }
  FnWriteK fn_delete_untracked = std::bind<ROCKSDB_NAMESPACE::Status (
      ROCKSDB_NAMESPACE::Transaction::*)(ROCKSDB_NAMESPACE::ColumnFamilyHandle*,
                                         const ROCKSDB_NAMESPACE::Slice&)>(
      &ROCKSDB_NAMESPACE::Transaction::DeleteUntracked, apiTxn.get(), cfh.get(),
      std::placeholders::_1);
  txn_write_k_helper(env, fn_delete_untracked, jkey, jkey_part_len);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    deleteUntracked
 * Signature: (J[BI)V
 */
void Java_org_rocksdb_Transaction_deleteUntracked__J_3BI(JNIEnv* env,
                                                         jobject /*jobj*/,
                                                         jlong jhandle,
                                                         jbyteArray jkey,
                                                         jint jkey_part_len) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  FnWriteK fn_delete_untracked = std::bind<ROCKSDB_NAMESPACE::Status (
      ROCKSDB_NAMESPACE::Transaction::*)(const ROCKSDB_NAMESPACE::Slice&)>(
      &ROCKSDB_NAMESPACE::Transaction::DeleteUntracked, apiTxn.get(),
      std::placeholders::_1);
  txn_write_k_helper(env, fn_delete_untracked, jkey, jkey_part_len);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    deleteUntracked
 * Signature: (J[[BIJ)V
 */
void Java_org_rocksdb_Transaction_deleteUntracked__J_3_3BIJ(
    JNIEnv* env, jobject /*jobj*/, jlong jhandle, jobjectArray jkey_parts,
    jint jkey_parts_len, jlong jcolumn_family_handle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  auto cfh = APIColumnFamilyHandle<ROCKSDB_NAMESPACE::DB>::lock(
      env, jcolumn_family_handle);
  if (!cfh) {
    // CFH exception
    return;
  }
  FnWriteKParts fn_delete_untracked_parts =
      std::bind<ROCKSDB_NAMESPACE::Status (ROCKSDB_NAMESPACE::Transaction::*)(
          ROCKSDB_NAMESPACE::ColumnFamilyHandle*,
          const ROCKSDB_NAMESPACE::SliceParts&)>(
          &ROCKSDB_NAMESPACE::Transaction::DeleteUntracked, apiTxn.get(),
          cfh.get(), std::placeholders::_1);
  txn_write_k_parts_helper(env, fn_delete_untracked_parts, jkey_parts,
                           jkey_parts_len);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    deleteUntracked
 * Signature: (J[[BI)V
 */
void Java_org_rocksdb_Transaction_deleteUntracked__J_3_3BI(
    JNIEnv* env, jobject /*jobj*/, jlong jhandle, jobjectArray jkey_parts,
    jint jkey_parts_len) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  FnWriteKParts fn_delete_untracked_parts =
      std::bind<ROCKSDB_NAMESPACE::Status (ROCKSDB_NAMESPACE::Transaction::*)(
          const ROCKSDB_NAMESPACE::SliceParts&)>(
          &ROCKSDB_NAMESPACE::Transaction::DeleteUntracked, apiTxn.get(),
          std::placeholders::_1);
  txn_write_k_parts_helper(env, fn_delete_untracked_parts, jkey_parts,
                           jkey_parts_len);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    putLogData
 * Signature: (J[BI)V
 */
void Java_org_rocksdb_Transaction_putLogData(JNIEnv* env, jobject /*jobj*/,
                                             jlong jhandle, jbyteArray jkey,
                                             jint jkey_part_len) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);

  jbyte* key = env->GetByteArrayElements(jkey, nullptr);
  if (key == nullptr) {
    // exception thrown: OutOfMemoryError
    return;
  }

  ROCKSDB_NAMESPACE::Slice key_slice(reinterpret_cast<char*>(key),
                                     jkey_part_len);
  apiTxn->PutLogData(key_slice);

  // trigger java unref on key.
  // by passing JNI_ABORT, it will simply release the reference without
  // copying the result back to the java byte array.
  env->ReleaseByteArrayElements(jkey, key, JNI_ABORT);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    disableIndexing
 * Signature: (J)V
 */
void Java_org_rocksdb_Transaction_disableIndexing(JNIEnv* /*env*/,
                                                  jobject /*jobj*/,
                                                  jlong jhandle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  apiTxn->DisableIndexing();
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    enableIndexing
 * Signature: (J)V
 */
void Java_org_rocksdb_Transaction_enableIndexing(JNIEnv* /*env*/,
                                                 jobject /*jobj*/,
                                                 jlong jhandle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  apiTxn->EnableIndexing();
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    getNumKeys
 * Signature: (J)J
 */
jlong Java_org_rocksdb_Transaction_getNumKeys(JNIEnv* /*env*/, jobject /*jobj*/,
                                              jlong jhandle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  return apiTxn->GetNumKeys();
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    getNumPuts
 * Signature: (J)J
 */
jlong Java_org_rocksdb_Transaction_getNumPuts(JNIEnv* /*env*/, jobject /*jobj*/,
                                              jlong jhandle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  return apiTxn->GetNumPuts();
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    getNumDeletes
 * Signature: (J)J
 */
jlong Java_org_rocksdb_Transaction_getNumDeletes(JNIEnv* /*env*/,
                                                 jobject /*jobj*/,
                                                 jlong jhandle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  return apiTxn->GetNumDeletes();
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    getNumMerges
 * Signature: (J)J
 */
jlong Java_org_rocksdb_Transaction_getNumMerges(JNIEnv* /*env*/,
                                                jobject /*jobj*/,
                                                jlong jhandle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  return apiTxn->GetNumMerges();
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    getElapsedTime
 * Signature: (J)J
 */
jlong Java_org_rocksdb_Transaction_getElapsedTime(JNIEnv* /*env*/,
                                                  jobject /*jobj*/,
                                                  jlong jhandle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  return apiTxn->GetElapsedTime();
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    getWriteBatch
 * Signature: (J)J
 */
jlong Java_org_rocksdb_Transaction_getWriteBatch(JNIEnv* /*env*/,
                                                 jobject /*jobj*/,
                                                 jlong jhandle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  return GET_CPLUSPLUS_POINTER(apiTxn->GetWriteBatch());
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    setLockTimeout
 * Signature: (JJ)V
 */
void Java_org_rocksdb_Transaction_setLockTimeout(JNIEnv* /*env*/,
                                                 jobject /*jobj*/,
                                                 jlong jhandle,
                                                 jlong jlock_timeout) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  apiTxn->SetLockTimeout(jlock_timeout);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    getWriteOptions
 * Signature: (J)J
 */
jlong Java_org_rocksdb_Transaction_getWriteOptions(JNIEnv* /*env*/,
                                                   jobject /*jobj*/,
                                                   jlong jhandle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  return GET_CPLUSPLUS_POINTER(apiTxn->GetWriteOptions());
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    setWriteOptions
 * Signature: (JJ)V
 */
void Java_org_rocksdb_Transaction_setWriteOptions(JNIEnv* /*env*/,
                                                  jobject /*jobj*/,
                                                  jlong jhandle,
                                                  jlong jwrite_options_handle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  auto* write_options =
      reinterpret_cast<ROCKSDB_NAMESPACE::WriteOptions*>(jwrite_options_handle);
  apiTxn->SetWriteOptions(*write_options);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    undo
 * Signature: (J[BIJ)V
 */
void Java_org_rocksdb_Transaction_undoGetForUpdate__J_3BIJ(
    JNIEnv* env, jobject /*jobj*/, jlong jhandle, jbyteArray jkey,
    jint jkey_part_len, jlong jcolumn_family_handle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  auto cfh = APIColumnFamilyHandle<ROCKSDB_NAMESPACE::DB>::lock(
      env, jcolumn_family_handle);
  if (!cfh) {
    // CFH exception
    return;
  }
  jbyte* key = env->GetByteArrayElements(jkey, nullptr);
  if (key == nullptr) {
    // exception thrown: OutOfMemoryError
    return;
  }

  ROCKSDB_NAMESPACE::Slice key_slice(reinterpret_cast<char*>(key),
                                     jkey_part_len);
  apiTxn->UndoGetForUpdate(cfh.get(), key_slice);

  env->ReleaseByteArrayElements(jkey, key, JNI_ABORT);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    undoGetForUpdate
 * Signature: (J[BI)V
 */
void Java_org_rocksdb_Transaction_undoGetForUpdate__J_3BI(JNIEnv* env,
                                                          jobject /*jobj*/,
                                                          jlong jhandle,
                                                          jbyteArray jkey,
                                                          jint jkey_part_len) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  jbyte* key = env->GetByteArrayElements(jkey, nullptr);
  if (key == nullptr) {
    // exception thrown: OutOfMemoryError
    return;
  }

  ROCKSDB_NAMESPACE::Slice key_slice(reinterpret_cast<char*>(key),
                                     jkey_part_len);
  apiTxn->UndoGetForUpdate(key_slice);

  env->ReleaseByteArrayElements(jkey, key, JNI_ABORT);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    rebuildFromWriteBatch
 * Signature: (JJ)V
 */
void Java_org_rocksdb_Transaction_rebuildFromWriteBatch(
    JNIEnv* env, jobject /*jobj*/, jlong jhandle, jlong jwrite_batch_handle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  auto* write_batch =
      reinterpret_cast<ROCKSDB_NAMESPACE::WriteBatch*>(jwrite_batch_handle);
  ROCKSDB_NAMESPACE::Status s = apiTxn->RebuildFromWriteBatch(write_batch);
  if (!s.ok()) {
    ROCKSDB_NAMESPACE::RocksDBExceptionJni::ThrowNew(env, s);
  }
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    getCommitTimeWriteBatch
 * Signature: (J)J
 */
jlong Java_org_rocksdb_Transaction_getCommitTimeWriteBatch(JNIEnv* /*env*/,
                                                           jobject /*jobj*/,
                                                           jlong jhandle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  return GET_CPLUSPLUS_POINTER(apiTxn->GetCommitTimeWriteBatch());
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    setLogNumber
 * Signature: (JJ)V
 */
void Java_org_rocksdb_Transaction_setLogNumber(JNIEnv* /*env*/,
                                               jobject /*jobj*/, jlong jhandle,
                                               jlong jlog_number) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  apiTxn->SetLogNumber(jlog_number);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    getLogNumber
 * Signature: (J)J
 */
jlong Java_org_rocksdb_Transaction_getLogNumber(JNIEnv* /*env*/,
                                                jobject /*jobj*/,
                                                jlong jhandle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  return apiTxn->GetLogNumber();
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    setName
 * Signature: (JLjava/lang/String;)V
 */
void Java_org_rocksdb_Transaction_setName(JNIEnv* env, jobject /*jobj*/,
                                          jlong jhandle, jstring jname) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  const char* name = env->GetStringUTFChars(jname, nullptr);
  if (name == nullptr) {
    // exception thrown: OutOfMemoryError
    return;
  }

  ROCKSDB_NAMESPACE::Status s = apiTxn->SetName(name);

  env->ReleaseStringUTFChars(jname, name);

  if (!s.ok()) {
    ROCKSDB_NAMESPACE::RocksDBExceptionJni::ThrowNew(env, s);
  }
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    getName
 * Signature: (J)Ljava/lang/String;
 */
jstring Java_org_rocksdb_Transaction_getName(JNIEnv* env, jobject /*jobj*/,
                                             jlong jhandle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  ROCKSDB_NAMESPACE::TransactionName name = apiTxn->GetName();
  return env->NewStringUTF(name.data());
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    getID
 * Signature: (J)J
 */
jlong Java_org_rocksdb_Transaction_getID(JNIEnv* /*env*/, jobject /*jobj*/,
                                         jlong jhandle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  ROCKSDB_NAMESPACE::TransactionID id = apiTxn->GetID();
  return static_cast<jlong>(id);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    isDeadlockDetect
 * Signature: (J)Z
 */
jboolean Java_org_rocksdb_Transaction_isDeadlockDetect(JNIEnv* /*env*/,
                                                       jobject /*jobj*/,
                                                       jlong jhandle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  return static_cast<jboolean>(apiTxn->IsDeadlockDetect());
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    getWaitingTxns
 * Signature: (J)Lorg/rocksdb/Transaction/WaitingTransactions;
 */
jobject Java_org_rocksdb_Transaction_getWaitingTxns(JNIEnv* env,
                                                    jobject jtransaction_obj,
                                                    jlong jhandle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  uint32_t column_family_id;
  std::string key;
  std::vector<ROCKSDB_NAMESPACE::TransactionID> waiting_txns =
      apiTxn->GetWaitingTxns(&column_family_id, &key);
  jobject jwaiting_txns =
      ROCKSDB_NAMESPACE::TransactionJni::newWaitingTransactions(
          env, jtransaction_obj, column_family_id, key, waiting_txns);
  return jwaiting_txns;
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    getState
 * Signature: (J)B
 */
jbyte Java_org_rocksdb_Transaction_getState(JNIEnv* /*env*/, jobject /*jobj*/,
                                            jlong jhandle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  ROCKSDB_NAMESPACE::Transaction::TransactionState txn_status =
      apiTxn->GetState();
  switch (txn_status) {
    case ROCKSDB_NAMESPACE::Transaction::TransactionState::STARTED:
      return 0x0;

    case ROCKSDB_NAMESPACE::Transaction::TransactionState::AWAITING_PREPARE:
      return 0x1;

    case ROCKSDB_NAMESPACE::Transaction::TransactionState::PREPARED:
      return 0x2;

    case ROCKSDB_NAMESPACE::Transaction::TransactionState::AWAITING_COMMIT:
      return 0x3;

    case ROCKSDB_NAMESPACE::Transaction::TransactionState::COMMITTED:
      return 0x4;

    case ROCKSDB_NAMESPACE::Transaction::TransactionState::AWAITING_ROLLBACK:
      return 0x5;

    case ROCKSDB_NAMESPACE::Transaction::TransactionState::ROLLEDBACK:
      return 0x6;

    case ROCKSDB_NAMESPACE::Transaction::TransactionState::LOCKS_STOLEN:
      return 0x7;
  }

  assert(false);
  return static_cast<jbyte>(-1);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    getId
 * Signature: (J)J
 */
jlong Java_org_rocksdb_Transaction_getId(JNIEnv* /*env*/, jobject /*jobj*/,
                                         jlong jhandle) {
  auto& apiTxn = *reinterpret_cast<API_TXN*>(jhandle);
  uint64_t id = apiTxn->GetId();
  return static_cast<jlong>(id);
}

/*
 * Class:     org_rocksdb_Transaction
 * Method:    disposeInternal
 * Signature: (J)V
 */
void Java_org_rocksdb_Transaction_nativeClose(JNIEnv* /*env*/, jobject /*jobj*/,
                                              jlong jhandle) {
  std::unique_ptr<API_TXN> txnAPI(reinterpret_cast<API_TXN*>(jhandle));
  txnAPI->check("nativeClose()");
  // Now the unique_ptr destructor will delete() referenced shared_ptr contents
  // in the API object.
}
