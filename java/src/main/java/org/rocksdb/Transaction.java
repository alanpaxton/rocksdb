// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

package org.rocksdb;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import org.rocksdb.RocksNative;

/**
 * Provides BEGIN/COMMIT/ROLLBACK transactions.
 *
 * To use transactions, you must first create either an
 * {@link OptimisticTransactionDB} or a {@link TransactionDB}
 *
 * To create a transaction, use
 * {@link OptimisticTransactionDB#beginTransaction(org.rocksdb.WriteOptions)} or
 * {@link TransactionDB#beginTransaction(org.rocksdb.WriteOptions)}
 *
 * It is up to the caller to synchronize access to this object.
 *
 * See samples/src/main/java/OptimisticTransactionSample.java and
 * samples/src/main/java/TransactionSample.java for some simple
 * examples.
 */
public class Transaction extends RocksNative {
  private final RocksDB parent;

  /**
   * Intentionally package private
   * as this is called from
   * {@link OptimisticTransactionDB#beginTransaction(org.rocksdb.WriteOptions)}
   * or {@link TransactionDB#beginTransaction(org.rocksdb.WriteOptions)}
   *
   * @param parent This must be either {@link TransactionDB} or
   *     {@link OptimisticTransactionDB}
   * @param transactionHandle The native handle to the underlying C++
   *     transaction object
   */
  Transaction(final RocksDB parent, final long transactionHandle) {
    super(transactionHandle);
    this.parent = parent;
  }

  /**
   * If a transaction has a snapshot set, the transaction will ensure that
   * any keys successfully written (or fetched via {@link #getForUpdate}) have
   * not been modified outside of this transaction since the time the snapshot
   * was set.
   *
   * If a snapshot has not been set, the transaction guarantees that keys have
   * not been modified since the time each key was first written (or fetched via
   * {@link #getForUpdate}).
   *
   * Using {@link #setSnapshot()} will provide stricter isolation guarantees
   * at the expense of potentially more transaction failures due to conflicts
   * with other writes.
   *
   * Calling {@link #setSnapshot()} has no effect on keys written before this
   * function has been called.
   *
   * {@link #setSnapshot()} may be called multiple times if you would like to
   * change the snapshot used for different operations in this transaction.
   *
   * Calling {@link #setSnapshot()} will not affect the version of Data returned
   * by get(...) methods. See {@link #get} for more details.
   */
  public void setSnapshot() {
    setSnapshot(getNative());
  }

  /**
   * Similar to {@link #setSnapshot()}, but will not change the current snapshot
   * until put/merge/delete/getForUpdate/multiGetForUpdate is called.
   * By calling this function, the transaction will essentially call
   * {@link #setSnapshot()} for you right before performing the next
   * write/getForUpdate.
   *
   * Calling {@link #setSnapshotOnNextOperation()} will not affect what
   * snapshot is returned by {@link #getSnapshot} until the next
   * write/getForUpdate is executed.
   *
   * When the snapshot is created the notifier's snapshotCreated method will
   * be called so that the caller can get access to the snapshot.
   *
   * This is an optimization to reduce the likelihood of conflicts that
   * could occur in between the time {@link #setSnapshot()} is called and the
   * first write/getForUpdate operation. i.e. this prevents the following
   * race-condition:
   *
   *   txn1-&gt;setSnapshot();
   *                             txn2-&gt;put("A", ...);
   *                             txn2-&gt;commit();
   *   txn1-&gt;getForUpdate(opts, "A", ...);  * FAIL!
   */
  public void setSnapshotOnNextOperation() {
    setSnapshotOnNextOperation(getNative());
  }

  /**
   * Similar to {@link #setSnapshot()}, but will not change the current snapshot
   * until put/merge/delete/getForUpdate/multiGetForUpdate is called.
   * By calling this function, the transaction will essentially call
   * {@link #setSnapshot()} for you right before performing the next
   * write/getForUpdate.
   *
   * Calling {@link #setSnapshotOnNextOperation()} will not affect what
   * snapshot is returned by {@link #getSnapshot} until the next
   * write/getForUpdate is executed.
   *
   * When the snapshot is created the
   * {@link AbstractTransactionNotifier#snapshotCreated(Snapshot)} method will
   * be called so that the caller can get access to the snapshot.
   *
   * This is an optimization to reduce the likelihood of conflicts that
   * could occur in between the time {@link #setSnapshot()} is called and the
   * first write/getForUpdate operation. i.e. this prevents the following
   * race-condition:
   *
   *   txn1-&gt;setSnapshot();
   *                             txn2-&gt;put("A", ...);
   *                             txn2-&gt;commit();
   *   txn1-&gt;getForUpdate(opts, "A", ...);  * FAIL!
   *
   * @param transactionNotifier A handler for receiving snapshot notifications
   *     for the transaction
   *
   */
  public void setSnapshotOnNextOperation(
      final AbstractTransactionNotifier transactionNotifier) {
    setSnapshotOnNextOperation(getNative(), transactionNotifier.nativeHandle_);
  }

 /**
  * Returns the Snapshot created by the last call to {@link #setSnapshot()}.
  *
  * REQUIRED: The returned Snapshot is only valid up until the next time
  * {@link #setSnapshot()}/{@link #setSnapshotOnNextOperation()} is called,
  * {@link #clearSnapshot()} is called, or the Transaction is deleted.
  *
  * @return The snapshot or null if there is no snapshot
  */
  public Snapshot getSnapshot() {
    final long snapshotNativeHandle = getSnapshot(getNative());
    if(snapshotNativeHandle == 0) {
      return null;
    } else {
      final Snapshot snapshot = new Snapshot(snapshotNativeHandle);
      return snapshot;
    }
  }

  /**
   * Clears the current snapshot (i.e. no snapshot will be 'set')
   *
   * This removes any snapshot that currently exists or is set to be created
   * on the next update operation ({@link #setSnapshotOnNextOperation()}).
   *
   * Calling {@link #clearSnapshot()} has no effect on keys written before this
   * function has been called.
   *
   * If a reference to a snapshot was retrieved via {@link #getSnapshot()}, it
   * will no longer be valid and should be discarded after a call to
   * {@link #clearSnapshot()}.
   */
  public void clearSnapshot() {
    clearSnapshot(getNative());
  }

  /**
   * Prepare the current transaction for 2PC
   */
  void prepare() throws RocksDBException {
    //TODO(AR) consider a Java'ish version of this function, which returns an AutoCloseable (commit)
    prepare(getNative());
  }

  /**
   * Write all batched keys to the db atomically.
   *
   * Returns OK on success.
   *
   * May return any error status that could be returned by DB:Write().
   *
   * If this transaction was created by an {@link OptimisticTransactionDB}
   * Status::Busy() may be returned if the transaction could not guarantee
   * that there are no write conflicts. Status::TryAgain() may be returned
   * if the memtable history size is not large enough
   *  (See max_write_buffer_number_to_maintain).
   *
   * If this transaction was created by a {@link TransactionDB},
   * Status::Expired() may be returned if this transaction has lived for
   * longer than {@link TransactionOptions#getExpiration()}.
   *
   * @throws RocksDBException if an error occurs when committing the transaction
   */
  public void commit() throws RocksDBException {
    commit(getNative());
  }

  /**
   * Discard all batched writes in this transaction.
   *
   * @throws RocksDBException if an error occurs when rolling back the transaction
   */
  public void rollback() throws RocksDBException {
    rollback(getNative());
  }

  /**
   * Records the state of the transaction for future calls to
   * {@link #rollbackToSavePoint()}.
   *
   * May be called multiple times to set multiple save points.
   *
   * @throws RocksDBException if an error occurs whilst setting a save point
   */
  public void setSavePoint() throws RocksDBException {
    setSavePoint(getNative());
  }

  /**
   * Undo all operations in this transaction (put, merge, delete, putLogData)
   * since the most recent call to {@link #setSavePoint()} and removes the most
   * recent {@link #setSavePoint()}.
   *
   * If there is no previous call to {@link #setSavePoint()},
   * returns Status::NotFound()
   *
   * @throws RocksDBException if an error occurs when rolling back to a save point
   */
  public void rollbackToSavePoint() throws RocksDBException {
    rollbackToSavePoint(getNative());
  }

  /**
   * This function is similar to
   * {@link RocksDB#get(ColumnFamilyHandle, ReadOptions, byte[])} except it will
   * also read pending changes in this transaction.
   * Currently, this function will return Status::MergeInProgress if the most
   * recent write to the queried key in this batch is a Merge.
   *
   * If {@link ReadOptions#snapshot()} is not set, the current version of the
   * key will be read. Calling {@link #setSnapshot()} does not affect the
   * version of the data returned.
   *
   * Note that setting {@link ReadOptions#setSnapshot(Snapshot)} will affect
   * what is read from the DB but will NOT change which keys are read from this
   * transaction (the keys in this transaction do not yet belong to any snapshot
   * and will be fetched regardless).
   *
   * @param columnFamilyHandle {@link org.rocksdb.ColumnFamilyHandle} instance
   * @param readOptions Read options.
   * @param key the key to retrieve the value for.
   *
   * @return a byte array storing the value associated with the input key if
   *     any. null if it does not find the specified key.
   *
   * @throws RocksDBException thrown if error happens in underlying native
   *     library.
   */
  public byte[] get(final ColumnFamilyHandle columnFamilyHandle, final ReadOptions readOptions,
      final byte[] key) throws RocksDBException {
    return get(
        getNative(), readOptions.nativeHandle_, key, key.length, columnFamilyHandle.getNative());
  }

