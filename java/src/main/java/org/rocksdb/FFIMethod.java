// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

package org.rocksdb;

import java.lang.foreign.*;
import java.lang.invoke.MethodHandle;
import java.util.Optional;

/**
 * Helper class creates a MethodHandle for each of the known C++ methods which support our RocksDB
 * Java interface
 */
public class FFIMethod {
  private static final Linker linker = Linker.nativeLinker();
  private static final SymbolLookup symbolLookup = SymbolLookup.loaderLookup();

  public static MethodHandle Lookup(final String name, final FunctionDescriptor functionDescriptor)
      throws RocksDBException {
    final Optional<MemorySegment> symbolSegment = symbolLookup.lookup(name);
    if (symbolSegment.isPresent()) {
      return linker.downcallHandle(symbolSegment.get(), functionDescriptor);
    }
    throw new RocksDBException(
        "FFI linker lookup failed for name: " + name + ", function: " + functionDescriptor);
  }

  static {
    try {
      Get = Lookup("rocksdb_ffi_get",
          FunctionDescriptor.of(ValueLayout.JAVA_INT, ValueLayout.ADDRESS, ValueLayout.ADDRESS,
              ValueLayout.ADDRESS, ValueLayout.ADDRESS));
      ResetOutput = Lookup("rocksdb_ffi_reset_output",
          FunctionDescriptor.of(ValueLayout.JAVA_INT, ValueLayout.ADDRESS));
    } catch (final RocksDBException e) {
      throw new RuntimeException(e);
    }
  }

  public static MethodHandle Get;
  public static MethodHandle ResetOutput;
}