  /**
   * This function is similar to
   * {@link RocksDB#get(ReadOptions, byte[])} except it will
   * also read pending changes in this transaction.
   * Currently, this function will return Status::MergeInProgress if the most
   * recent write to the queried key in this batch is a Merge.
   *
   * If {@link ReadOptions#snapshot()} is not set, the current version of the
   * key will be read. Calling {@link #setSnapshot()} does not affect the
   * version of the data returned.
   *
   * Note that setting {@link ReadOptions#setSnapshot(Snapshot)} will affect
   * what is read from the DB but will NOT change which keys are read from this
   * transaction (the keys in this transaction do not yet belong to any snapshot
   * and will be fetched regardless).
   *
   * @param readOptions Read options.
   * @param key the key to retrieve the value for.
   *
   * @return a byte array storing the value associated with the input key if
   *     any. null if it does not find the specified key.
   *
   * @throws RocksDBException thrown if error happens in underlying native
   *     library.
   */
  public byte[] get(final ReadOptions readOptions, final byte[] key)
      throws RocksDBException {
    return get(getNative(), readOptions.nativeHandle_, key, key.length);
  }

  /**
   * This function is similar to
   * {@link RocksDB#multiGetAsList} except it will
   * also read pending changes in this transaction.
   * Currently, this function will return Status::MergeInProgress if the most
   * recent write to the queried key in this batch is a Merge.
   *
   * If {@link ReadOptions#snapshot()} is not set, the current version of the
   * key will be read. Calling {@link #setSnapshot()} does not affect the
   * version of the data returned.
   *
   * Note that setting {@link ReadOptions#setSnapshot(Snapshot)} will affect
   * what is read from the DB but will NOT change which keys are read from this
   * transaction (the keys in this transaction do not yet belong to any snapshot
   * and will be fetched regardless).
   *
   * @param readOptions Read options.
   * @param columnFamilyHandles {@link java.util.List} containing
   *     {@link org.rocksdb.ColumnFamilyHandle} instances.
   * @param keys of keys for which values need to be retrieved.
   *
   * @return Array of values, one for each key
   *
   * @throws RocksDBException thrown if error happens in underlying
   *    native library.
   * @throws IllegalArgumentException thrown if the size of passed keys is not
   *    equal to the amount of passed column family handles.
   */
  @Deprecated
  public byte[][] multiGet(final ReadOptions readOptions,
      final List<ColumnFamilyHandle> columnFamilyHandles, final byte[][] keys)
      throws RocksDBException {
    // Check if key size equals cfList size. If not a exception must be
    // thrown. If not a Segmentation fault happens.
    if (keys.length != columnFamilyHandles.size()) {
      throw new IllegalArgumentException(
          "For each key there must be a ColumnFamilyHandle.");
    }
    if(keys.length == 0) {
      return new byte[0][0];
    }
    final long[] cfHandles = new long[columnFamilyHandles.size()];
    for (int i = 0; i < columnFamilyHandles.size(); i++) {
      cfHandles[i] = columnFamilyHandles.get(i).getNative();
    }

    return multiGet(getNative(), readOptions.nativeHandle_, keys, cfHandles);
  }

  /**
   * This function is similar to
   * {@link RocksDB#multiGetAsList(ReadOptions, List, List)} except it will
   * also read pending changes in this transaction.
   * Currently, this function will return Status::MergeInProgress if the most
   * recent write to the queried key in this batch is a Merge.
   *
   * If {@link ReadOptions#snapshot()} is not set, the current version of the
   * key will be read. Calling {@link #setSnapshot()} does not affect the
   * version of the data returned.
   *
   * Note that setting {@link ReadOptions#setSnapshot(Snapshot)} will affect
   * what is read from the DB but will NOT change which keys are read from this
   * transaction (the keys in this transaction do not yet belong to any snapshot
   * and will be fetched regardless).
   *
   * @param readOptions Read options.
   * @param columnFamilyHandles {@link java.util.List} containing
   *     {@link org.rocksdb.ColumnFamilyHandle} instances.
   * @param keys of keys for which values need to be retrieved.
   *
   * @return Array of values, one for each key
   *
   * @throws RocksDBException thrown if error happens in underlying
   *    native library.
   * @throws IllegalArgumentException thrown if the size of passed keys is not
   *    equal to the amount of passed column family handles.
   */

  public List<byte[]> multiGetAsList(final ReadOptions readOptions,
      final List<ColumnFamilyHandle> columnFamilyHandles, final List<byte[]> keys)
      throws RocksDBException {
    // Check if key size equals cfList size. If not a exception must be
    // thrown. If not a Segmentation fault happens.
    if (keys.size() != columnFamilyHandles.size()) {
      throw new IllegalArgumentException("For each key there must be a ColumnFamilyHandle.");
    }
    if (keys.size() == 0) {
      return new ArrayList<>(0);
    }
    final byte[][] keysArray = keys.toArray(new byte[keys.size()][]);
    final long[] cfHandles = new long[columnFamilyHandles.size()];
    for (int i = 0; i < columnFamilyHandles.size(); i++) {
      cfHandles[i] = columnFamilyHandles.get(i).getNative();
    }

    return Arrays.asList(multiGet(getNative(), readOptions.nativeHandle_, keysArray, cfHandles));
  }

  /**
   * This function is similar to
   * {@link RocksDB#multiGetAsList} except it will
   * also read pending changes in this transaction.
   * Currently, this function will return Status::MergeInProgress if the most
   * recent write to the queried key in this batch is a Merge.
   *
   * If {@link ReadOptions#snapshot()} is not set, the current version of the
   * key will be read. Calling {@link #setSnapshot()} does not affect the
   * version of the data returned.
   *
   * Note that setting {@link ReadOptions#setSnapshot(Snapshot)} will affect
   * what is read from the DB but will NOT change which keys are read from this
   * transaction (the keys in this transaction do not yet belong to any snapshot
   * and will be fetched regardless).
   *
   * @param readOptions Read options.=
   *     {@link org.rocksdb.ColumnFamilyHandle} instances.
   * @param keys of keys for which values need to be retrieved.
   *
   * @return Array of values, one for each key
   *
   * @throws RocksDBException thrown if error happens in underlying
   *    native library.
   */
  @Deprecated
  public byte[][] multiGet(final ReadOptions readOptions, final byte[][] keys)
      throws RocksDBException {
    if(keys.length == 0) {
      return new byte[0][0];
    }

    return multiGet(getNative(), readOptions.nativeHandle_, keys);
  }

  /**
   * This function is similar to
   * {@link RocksDB#multiGetAsList} except it will
   * also read pending changes in this transaction.
   * Currently, this function will return Status::MergeInProgress if the most
   * recent write to the queried key in this batch is a Merge.
   *
   * If {@link ReadOptions#snapshot()} is not set, the current version of the
   * key will be read. Calling {@link #setSnapshot()} does not affect the
   * version of the data returned.
   *
   * Note that setting {@link ReadOptions#setSnapshot(Snapshot)} will affect
   * what is read from the DB but will NOT change which keys are read from this
   * transaction (the keys in this transaction do not yet belong to any snapshot
   * and will be fetched regardless).
   *
   * @param readOptions Read options.=
   *     {@link org.rocksdb.ColumnFamilyHandle} instances.
   * @param keys of keys for which values need to be retrieved.
   *
   * @return Array of values, one for each key
   *
   * @throws RocksDBException thrown if error happens in underlying
   *    native library.
   */
  public List<byte[]> multiGetAsList(final ReadOptions readOptions, final List<byte[]> keys)
      throws RocksDBException {
    if (keys.size() == 0) {
      return new ArrayList<>(0);
    }
    final byte[][] keysArray = keys.toArray(new byte[keys.size()][]);

    return Arrays.asList(multiGet(getNative(), readOptions.nativeHandle_, keysArray));
  }

  /**
   * Read this key and ensure that this transaction will only
   * be able to be committed if this key is not written outside this
   * transaction after it has first been read (or after the snapshot if a
   * snapshot is set in this transaction). The transaction behavior is the
   * same regardless of whether the key exists or not.
   *
   * Note: Currently, this function will return Status::MergeInProgress
   * if the most recent write to the queried key in this batch is a Merge.
   *
   * The values returned by this function are similar to
   * {@link RocksDB#get(ColumnFamilyHandle, ReadOptions, byte[])}.
   * If value==nullptr, then this function will not read any data, but will
   * still ensure that this key cannot be written to by outside of this
   * transaction.
   *
   * If this transaction was created by an {@link OptimisticTransactionDB},
   * {@link #getForUpdate(ReadOptions, ColumnFamilyHandle, byte[], boolean)}
   * could cause {@link #commit()} to fail. Otherwise, it could return any error
   * that could be returned by
   * {@link RocksDB#get(ColumnFamilyHandle, ReadOptions, byte[])}.
   *
   * If this transaction was created on a {@link TransactionDB}, an
   * {@link RocksDBException} may be thrown with an accompanying {@link Status}
   * when:
   *     {@link Status.Code#Busy} if there is a write conflict,
   *     {@link Status.Code#TimedOut} if a lock could not be acquired,
   *     {@link Status.Code#TryAgain} if the memtable history size is not large
   *         enough. See
   *         {@link ColumnFamilyOptions#maxWriteBufferNumberToMaintain()}
   *     {@link Status.Code#MergeInProgress} if merge operations cannot be
   *     resolved.
   *
   * @param readOptions Read options.
   * @param columnFamilyHandle {@link org.rocksdb.ColumnFamilyHandle}
   *     instance
   * @param key the key to retrieve the value for.
   * @param exclusive true if the transaction should have exclusive access to
   *     the key, otherwise false for shared access.
   * @param doValidate true if it should validate the snapshot before doing the read
   *
   * @return a byte array storing the value associated with the input key if
   *     any.  null if it does not find the specified key.
   *
   * @throws RocksDBException thrown if error happens in underlying
   *    native library.
   */
  public byte[] getForUpdate(final ReadOptions readOptions,
      final ColumnFamilyHandle columnFamilyHandle, final byte[] key, final boolean exclusive,
      final boolean doValidate) throws RocksDBException {
    return getForUpdate(getNative(), readOptions.nativeHandle_, key, key.length,
        columnFamilyHandle.getNative(), exclusive, doValidate);
  }

  /**
   * Same as
   * {@link #getForUpdate(ReadOptions, ColumnFamilyHandle, byte[], boolean, boolean)}
   * with doValidate=true.
   *
   * @param readOptions Read options.
   * @param columnFamilyHandle {@link org.rocksdb.ColumnFamilyHandle}
   *     instance
   * @param key the key to retrieve the value for.
   * @param exclusive true if the transaction should have exclusive access to
   *     the key, otherwise false for shared access.
   *
   * @return a byte array storing the value associated with the input key if
   *     any.  null if it does not find the specified key.
   *
   * @throws RocksDBException thrown if error happens in underlying
   *    native library.
   */
  public byte[] getForUpdate(final ReadOptions readOptions,
      final ColumnFamilyHandle columnFamilyHandle, final byte[] key,
      final boolean exclusive) throws RocksDBException {
    return getForUpdate(getNative(), readOptions.nativeHandle_, key, key.length,
        columnFamilyHandle.getNative(), exclusive, true /*doValidate*/);
  }

  /**
   * Read this key and ensure that this transaction will only
   * be able to be committed if this key is not written outside this
   * transaction after it has first been read (or after the snapshot if a
   * snapshot is set in this transaction). The transaction behavior is the
   * same regardless of whether the key exists or not.
   *
   * Note: Currently, this function will return Status::MergeInProgress
   * if the most recent write to the queried key in this batch is a Merge.
   *
   * The values returned by this function are similar to
   * {@link RocksDB#get(ReadOptions, byte[])}.
   * If value==nullptr, then this function will not read any data, but will
   * still ensure that this key cannot be written to by outside of this
   * transaction.
   *
   * If this transaction was created on an {@link OptimisticTransactionDB},
   * {@link #getForUpdate(ReadOptions, ColumnFamilyHandle, byte[], boolean)}
   * could cause {@link #commit()} to fail. Otherwise, it could return any error
   * that could be returned by
   * {@link RocksDB#get(ReadOptions, byte[])}.
   *
   * If this transaction was created on a {@link TransactionDB}, an
   * {@link RocksDBException} may be thrown with an accompanying {@link Status}
   * when:
   *     {@link Status.Code#Busy} if there is a write conflict,
   *     {@link Status.Code#TimedOut} if a lock could not be acquired,
   *     {@link Status.Code#TryAgain} if the memtable history size is not large
   *         enough. See
   *         {@link ColumnFamilyOptions#maxWriteBufferNumberToMaintain()}
   *     {@link Status.Code#MergeInProgress} if merge operations cannot be
   *     resolved.
   *
   * @param readOptions Read options.
   * @param key the key to retrieve the value for.
   * @param exclusive true if the transaction should have exclusive access to
   *     the key, otherwise false for shared access.
   *
   * @return a byte array storing the value associated with the input key if
   *     any.  null if it does not find the specified key.
   *
   * @throws RocksDBException thrown if error happens in underlying
   *    native library.
   */
  public byte[] getForUpdate(final ReadOptions readOptions, final byte[] key,
      final boolean exclusive) throws RocksDBException {
    return getForUpdate(
        getNative(), readOptions.nativeHandle_, key, key.length, exclusive, true /*doValidate*/);
  }

  /**
   * A multi-key version of
   * {@link #getForUpdate(ReadOptions, ColumnFamilyHandle, byte[], boolean)}.
   *
   *
   * @param readOptions Read options.
   * @param columnFamilyHandles {@link org.rocksdb.ColumnFamilyHandle}
   *     instances
   * @param keys the keys to retrieve the values for.
   *
   * @return Array of values, one for each key
   *
   * @throws RocksDBException thrown if error happens in underlying
   *    native library.
   */
  @Deprecated
  public byte[][] multiGetForUpdate(final ReadOptions readOptions,
      final List<ColumnFamilyHandle> columnFamilyHandles, final byte[][] keys)
      throws RocksDBException {
    // Check if key size equals cfList size. If not a exception must be
    // thrown. If not a Segmentation fault happens.
    if (keys.length != columnFamilyHandles.size()){
      throw new IllegalArgumentException(
          "For each key there must be a ColumnFamilyHandle.");
    }
    if(keys.length == 0) {
      return new byte[0][0];
    }
    final long[] cfHandles = new long[columnFamilyHandles.size()];
    for (int i = 0; i < columnFamilyHandles.size(); i++) {
      cfHandles[i] = columnFamilyHandles.get(i).getNative();
    }
    return multiGetForUpdate(getNative(), readOptions.nativeHandle_, keys, cfHandles);
  }

  /**
   * A multi-key version of
   * {@link #getForUpdate(ReadOptions, ColumnFamilyHandle, byte[], boolean)}.
   *
   *
   * @param readOptions Read options.
   * @param columnFamilyHandles {@link org.rocksdb.ColumnFamilyHandle}
   *     instances
   * @param keys the keys to retrieve the values for.
   *
   * @return Array of values, one for each key
   *
   * @throws RocksDBException thrown if error happens in underlying
   *    native library.
   */
  public List<byte[]> multiGetForUpdateAsList(final ReadOptions readOptions,
      final List<ColumnFamilyHandle> columnFamilyHandles, final List<byte[]> keys)
      throws RocksDBException {
    // Check if key size equals cfList size. If not a exception must be
    // thrown. If not a Segmentation fault happens.
    if (keys.size() != columnFamilyHandles.size()) {
      throw new IllegalArgumentException("For each key there must be a ColumnFamilyHandle.");
    }
    if (keys.size() == 0) {
      return new ArrayList<>();
    }
    final byte[][] keysArray = keys.toArray(new byte[keys.size()][]);

    final long[] cfHandles = new long[columnFamilyHandles.size()];
    for (int i = 0; i < columnFamilyHandles.size(); i++) {
      cfHandles[i] = columnFamilyHandles.get(i).getNative();
    }
    return Arrays.asList(
        multiGetForUpdate(getNative(), readOptions.nativeHandle_, keysArray, cfHandles));
  }

  /**
   * A multi-key version of {@link #getForUpdate(ReadOptions, byte[], boolean)}.
   *
   *
   * @param readOptions Read options.
   * @param keys the keys to retrieve the values for.
   *
   * @return Array of values, one for each key
   *
   * @throws RocksDBException thrown if error happens in underlying
   *    native library.
   */
  @Deprecated
  public byte[][] multiGetForUpdate(final ReadOptions readOptions, final byte[][] keys)
      throws RocksDBException {
    if(keys.length == 0) {
      return new byte[0][0];
    }

    return multiGetForUpdate(getNative(), readOptions.nativeHandle_, keys);
  }

  /**
   * A multi-key version of {@link #getForUpdate(ReadOptions, byte[], boolean)}.
   *
   *
   * @param readOptions Read options.
   * @param keys the keys to retrieve the values for.
   *
   * @return List of values, one for each key
   *
   * @throws RocksDBException thrown if error happens in underlying
   *    native library.
   */
  public List<byte[]> multiGetForUpdateAsList(
      final ReadOptions readOptions, final List<byte[]> keys) throws RocksDBException {
    if (keys.size() == 0) {
      return new ArrayList<>(0);
    }

    final byte[][] keysArray = keys.toArray(new byte[keys.size()][]);

    return Arrays.asList(multiGetForUpdate(getNative(), readOptions.nativeHandle_, keysArray));
  }

  /**
   * Returns an iterator that will iterate on all keys in the default
   * column family including both keys in the DB and uncommitted keys in this
   * transaction.
   *
   * Setting {@link ReadOptions#setSnapshot(Snapshot)} will affect what is read
   * from the DB but will NOT change which keys are read from this transaction
   * (the keys in this transaction do not yet belong to any snapshot and will be
   * fetched regardless).
   *
   * Caller is responsible for deleting the returned Iterator.
   *
   * The returned iterator is only valid until {@link #commit()},
   * {@link #rollback()}, or {@link #rollbackToSavePoint()} is called.
   *
   * @param readOptions Read options.
   *
   * @return instance of iterator object.
   */
  public RocksIterator getIterator(final ReadOptions readOptions) {
    return new RocksIterator(parent, getIterator(getNative(), readOptions.nativeHandle_));
  }

  /**
   * Returns an iterator that will iterate on all keys in the column family
   * specified by {@code columnFamilyHandle} including both keys in the DB
   * and uncommitted keys in this transaction.
   *
   * Setting {@link ReadOptions#setSnapshot(Snapshot)} will affect what is read
   * from the DB but will NOT change which keys are read from this transaction
   * (the keys in this transaction do not yet belong to any snapshot and will be
   * fetched regardless).
   *
   * Caller is responsible for calling {@link RocksIterator#close()} on
   * the returned Iterator.
   *
   * The returned iterator is only valid until {@link #commit()},
   * {@link #rollback()}, or {@link #rollbackToSavePoint()} is called.
   *
   * @param readOptions Read options.
   * @param columnFamilyHandle {@link org.rocksdb.ColumnFamilyHandle}
   *     instance
   *
   * @return instance of iterator object.
   */
  public RocksIterator getIterator(
      final ReadOptions readOptions, final ColumnFamilyHandle columnFamilyHandle) {
    return new RocksIterator(parent,
        getIterator(getNative(), readOptions.nativeHandle_, columnFamilyHandle.getNative()));
  }

  /**
   * Similar to {@link RocksDB#put(ColumnFamilyHandle, byte[], byte[])}, but
   * will also perform conflict checking on the keys be written.
   *
   * If this Transaction was created on an {@link OptimisticTransactionDB},
   * these functions should always succeed.
   *
   * If this Transaction was created on a {@link TransactionDB}, an
   * {@link RocksDBException} may be thrown with an accompanying {@link Status}
   * when:
   *     {@link Status.Code#Busy} if there is a write conflict,
   *     {@link Status.Code#TimedOut} if a lock could not be acquired,
   *     {@link Status.Code#TryAgain} if the memtable history size is not large
   *         enough. See
   *         {@link ColumnFamilyOptions#maxWriteBufferNumberToMaintain()}
   *
   * @param columnFamilyHandle The column family to put the key/value into
   * @param key the specified key to be inserted.
   * @param value the value associated with the specified key.
   * @param assumeTracked true when it is expected that the key is already
   *     tracked. More specifically, it means the the key was previous tracked
   *     in the same savepoint, with the same exclusive flag, and at a lower
   *     sequence number. If valid then it skips ValidateSnapshot,
   *     throws an error otherwise.
   *
   * @throws RocksDBException when one of the TransactionalDB conditions
   *     described above occurs, or in the case of an unexpected error
   */
  public void put(final ColumnFamilyHandle columnFamilyHandle, final byte[] key,
      final byte[] value, final boolean assumeTracked) throws RocksDBException {
    put(getNative(), key, key.length, value, value.length, columnFamilyHandle.getNative(),
        assumeTracked);
  }

  /**
   * Similar to {@link #put(ColumnFamilyHandle, byte[], byte[], boolean)}
   * but with {@code assumeTracked = false}.
   *
   * Will also perform conflict checking on the keys be written.
   *
   * If this Transaction was created on an {@link OptimisticTransactionDB},
   * these functions should always succeed.
   *
   * If this Transaction was created on a {@link TransactionDB}, an
   * {@link RocksDBException} may be thrown with an accompanying {@link Status}
   * when:
   *     {@link Status.Code#Busy} if there is a write conflict,
   *     {@link Status.Code#TimedOut} if a lock could not be acquired,
   *     {@link Status.Code#TryAgain} if the memtable history size is not large
   *         enough. See
   *         {@link ColumnFamilyOptions#maxWriteBufferNumberToMaintain()}
   *
   * @param columnFamilyHandle The column family to put the key/value into
   * @param key the specified key to be inserted.
   * @param value the value associated with the specified key.
   *
   * @throws RocksDBException when one of the TransactionalDB conditions
   *     described above occurs, or in the case of an unexpected error
   */
  public void put(final ColumnFamilyHandle columnFamilyHandle, final byte[] key,
      final byte[] value) throws RocksDBException {
    put(getNative(), key, key.length, value, value.length, columnFamilyHandle.getNative(), false);
  }

  /**
   * Similar to {@link RocksDB#put(byte[], byte[])}, but
   * will also perform conflict checking on the keys be written.
   *
   * If this Transaction was created on an {@link OptimisticTransactionDB},
   * these functions should always succeed.
   *
   *  If this Transaction was created on a {@link TransactionDB}, an
   *  {@link RocksDBException} may be thrown with an accompanying {@link Status}
   *  when:
   *    {@link Status.Code#Busy} if there is a write conflict,
   *    {@link Status.Code#TimedOut} if a lock could not be acquired,
   *    {@link Status.Code#TryAgain} if the memtable history size is not large
   *       enough. See
   *       {@link ColumnFamilyOptions#maxWriteBufferNumberToMaintain()}
   *
   * @param key the specified key to be inserted.
   * @param value the value associated with the specified key.
   *
   * @throws RocksDBException when one of the TransactionalDB conditions
   *     described above occurs, or in the case of an unexpected error
   */
  public void put(final byte[] key, final byte[] value)
      throws RocksDBException {
    put(getNative(), key, key.length, value, value.length);
  }

  //TODO(AR) refactor if we implement org.rocksdb.SliceParts in future
  /**
   * Similar to {@link #put(ColumnFamilyHandle, byte[], byte[])} but allows
   * you to specify the key and value in several parts that will be
   * concatenated together.
   *
   * @param columnFamilyHandle The column family to put the key/value into
   * @param keyParts the specified key to be inserted.
   * @param valueParts the value associated with the specified key.
   * @param assumeTracked true when it is expected that the key is already
   *     tracked. More specifically, it means the the key was previous tracked
   *     in the same savepoint, with the same exclusive flag, and at a lower
   *     sequence number. If valid then it skips ValidateSnapshot,
   *     throws an error otherwise.
   *
   * @throws RocksDBException when one of the TransactionalDB conditions
   *     described above occurs, or in the case of an unexpected error
   */
  public void put(final ColumnFamilyHandle columnFamilyHandle,
      final byte[][] keyParts, final byte[][] valueParts,
      final boolean assumeTracked) throws RocksDBException {
    put(getNative(), keyParts, keyParts.length, valueParts, valueParts.length,
        columnFamilyHandle.getNative(), assumeTracked);
  }

  /**
   * Similar to {@link #put(ColumnFamilyHandle, byte[][], byte[][], boolean)}
   * but with with {@code assumeTracked = false}.
   *
   * Allows you to specify the key and value in several parts that will be
   * concatenated together.
   *
   * @param columnFamilyHandle The column family to put the key/value into
   * @param keyParts the specified key to be inserted.
   * @param valueParts the value associated with the specified key.
   *
   * @throws RocksDBException when one of the TransactionalDB conditions
   *     described above occurs, or in the case of an unexpected error
   */
  public void put(final ColumnFamilyHandle columnFamilyHandle,
      final byte[][] keyParts, final byte[][] valueParts)
      throws RocksDBException {
    put(getNative(), keyParts, keyParts.length, valueParts, valueParts.length,
        columnFamilyHandle.getNative(), false);
  }

  //TODO(AR) refactor if we implement org.rocksdb.SliceParts in future
  /**
   * Similar to {@link #put(byte[], byte[])} but allows
   * you to specify the key and value in several parts that will be
   * concatenated together
   *
   * @param keyParts the specified key to be inserted.
   * @param valueParts the value associated with the specified key.
   *
   * @throws RocksDBException when one of the TransactionalDB conditions
   *     described above occurs, or in the case of an unexpected error
   */
  public void put(final byte[][] keyParts, final byte[][] valueParts)
      throws RocksDBException {
    put(getNative(), keyParts, keyParts.length, valueParts, valueParts.length);
  }

  /**
   * Similar to {@link RocksDB#merge(ColumnFamilyHandle, byte[], byte[])}, but
   * will also perform conflict checking on the keys be written.
   *
   * If this Transaction was created on an {@link OptimisticTransactionDB},
   * these functions should always succeed.
   *
   *  If this Transaction was created on a {@link TransactionDB}, an
   *  {@link RocksDBException} may be thrown with an accompanying {@link Status}
   *  when:
   *    {@link Status.Code#Busy} if there is a write conflict,
   *    {@link Status.Code#TimedOut} if a lock could not be acquired,
   *    {@link Status.Code#TryAgain} if the memtable history size is not large
   *       enough. See
   *       {@link ColumnFamilyOptions#maxWriteBufferNumberToMaintain()}
   *
   * @param columnFamilyHandle The column family to merge the key/value into
   * @param key the specified key to be merged.
   * @param value the value associated with the specified key.
   * @param assumeTracked true when it is expected that the key is already
   *     tracked. More specifically, it means the the key was previous tracked
   *     in the same savepoint, with the same exclusive flag, and at a lower
   *     sequence number. If valid then it skips ValidateSnapshot,
   *     throws an error otherwise.
   *
   * @throws RocksDBException when one of the TransactionalDB conditions
   *     described above occurs, or in the case of an unexpected error
   */
  public void merge(final ColumnFamilyHandle columnFamilyHandle,
      final byte[] key, final byte[] value, final boolean assumeTracked)
      throws RocksDBException {
    merge(getNative(), key, key.length, value, value.length, columnFamilyHandle.getNative(),
        assumeTracked);
  }

  /**
   * Similar to {@link #merge(ColumnFamilyHandle, byte[], byte[], boolean)}
   * but with {@code assumeTracked = false}.
   *
   * Will also perform conflict checking on the keys be written.
   *
   * If this Transaction was created on an {@link OptimisticTransactionDB},
   * these functions should always succeed.
   *
   *  If this Transaction was created on a {@link TransactionDB}, an
   *  {@link RocksDBException} may be thrown with an accompanying {@link Status}
   *  when:
   *    {@link Status.Code#Busy} if there is a write conflict,
   *    {@link Status.Code#TimedOut} if a lock could not be acquired,
   *    {@link Status.Code#TryAgain} if the memtable history size is not large
   *       enough. See
   *       {@link ColumnFamilyOptions#maxWriteBufferNumberToMaintain()}
   *
   * @param columnFamilyHandle The column family to merge the key/value into
   * @param key the specified key to be merged.
   * @param value the value associated with the specified key.
   *
   * @throws RocksDBException when one of the TransactionalDB conditions
   *     described above occurs, or in the case of an unexpected error
   */
  public void merge(final ColumnFamilyHandle columnFamilyHandle, final byte[] key,
      final byte[] value) throws RocksDBException {
    merge(getNative(), key, key.length, value, value.length, columnFamilyHandle.getNative(), false);
  }

  /**
   * Similar to {@link RocksDB#merge(byte[], byte[])}, but
   * will also perform conflict checking on the keys be written.
   *
   * If this Transaction was created on an {@link OptimisticTransactionDB},
   * these functions should always succeed.
   *
   *  If this Transaction was created on a {@link TransactionDB}, an
   *  {@link RocksDBException} may be thrown with an accompanying {@link Status}
   *  when:
   *    {@link Status.Code#Busy} if there is a write conflict,
   *    {@link Status.Code#TimedOut} if a lock could not be acquired,
   *    {@link Status.Code#TryAgain} if the memtable history size is not large
   *       enough. See
   *       {@link ColumnFamilyOptions#maxWriteBufferNumberToMaintain()}
   *
   * @param key the specified key to be merged.
   * @param value the value associated with the specified key.
   *
   * @throws RocksDBException when one of the TransactionalDB conditions
   *     described above occurs, or in the case of an unexpected error
   */
  public void merge(final byte[] key, final byte[] value)
      throws RocksDBException {
    merge(getNative(), key, key.length, value, value.length);
  }

  /**
   * Similar to {@link RocksDB#delete(ColumnFamilyHandle, byte[])}, but
   * will also perform conflict checking on the keys be written.
   *
   * If this Transaction was created on an {@link OptimisticTransactionDB},
   * these functions should always succeed.
   *
   *  If this Transaction was created on a {@link TransactionDB}, an
   *  {@link RocksDBException} may be thrown with an accompanying {@link Status}
   *  when:
   *    {@link Status.Code#Busy} if there is a write conflict,
   *    {@link Status.Code#TimedOut} if a lock could not be acquired,
   *    {@link Status.Code#TryAgain} if the memtable history size is not large
   *       enough. See
   *       {@link ColumnFamilyOptions#maxWriteBufferNumberToMaintain()}
   *
   * @param columnFamilyHandle The column family to delete the key/value from
   * @param key the specified key to be deleted.
   * @param assumeTracked true when it is expected that the key is already
   *     tracked. More specifically, it means the the key was previous tracked
   *     in the same savepoint, with the same exclusive flag, and at a lower
   *     sequence number. If valid then it skips ValidateSnapshot,
   *     throws an error otherwise.
   *
   * @throws RocksDBException when one of the TransactionalDB conditions
   *     described above occurs, or in the case of an unexpected error
   */
  public void delete(final ColumnFamilyHandle columnFamilyHandle,
      final byte[] key, final boolean assumeTracked) throws RocksDBException {
    delete(getNative(), key, key.length, columnFamilyHandle.getNative(), assumeTracked);
  }

  /**
   * Similar to {@link #delete(ColumnFamilyHandle, byte[], boolean)}
   * but with {@code assumeTracked = false}.
   *
   * Will also perform conflict checking on the keys be written.
   *
   * If this Transaction was created on an {@link OptimisticTransactionDB},
   * these functions should always succeed.
   *
   *  If this Transaction was created on a {@link TransactionDB}, an
   *  {@link RocksDBException} may be thrown with an accompanying {@link Status}
   *  when:
   *    {@link Status.Code#Busy} if there is a write conflict,
   *    {@link Status.Code#TimedOut} if a lock could not be acquired,
   *    {@link Status.Code#TryAgain} if the memtable history size is not large
   *       enough. See
   *       {@link ColumnFamilyOptions#maxWriteBufferNumberToMaintain()}
   *
   * @param columnFamilyHandle The column family to delete the key/value from
   * @param key the specified key to be deleted.
   *
   * @throws RocksDBException when one of the TransactionalDB conditions
   *     described above occurs, or in the case of an unexpected error
   */
  public void delete(final ColumnFamilyHandle columnFamilyHandle,
      final byte[] key) throws RocksDBException {
    delete(getNative(), key, key.length, columnFamilyHandle.getNative(),
        /*assumeTracked*/ false);
  }

  /**
   * Similar to {@link RocksDB#delete(byte[])}, but
   * will also perform conflict checking on the keys be written.
   *
   * If this Transaction was created on an {@link OptimisticTransactionDB},
   * these functions should always succeed.
   *
   *  If this Transaction was created on a {@link TransactionDB}, an
   *  {@link RocksDBException} may be thrown with an accompanying {@link Status}
   *  when:
   *    {@link Status.Code#Busy} if there is a write conflict,
   *    {@link Status.Code#TimedOut} if a lock could not be acquired,
   *    {@link Status.Code#TryAgain} if the memtable history size is not large
   *       enough. See
   *       {@link ColumnFamilyOptions#maxWriteBufferNumberToMaintain()}
   *
   * @param key the specified key to be deleted.
   *
   * @throws RocksDBException when one of the TransactionalDB conditions
   *     described above occurs, or in the case of an unexpected error
   */
  public void delete(final byte[] key) throws RocksDBException {
    delete(getNative(), key, key.length);
  }

  //TODO(AR) refactor if we implement org.rocksdb.SliceParts in future
  /**
   * Similar to {@link #delete(ColumnFamilyHandle, byte[])} but allows
   * you to specify the key in several parts that will be
   * concatenated together.
   *
   * @param columnFamilyHandle The column family to delete the key/value from
   * @param keyParts the specified key to be deleted.
   * @param assumeTracked true when it is expected that the key is already
   *     tracked. More specifically, it means the the key was previous tracked
   *     in the same savepoint, with the same exclusive flag, and at a lower
   *     sequence number. If valid then it skips ValidateSnapshot,
   *     throws an error otherwise.
   *
   * @throws RocksDBException when one of the TransactionalDB conditions
   *     described above occurs, or in the case of an unexpected error
   */
  public void delete(final ColumnFamilyHandle columnFamilyHandle,
      final byte[][] keyParts, final boolean assumeTracked)
      throws RocksDBException {
    delete(getNative(), keyParts, keyParts.length, columnFamilyHandle.getNative(), assumeTracked);
  }

  /**
   * Similar to{@link #delete(ColumnFamilyHandle, byte[][], boolean)}
   * but with {@code assumeTracked = false}.
   *
   * Allows you to specify the key in several parts that will be
   * concatenated together.
   *
   * @param columnFamilyHandle The column family to delete the key/value from
   * @param keyParts the specified key to be deleted.
   *
   * @throws RocksDBException when one of the TransactionalDB conditions
   *     described above occurs, or in the case of an unexpected error
   */
  public void delete(final ColumnFamilyHandle columnFamilyHandle,
      final byte[][] keyParts) throws RocksDBException {
    delete(getNative(), keyParts, keyParts.length, columnFamilyHandle.getNative(), false);
  }

  //TODO(AR) refactor if we implement org.rocksdb.SliceParts in future
  /**
   * Similar to {@link #delete(byte[])} but allows
   * you to specify key the in several parts that will be
   * concatenated together.
   *
   * @param keyParts the specified key to be deleted
   *
   * @throws RocksDBException when one of the TransactionalDB conditions
   *     described above occurs, or in the case of an unexpected error
   */
  public void delete(final byte[][] keyParts) throws RocksDBException {
    delete(getNative(), keyParts, keyParts.length);
  }

  /**
   * Similar to {@link RocksDB#singleDelete(ColumnFamilyHandle, byte[])}, but
   * will also perform conflict checking on the keys be written.
   *
   * If this Transaction was created on an {@link OptimisticTransactionDB},
   * these functions should always succeed.
   *
   *  If this Transaction was created on a {@link TransactionDB}, an
   *  {@link RocksDBException} may be thrown with an accompanying {@link Status}
   *  when:
   *    {@link Status.Code#Busy} if there is a write conflict,
   *    {@link Status.Code#TimedOut} if a lock could not be acquired,
   *    {@link Status.Code#TryAgain} if the memtable history size is not large
   *       enough. See
   *       {@link ColumnFamilyOptions#maxWriteBufferNumberToMaintain()}
   *
   * @param columnFamilyHandle The column family to delete the key/value from
   * @param key the specified key to be deleted.
   * @param assumeTracked true when it is expected that the key is already
   *     tracked. More specifically, it means the key was previously tracked
   *     in the same savepoint, with the same exclusive flag, and at a lower
   *     sequence number. If valid then it skips ValidateSnapshot,
   *     throws an error otherwise.
   *
   * @throws RocksDBException when one of the TransactionalDB conditions
   *     described above occurs, or in the case of an unexpected error
   */
  @Experimental("Performance optimization for a very specific workload")
  public void singleDelete(final ColumnFamilyHandle columnFamilyHandle,
      final byte[] key, final boolean assumeTracked) throws RocksDBException {
    singleDelete(getNative(), key, key.length, columnFamilyHandle.getNative(), assumeTracked);
  }

  /**
   * Similar to {@link #singleDelete(ColumnFamilyHandle, byte[], boolean)}
   * but with {@code assumeTracked = false}.
   *
   * will also perform conflict checking on the keys be written.
   *
   * If this Transaction was created on an {@link OptimisticTransactionDB},
   * these functions should always succeed.
   *
   *  If this Transaction was created on a {@link TransactionDB}, an
   *  {@link RocksDBException} may be thrown with an accompanying {@link Status}
   *  when:
   *    {@link Status.Code#Busy} if there is a write conflict,
   *    {@link Status.Code#TimedOut} if a lock could not be acquired,
   *    {@link Status.Code#TryAgain} if the memtable history size is not large
   *       enough. See
   *       {@link ColumnFamilyOptions#maxWriteBufferNumberToMaintain()}
   *
   * @param columnFamilyHandle The column family to delete the key/value from
   * @param key the specified key to be deleted.
   *
   * @throws RocksDBException when one of the TransactionalDB conditions
   *     described above occurs, or in the case of an unexpected error
   */
  @Experimental("Performance optimization for a very specific workload")
  public void singleDelete(final ColumnFamilyHandle columnFamilyHandle,
      final byte[] key) throws RocksDBException {
    singleDelete(getNative(), key, key.length, columnFamilyHandle.getNative(), false);
  }

  /**
   * Similar to {@link RocksDB#singleDelete(byte[])}, but
   * will also perform conflict checking on the keys be written.
   *
   * If this Transaction was created on an {@link OptimisticTransactionDB},
   * these functions should always succeed.
   *
   *  If this Transaction was created on a {@link TransactionDB}, an
   *  {@link RocksDBException} may be thrown with an accompanying {@link Status}
   *  when:
   *    {@link Status.Code#Busy} if there is a write conflict,
   *    {@link Status.Code#TimedOut} if a lock could not be acquired,
   *    {@link Status.Code#TryAgain} if the memtable history size is not large
   *       enough. See
   *       {@link ColumnFamilyOptions#maxWriteBufferNumberToMaintain()}
   *
   * @param key the specified key to be deleted.
   *
   * @throws RocksDBException when one of the TransactionalDB conditions
   *     described above occurs, or in the case of an unexpected error
   */
  @Experimental("Performance optimization for a very specific workload")
  public void singleDelete(final byte[] key) throws RocksDBException {
    singleDelete(getNative(), key, key.length);
  }

  //TODO(AR) refactor if we implement org.rocksdb.SliceParts in future
  /**
   * Similar to {@link #singleDelete(ColumnFamilyHandle, byte[])} but allows
   * you to specify the key in several parts that will be
   * concatenated together.
   *
   * @param columnFamilyHandle The column family to delete the key/value from
   * @param keyParts the specified key to be deleted.
   * @param assumeTracked true when it is expected that the key is already
   *     tracked. More specifically, it means the key was previously tracked
   *     in the same savepoint, with the same exclusive flag, and at a lower
   *     sequence number. If valid then it skips ValidateSnapshot,
   *     throws an error otherwise.
   *
   * @throws RocksDBException when one of the TransactionalDB conditions
   *     described above occurs, or in the case of an unexpected error
   */
  @Experimental("Performance optimization for a very specific workload")
  public void singleDelete(final ColumnFamilyHandle columnFamilyHandle, final byte[][] keyParts,
      final boolean assumeTracked) throws RocksDBException {
    singleDelete(
        getNative(), keyParts, keyParts.length, columnFamilyHandle.getNative(), assumeTracked);
  }

  /**
   * Similar to{@link #singleDelete(ColumnFamilyHandle, byte[][], boolean)}
   * but with {@code assumeTracked = false}.
   *
   * Allows you to specify the key in several parts that will be
   * concatenated together.
   *
   * @param columnFamilyHandle The column family to delete the key/value from
   * @param keyParts the specified key to be deleted.
   *
   * @throws RocksDBException when one of the TransactionalDB conditions
   *     described above occurs, or in the case of an unexpected error
   */
  @Experimental("Performance optimization for a very specific workload")
  public void singleDelete(final ColumnFamilyHandle columnFamilyHandle,
      final byte[][] keyParts) throws RocksDBException {
    singleDelete(getNative(), keyParts, keyParts.length, columnFamilyHandle.getNative(), false);
  }

  //TODO(AR) refactor if we implement org.rocksdb.SliceParts in future
  /**
   * Similar to {@link #singleDelete(byte[])} but allows
   * you to specify the key in several parts that will be
   * concatenated together.
   *
   * @param keyParts the specified key to be deleted.
   *
   * @throws RocksDBException when one of the TransactionalDB conditions
   *     described above occurs, or in the case of an unexpected error
   */
  @Experimental("Performance optimization for a very specific workload")
  public void singleDelete(final byte[][] keyParts) throws RocksDBException {
    singleDelete(getNative(), keyParts, keyParts.length);
  }

  /**
   * Similar to {@link RocksDB#put(ColumnFamilyHandle, byte[], byte[])},
   * but operates on the transactions write batch. This write will only happen
   * if this transaction gets committed successfully.
   *
   * Unlike {@link #put(ColumnFamilyHandle, byte[], byte[])} no conflict
   * checking will be performed for this key.
   *
   * If this Transaction was created on a {@link TransactionDB}, this function
   * will still acquire locks necessary to make sure this write doesn't cause
   * conflicts in other transactions; This may cause a {@link RocksDBException}
   * with associated {@link Status.Code#Busy}.
   *
   * @param columnFamilyHandle The column family to put the key/value into
   * @param key the specified key to be inserted.
   * @param value the value associated with the specified key.
   *
   * @throws RocksDBException when one of the TransactionalDB conditions
   *     described above occurs, or in the case of an unexpected error
   */
  public void putUntracked(final ColumnFamilyHandle columnFamilyHandle, final byte[] key,
      final byte[] value) throws RocksDBException {
    putUntracked(getNative(), key, key.length, value, value.length, columnFamilyHandle.getNative());
  }

  /**
   * Similar to {@link RocksDB#put(byte[], byte[])},
   * but operates on the transactions write batch. This write will only happen
   * if this transaction gets committed successfully.
   *
   * Unlike {@link #put(byte[], byte[])} no conflict
   * checking will be performed for this key.
   *
   * If this Transaction was created on a {@link TransactionDB}, this function
   * will still acquire locks necessary to make sure this write doesn't cause
   * conflicts in other transactions; This may cause a {@link RocksDBException}
   * with associated {@link Status.Code#Busy}.
   *
   * @param key the specified key to be inserted.
   * @param value the value associated with the specified key.
   *
   * @throws RocksDBException when one of the TransactionalDB conditions
   *     described above occurs, or in the case of an unexpected error
   */
  public void putUntracked(final byte[] key, final byte[] value)
      throws RocksDBException {
    putUntracked(getNative(), key, key.length, value, value.length);
  }

  //TODO(AR) refactor if we implement org.rocksdb.SliceParts in future
  /**
   * Similar to {@link #putUntracked(ColumnFamilyHandle, byte[], byte[])} but
   * allows you to specify the key and value in several parts that will be
   * concatenated together.
   *
   * @param columnFamilyHandle The column family to put the key/value into
   * @param keyParts the specified key to be inserted.
   * @param valueParts the value associated with the specified key.
   *
   * @throws RocksDBException when one of the TransactionalDB conditions
   *     described above occurs, or in the case of an unexpected error
   */
  public void putUntracked(final ColumnFamilyHandle columnFamilyHandle,
      final byte[][] keyParts, final byte[][] valueParts)
      throws RocksDBException {
    putUntracked(getNative(), keyParts, keyParts.length, valueParts, valueParts.length,
        columnFamilyHandle.getNative());
  }

  //TODO(AR) refactor if we implement org.rocksdb.SliceParts in future
  /**
   * Similar to {@link #putUntracked(byte[], byte[])} but
   * allows you to specify the key and value in several parts that will be
   * concatenated together.
   *
   * @param keyParts the specified key to be inserted.
   * @param valueParts the value associated with the specified key.
   *
   * @throws RocksDBException when one of the TransactionalDB conditions
   *     described above occurs, or in the case of an unexpected error
   */
  public void putUntracked(final byte[][] keyParts, final byte[][] valueParts)
      throws RocksDBException {
    putUntracked(getNative(), keyParts, keyParts.length, valueParts, valueParts.length);
  }

  /**
   * Similar to {@link RocksDB#merge(ColumnFamilyHandle, byte[], byte[])},
   * but operates on the transactions write batch. This write will only happen
   * if this transaction gets committed successfully.
   *
   * Unlike {@link #merge(ColumnFamilyHandle, byte[], byte[])} no conflict
   * checking will be performed for this key.
   *
   * If this Transaction was created on a {@link TransactionDB}, this function
   * will still acquire locks necessary to make sure this write doesn't cause
   * conflicts in other transactions; This may cause a {@link RocksDBException}
   * with associated {@link Status.Code#Busy}.
   *
   * @param columnFamilyHandle The column family to merge the key/value into
   * @param key the specified key to be merged.
   * @param value the value associated with the specified key.
   *
   * @throws RocksDBException when one of the TransactionalDB conditions
   *     described above occurs, or in the case of an unexpected error
   */
  public void mergeUntracked(final ColumnFamilyHandle columnFamilyHandle,
      final byte[] key, final byte[] value) throws RocksDBException {
    mergeUntracked(
        getNative(), key, key.length, value, value.length, columnFamilyHandle.getNative());
  }

  /**
   * Similar to {@link RocksDB#merge(byte[], byte[])},
   * but operates on the transactions write batch. This write will only happen
   * if this transaction gets committed successfully.
   *
   * Unlike {@link #merge(byte[], byte[])} no conflict
   * checking will be performed for this key.
   *
   * If this Transaction was created on a {@link TransactionDB}, this function
   * will still acquire locks necessary to make sure this write doesn't cause
   * conflicts in other transactions; This may cause a {@link RocksDBException}
   * with associated {@link Status.Code#Busy}.
   *
   * @param key the specified key to be merged.
   * @param value the value associated with the specified key.
   *
   * @throws RocksDBException when one of the TransactionalDB conditions
   *     described above occurs, or in the case of an unexpected error
   */
  public void mergeUntracked(final byte[] key, final byte[] value)
      throws RocksDBException {
    mergeUntracked(getNative(), key, key.length, value, value.length);
  }

  /**
   * Similar to {@link RocksDB#delete(ColumnFamilyHandle, byte[])},
   * but operates on the transactions write batch. This write will only happen
   * if this transaction gets committed successfully.
   *
   * Unlike {@link #delete(ColumnFamilyHandle, byte[])} no conflict
   * checking will be performed for this key.
   *
   * If this Transaction was created on a {@link TransactionDB}, this function
   * will still acquire locks necessary to make sure this write doesn't cause
   * conflicts in other transactions; This may cause a {@link RocksDBException}
   * with associated {@link Status.Code#Busy}.
   *
   * @param columnFamilyHandle The column family to delete the key/value from
   * @param key the specified key to be deleted.
   *
   * @throws RocksDBException when one of the TransactionalDB conditions
   *     described above occurs, or in the case of an unexpected error
   */
  public void deleteUntracked(final ColumnFamilyHandle columnFamilyHandle,
      final byte[] key) throws RocksDBException {
    deleteUntracked(getNative(), key, key.length, columnFamilyHandle.getNative());
  }

  /**
   * Similar to {@link RocksDB#delete(byte[])},
   * but operates on the transactions write batch. This write will only happen
   * if this transaction gets committed successfully.
   *
   * Unlike {@link #delete(byte[])} no conflict
   * checking will be performed for this key.
   *
   * If this Transaction was created on a {@link TransactionDB}, this function
   * will still acquire locks necessary to make sure this write doesn't cause
   * conflicts in other transactions; This may cause a {@link RocksDBException}
   * with associated {@link Status.Code#Busy}.
   *
   * @param key the specified key to be deleted.
   *
   * @throws RocksDBException when one of the TransactionalDB conditions
   *     described above occurs, or in the case of an unexpected error
   */
  public void deleteUntracked(final byte[] key) throws RocksDBException {
    deleteUntracked(getNative(), key, key.length);
  }

  //TODO(AR) refactor if we implement org.rocksdb.SliceParts in future
  /**
   * Similar to {@link #deleteUntracked(ColumnFamilyHandle, byte[])} but allows
   * you to specify the key in several parts that will be
   * concatenated together.
   *
   * @param columnFamilyHandle The column family to delete the key/value from
   * @param keyParts the specified key to be deleted.
   *
   * @throws RocksDBException when one of the TransactionalDB conditions
   *     described above occurs, or in the case of an unexpected error
   */
  public void deleteUntracked(final ColumnFamilyHandle columnFamilyHandle,
      final byte[][] keyParts) throws RocksDBException {
    deleteUntracked(getNative(), keyParts, keyParts.length, columnFamilyHandle.getNative());
  }

  //TODO(AR) refactor if we implement org.rocksdb.SliceParts in future
  /**
   * Similar to {@link #deleteUntracked(byte[])} but allows
   * you to specify the key in several parts that will be
   * concatenated together.
   *
   * @param keyParts the specified key to be deleted.
   *
   * @throws RocksDBException when one of the TransactionalDB conditions
   *     described above occurs, or in the case of an unexpected error
   */
  public void deleteUntracked(final byte[][] keyParts) throws RocksDBException {
    deleteUntracked(getNative(), keyParts, keyParts.length);
  }

  /**
   * Similar to {@link WriteBatch#putLogData(byte[])}
   *
   * @param blob binary object to be inserted
   */
  public void putLogData(final byte[] blob) {
    putLogData(getNative(), blob, blob.length);
  }

  /**
   * By default, all put/merge/delete operations will be indexed in the
   * transaction so that get/getForUpdate/getIterator can search for these
   * keys.
   *
   * If the caller does not want to fetch the keys about to be written,
   * they may want to avoid indexing as a performance optimization.
   * Calling {@link #disableIndexing()} will turn off indexing for all future
   * put/merge/delete operations until {@link #enableIndexing()} is called.
   *
   * If a key is put/merge/deleted after {@link #disableIndexing()} is called
   * and then is fetched via get/getForUpdate/getIterator, the result of the
   * fetch is undefined.
   */
  public void disableIndexing() {
    disableIndexing(getNative());
  }

  /**
   * Re-enables indexing after a previous call to {@link #disableIndexing()}
   */
  public void enableIndexing() {
    enableIndexing(getNative());
  }

  /**
   * Returns the number of distinct Keys being tracked by this transaction.
   * If this transaction was created by a {@link TransactionDB}, this is the
   * number of keys that are currently locked by this transaction.
   * If this transaction was created by an {@link OptimisticTransactionDB},
   * this is the number of keys that need to be checked for conflicts at commit
   * time.
   *
   * @return the number of distinct Keys being tracked by this transaction
   */
  public long getNumKeys() {
    return getNumKeys(getNative());
  }

  /**
   * Returns the number of puts that have been applied to this
   * transaction so far.
   *
   * @return the number of puts that have been applied to this transaction
   */
  public long getNumPuts() {
    return getNumPuts(getNative());
  }

  /**
   * Returns the number of deletes that have been applied to this
   * transaction so far.
   *
   * @return the number of deletes that have been applied to this transaction
   */
  public long getNumDeletes() {
    return getNumDeletes(getNative());
  }

  /**
   * Returns the number of merges that have been applied to this
   * transaction so far.
   *
   * @return the number of merges that have been applied to this transaction
   */
  public long getNumMerges() {
    return getNumMerges(getNative());
  }

  /**
   * Returns the elapsed time in milliseconds since this Transaction began.
   *
   * @return the elapsed time in milliseconds since this transaction began.
   */
  public long getElapsedTime() {
    return getElapsedTime(getNative());
  }

  /**
   * Fetch the underlying write batch that contains all pending changes to be
   * committed.
   *
   * Note: You should not write or delete anything from the batch directly and
   * should only use the functions in the {@link Transaction} class to
   * write to this transaction.
   *
   * @return The write batch
   */
  public WriteBatchWithIndex getWriteBatch() {
    final WriteBatchWithIndex writeBatchWithIndex =
        new WriteBatchWithIndex(getWriteBatch(getNative()));
    return writeBatchWithIndex;
  }

  /**
   * Change the value of {@link TransactionOptions#getLockTimeout()}
   * (in milliseconds) for this transaction.
   *
   * Has no effect on OptimisticTransactions.
   *
   * @param lockTimeout the timeout (in milliseconds) for locks used by this
   *     transaction.
   */
  public void setLockTimeout(final long lockTimeout) {
    setLockTimeout(getNative(), lockTimeout);
  }

  /**
   * Return the WriteOptions that will be used during {@link #commit()}.
   *
   * @return the WriteOptions that will be used
   */
  public WriteOptions getWriteOptions() {
    final WriteOptions writeOptions = new WriteOptions(getWriteOptions(getNative()));
    return writeOptions;
  }

  /**
   * Reset the WriteOptions that will be used during {@link #commit()}.
   *
   * @param writeOptions The new WriteOptions
   */
  public void setWriteOptions(final WriteOptions writeOptions) {
    setWriteOptions(getNative(), writeOptions.nativeHandle_);
  }

  /**
   * If this key was previously fetched in this transaction using
   * {@link #getForUpdate(ReadOptions, ColumnFamilyHandle, byte[], boolean)}/
   * {@link #multiGetForUpdate(ReadOptions, List, byte[][])}, calling
   * {@link #undoGetForUpdate(ColumnFamilyHandle, byte[])} will tell
   * the transaction that it no longer needs to do any conflict checking
   * for this key.
   *
   * If a key has been fetched N times via
   * {@link #getForUpdate(ReadOptions, ColumnFamilyHandle, byte[], boolean)}/
   * {@link #multiGetForUpdate(ReadOptions, List, byte[][])}, then
   * {@link #undoGetForUpdate(ColumnFamilyHandle, byte[])}  will only have an
   * effect if it is also called N times. If this key has been written to in
   * this transaction, {@link #undoGetForUpdate(ColumnFamilyHandle, byte[])}
   * will have no effect.
   *
   * If {@link #setSavePoint()} has been called after the
   * {@link #getForUpdate(ReadOptions, ColumnFamilyHandle, byte[], boolean)},
   * {@link #undoGetForUpdate(ColumnFamilyHandle, byte[])} will not have any
   * effect.
   *
   * If this Transaction was created by an {@link OptimisticTransactionDB},
   * calling {@link #undoGetForUpdate(ColumnFamilyHandle, byte[])} can affect
   * whether this key is conflict checked at commit time.
   * If this Transaction was created by a {@link TransactionDB},
   * calling {@link #undoGetForUpdate(ColumnFamilyHandle, byte[])} may release
   * any held locks for this key.
   *
   * @param columnFamilyHandle {@link org.rocksdb.ColumnFamilyHandle}
   *     instance
   * @param key the key to retrieve the value for.
   */
  public void undoGetForUpdate(final ColumnFamilyHandle columnFamilyHandle,
      final byte[] key) {
    undoGetForUpdate(getNative(), key, key.length, columnFamilyHandle.getNative());
  }

  /**
   * If this key was previously fetched in this transaction using
   * {@link #getForUpdate(ReadOptions, byte[], boolean)}/
   * {@link #multiGetForUpdate(ReadOptions, List, byte[][])}, calling
   * {@link #undoGetForUpdate(byte[])} will tell
   * the transaction that it no longer needs to do any conflict checking
   * for this key.
   *
   * If a key has been fetched N times via
   * {@link #getForUpdate(ReadOptions, byte[], boolean)}/
   * {@link #multiGetForUpdate(ReadOptions, List, byte[][])}, then
   * {@link #undoGetForUpdate(byte[])}  will only have an
   * effect if it is also called N times. If this key has been written to in
   * this transaction, {@link #undoGetForUpdate(byte[])}
   * will have no effect.
   *
   * If {@link #setSavePoint()} has been called after the
   * {@link #getForUpdate(ReadOptions, byte[], boolean)},
   * {@link #undoGetForUpdate(byte[])} will not have any
   * effect.
   *
   * If this Transaction was created by an {@link OptimisticTransactionDB},
   * calling {@link #undoGetForUpdate(byte[])} can affect
   * whether this key is conflict checked at commit time.
   * If this Transaction was created by a {@link TransactionDB},
   * calling {@link #undoGetForUpdate(byte[])} may release
   * any held locks for this key.
   *
   * @param key the key to retrieve the value for.
   */
  public void undoGetForUpdate(final byte[] key) {
    undoGetForUpdate(getNative(), key, key.length);
  }

  /**
   * Adds the keys from the WriteBatch to the transaction
   *
   * @param writeBatch The write batch to read from
   *
   * @throws RocksDBException if an error occurs whilst rebuilding from the
   *     write batch.
   */
  public void rebuildFromWriteBatch(final WriteBatch writeBatch)
      throws RocksDBException {
    rebuildFromWriteBatch(getNative(), writeBatch.getNative());
  }

  /**
   * Get the Commit time Write Batch.
   *
   * @return the commit time write batch.
   */
  public WriteBatch getCommitTimeWriteBatch() {
    final WriteBatch writeBatch = new WriteBatch(getCommitTimeWriteBatch(getNative()));
    return writeBatch;
  }

  /**
   * Set the log number.
   *
   * @param logNumber the log number
   */
  public void setLogNumber(final long logNumber) {
    setLogNumber(getNative(), logNumber);
  }

  /**
   * Get the log number.
   *
   * @return the log number
   */
  public long getLogNumber() {
    return getLogNumber(getNative());
  }

  /**
   * Set the name of the transaction.
   *
   * @param transactionName the name of the transaction
   *
   * @throws RocksDBException if an error occurs when setting the transaction
   *     name.
   */
  public void setName(final String transactionName) throws RocksDBException {
    setName(getNative(), transactionName);
  }

  /**
   * Get the name of the transaction.
   *
   * @return the name of the transaction
   */
  public String getName() {
    return getName(getNative());
  }

  /**
   * Get the ID of the transaction.
   *
   * @return the ID of the transaction.
   */
  public long getID() {
    return getID(getNative());
  }

  /**
   * Determine if a deadlock has been detected.
   *
   * @return true if a deadlock has been detected.
   */
  public boolean isDeadlockDetect() {
    return isDeadlockDetect(getNative());
  }

  /**
   * Get the list of waiting transactions.
   *
   * @return The list of waiting transactions.
   */
  public WaitingTransactions getWaitingTxns() {
    return getWaitingTxns(getNative());
  }

  /**
   * Get the execution status of the transaction.
   *
   * NOTE: The execution status of an Optimistic Transaction
   * never changes. This is only useful for non-optimistic transactions!
   *
   * @return The execution status of the transaction
   */
  public TransactionState getState() {
    return TransactionState.getTransactionState(getState(getNative()));
  }

  /**
   * The globally unique id with which the transaction is identified. This id
   * might or might not be set depending on the implementation. Similarly the
   * implementation decides the point in lifetime of a transaction at which it
   * assigns the id. Although currently it is the case, the id is not guaranteed
   * to remain the same across restarts.
   *
   * @return the transaction id.
   */
  @Experimental("NOTE: Experimental feature")
  public long getId() {
    return getId(getNative());
  }

  public enum TransactionState {
    STARTED((byte)0),
    AWAITING_PREPARE((byte)1),
    PREPARED((byte)2),
    AWAITING_COMMIT((byte)3),
    COMMITTED((byte)4),
    AWAITING_ROLLBACK((byte)5),
    ROLLEDBACK((byte)6),
    LOCKS_STOLEN((byte)7);

    /*
     * Keep old misspelled variable as alias
     * Tip from https://stackoverflow.com/a/37092410/454544
     */
    public static final TransactionState COMMITED = COMMITTED;

    private final byte value;

    TransactionState(final byte value) {
      this.value = value;
    }

    /**
     * Get TransactionState by byte value.
     *
     * @param value byte representation of TransactionState.
     *
     * @return {@link org.rocksdb.Transaction.TransactionState} instance or null.
     * @throws java.lang.IllegalArgumentException if an invalid
     *     value is provided.
     */
    public static TransactionState getTransactionState(final byte value) {
      for (final TransactionState transactionState : TransactionState.values()) {
        if (transactionState.value == value){
          return transactionState;
        }
      }
      throw new IllegalArgumentException(
          "Illegal value provided for TransactionState.");
    }
  }

  /**
   * Called from C++ native method {@link #getWaitingTxns(long)}
   * to construct a WaitingTransactions object.
   *
   * @param columnFamilyId The id of the {@link ColumnFamilyHandle}
   * @param key The key
   * @param transactionIds The transaction ids
   *
   * @return The waiting transactions
   */
  private WaitingTransactions newWaitingTransactions(
      final long columnFamilyId, final String key,
      final long[] transactionIds) {
    return new WaitingTransactions(columnFamilyId, key, transactionIds);
  }

  public static class WaitingTransactions {
    private final long columnFamilyId;
    private final String key;
    private final long[] transactionIds;

    private WaitingTransactions(final long columnFamilyId, final String key,
        final long[] transactionIds) {
      this.columnFamilyId = columnFamilyId;
      this.key = key;
      this.transactionIds = transactionIds;
    }

    /**
     * Get the Column Family ID.
     *
     * @return The column family ID
     */
    public long getColumnFamilyId() {
      return columnFamilyId;
    }

    /**
     * Get the key on which the transactions are waiting.
     *
     * @return The key
     */
    public String getKey() {
      return key;
    }

    /**
     * Get the IDs of the waiting transactions.
     *
     * @return The IDs of the waiting transactions
     */
    public long[] getTransactionIds() {
      return transactionIds;
    }
  }

  private native void setSnapshot(final long handle);
  private native void setSnapshotOnNextOperation(final long handle);
  private native void setSnapshotOnNextOperation(final long handle,
      final long transactionNotifierHandle);
  private native long getSnapshot(final long handle);
  private native void clearSnapshot(final long handle);
  private native void prepare(final long handle) throws RocksDBException;
  private native void commit(final long handle) throws RocksDBException;
  private native void rollback(final long handle) throws RocksDBException;
  private native void setSavePoint(final long handle) throws RocksDBException;
  private native void rollbackToSavePoint(final long handle)
      throws RocksDBException;
  private native byte[] get(final long handle, final long readOptionsHandle,
      final byte key[], final int keyLength, final long columnFamilyHandle)
      throws RocksDBException;
  private native byte[] get(final long handle, final long readOptionsHandle,
      final byte key[], final int keyLen) throws RocksDBException;
  private native byte[][] multiGet(final long handle,
      final long readOptionsHandle, final byte[][] keys,
      final long[] columnFamilyHandles) throws RocksDBException;
  private native byte[][] multiGet(final long handle,
      final long readOptionsHandle, final byte[][] keys)
      throws RocksDBException;
  private native byte[] getForUpdate(final long handle, final long readOptionsHandle,
      final byte key[], final int keyLength, final long columnFamilyHandle, final boolean exclusive,
      final boolean doValidate) throws RocksDBException;
  private native byte[] getForUpdate(final long handle, final long readOptionsHandle,
      final byte key[], final int keyLen, final boolean exclusive, final boolean doValidate)
      throws RocksDBException;
  private native byte[][] multiGetForUpdate(final long handle,
      final long readOptionsHandle, final byte[][] keys,
      final long[] columnFamilyHandles) throws RocksDBException;
  private native byte[][] multiGetForUpdate(final long handle,
      final long readOptionsHandle, final byte[][] keys)
      throws RocksDBException;
  private native long getIterator(final long handle,
      final long readOptionsHandle);
  private native long getIterator(final long handle,
      final long readOptionsHandle, final long columnFamilyHandle);
  private native void put(final long handle, final byte[] key, final int keyLength,
      final byte[] value, final int valueLength, final long columnFamilyHandle,
      final boolean assumeTracked) throws RocksDBException;
  private native void put(final long handle, final byte[] key,
      final int keyLength, final byte[] value, final int valueLength)
      throws RocksDBException;
  private native void put(final long handle, final byte[][] keys, final int keysLength,
      final byte[][] values, final int valuesLength, final long columnFamilyHandle,
      final boolean assumeTracked) throws RocksDBException;
  private native void put(final long handle, final byte[][] keys,
      final int keysLength, final byte[][] values, final int valuesLength)
      throws RocksDBException;
  private native void merge(final long handle, final byte[] key, final int keyLength,
      final byte[] value, final int valueLength, final long columnFamilyHandle,
      final boolean assumeTracked) throws RocksDBException;
  private native void merge(final long handle, final byte[] key,
      final int keyLength, final byte[] value, final int valueLength)
      throws RocksDBException;
  private native void delete(final long handle, final byte[] key, final int keyLength,
      final long columnFamilyHandle, final boolean assumeTracked) throws RocksDBException;
  private native void delete(final long handle, final byte[] key,
      final int keyLength) throws RocksDBException;
  private native void delete(final long handle, final byte[][] keys, final int keysLength,
      final long columnFamilyHandle, final boolean assumeTracked) throws RocksDBException;
  private native void delete(final long handle, final byte[][] keys,
      final int keysLength) throws RocksDBException;
  private native void singleDelete(final long handle, final byte[] key, final int keyLength,
      final long columnFamilyHandle, final boolean assumeTracked) throws RocksDBException;
  private native void singleDelete(final long handle, final byte[] key,
      final int keyLength) throws RocksDBException;
  private native void singleDelete(final long handle, final byte[][] keys, final int keysLength,
      final long columnFamilyHandle, final boolean assumeTracked) throws RocksDBException;
  private native void singleDelete(final long handle, final byte[][] keys,
      final int keysLength) throws RocksDBException;
  private native void putUntracked(final long handle, final byte[] key,
      final int keyLength, final byte[] value, final int valueLength,
      final long columnFamilyHandle) throws RocksDBException;
  private native void putUntracked(final long handle, final byte[] key,
      final int keyLength, final byte[] value, final int valueLength)
      throws RocksDBException;
  private native void putUntracked(final long handle, final byte[][] keys,
      final int keysLength, final byte[][] values, final int valuesLength,
      final long columnFamilyHandle) throws RocksDBException;
  private native void putUntracked(final long handle, final byte[][] keys,
      final int keysLength, final byte[][] values, final int valuesLength)
      throws RocksDBException;
  private native void mergeUntracked(final long handle, final byte[] key,
      final int keyLength, final byte[] value, final int valueLength,
      final long columnFamilyHandle) throws RocksDBException;
  private native void mergeUntracked(final long handle, final byte[] key,
      final int keyLength, final byte[] value, final int valueLength)
      throws RocksDBException;
  private native void deleteUntracked(final long handle, final byte[] key,
      final int keyLength, final long columnFamilyHandle)
      throws RocksDBException;
  private native void deleteUntracked(final long handle, final byte[] key,
      final int keyLength) throws RocksDBException;
  private native void deleteUntracked(final long handle, final byte[][] keys,
      final int keysLength, final long columnFamilyHandle)
      throws RocksDBException;
  private native void deleteUntracked(final long handle, final byte[][] keys,
      final int keysLength) throws RocksDBException;
  private native void putLogData(final long handle, final byte[] blob,
      final int blobLength);
  private native void disableIndexing(final long handle);
  private native void enableIndexing(final long handle);
  private native long getNumKeys(final long handle);
  private native long getNumPuts(final long handle);
  private native long getNumDeletes(final long handle);
  private native long getNumMerges(final long handle);
  private native long getElapsedTime(final long handle);
  private native long getWriteBatch(final long handle);
  private native void setLockTimeout(final long handle, final long lockTimeout);
  private native long getWriteOptions(final long handle);
  private native void setWriteOptions(final long handle,
      final long writeOptionsHandle);
  private native void undoGetForUpdate(final long handle, final byte[] key,
      final int keyLength, final long columnFamilyHandle);
  private native void undoGetForUpdate(final long handle, final byte[] key,
      final int keyLength);
  private native void rebuildFromWriteBatch(final long handle,
      final long writeBatchHandle) throws RocksDBException;
  private native long getCommitTimeWriteBatch(final long handle);
  private native void setLogNumber(final long handle, final long logNumber);
  private native long getLogNumber(final long handle);
  private native void setName(final long handle, final String name)
      throws RocksDBException;
  private native String getName(final long handle);
  private native long getID(final long handle);
  private native boolean isDeadlockDetect(final long handle);
  private native WaitingTransactions getWaitingTxns(final long handle);
  private native byte getState(final long handle);
  private native long getId(final long handle);

  @Override protected final native void nativeClose(final long handle);
  @Override protected native boolean isLastReference(long nativeAPIReference);
}
