/*
 * Copyright (c) 1997, 2010, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#include "precompiled.hpp"
#include "code/icBuffer.hpp"
#include "gc_interface/collectedHeap.hpp"
#include "interpreter/bytecodes.hpp"
#include "memory/universe.hpp"
#include "prims/methodHandles.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/icache.hpp"
#include "runtime/init.hpp"
#include "runtime/safepoint.hpp"
#include "runtime/sharedRuntime.hpp"

// Initialization done by VM thread in vm_init_globals()
void check_ThreadShadow();
void eventlog_init();
void mutex_init();
void chunkpool_init();
void perfMemory_init();

// Initialization done by Java thread in init_globals()
void management_init();
void bytecodes_init();
void classLoader_init();
void codeCache_init();
void VM_Version_init();
void stubRoutines_init1();
jint universe_init();  // dependent on codeCache_init and stubRoutines_init
void interpreter_init();  // before any methods loaded
void invocationCounter_init();  // before any methods loaded
void marksweep_init();
void accessFlags_init();
void templateTable_init();
void InterfaceSupport_init();
void universe2_init();  // dependent on codeCache_init and stubRoutines_init
void referenceProcessor_init();
void jni_handles_init();
void vmStructs_init();

void vtableStubs_init();
void InlineCacheBuffer_init();
void compilerOracle_init();
void compilationPolicy_init();


// Initialization after compiler initialization
bool universe_post_init();  // must happen after compiler_init
void javaClasses_init();  // must happen after vtable initialization
void stubRoutines_init2(); // note: StubRoutines need 2-phase init

// Do not disable thread-local-storage, as it is important for some
// JNI/JVM/JVMTI functions and signal handlers to work properly
// during VM shutdown
void perfMemory_exit();
void ostream_exit();

// Initialize global data structures and create system classes in heap
void vm_init_globals() {
	//做一些检查
  check_ThreadShadow();
  //根据平台 操作系统位数做一些基本类型检查和初始化工作
  basic_types_init();
  //初始化event log buffer
  eventlog_init();
  //初始化mutex 用于互斥
  mutex_init();
  //初始化chunk pool 里面三个chunked list
  //使用os 的malloc分配系统堆内存，并作为堆块池共用
  chunkpool_init();
  //perf data  性能监控相关初始化
  perfMemory_init();
}


jint init_globals() {
  HandleMark hm;
  //初始化一些计数器
  management_init();
  //初始化java字节码指令 byte 数值
  bytecodes_init();
  //初始化system  class loader
  classLoader_init();
  //JIT(及时编译)产生的机器码的缓存
  codeCache_init();
  //虚拟机版本信息
  VM_Version_init();
  //jit编译成的机器码入口 c1 编译
  stubRoutines_init1();
  jint status = universe_init();  // dependent on codeCache_init and stubRoutines_init
  if (status != JNI_OK)
    return status;
  //初始化解释器
  interpreter_init();  // before any methods loaded
  //执行计数器，用于JIT优化
  invocationCounter_init();  // before any methods loaded
  //gc
  marksweep_init();
  //控制
  accessFlags_init();
  //字节码table
  templateTable_init();
  //
  InterfaceSupport_init();
  //jit相关
  SharedRuntime::generate_stubs();
  universe2_init();  // dependent on codeCache_init and stubRoutines_init
  //java对象引用处理
  referenceProcessor_init();
  //初始化全局/局部jni handles
  jni_handles_init();
#ifndef VM_STRUCTS_KERNEL
  vmStructs_init();
#endif // VM_STRUCTS_KERNEL
//初始化虚表 用于动态绑定
  vtableStubs_init();
  //JIT 内联cache
  InlineCacheBuffer_init();
  //
  compilerOracle_init();
  //CompilationPolicy 用于选择jit编译的方法或者代码块
  compilationPolicy_init();
  VMRegImpl::set_regName();

  if (!universe_post_init()) {
    return JNI_ERR;
  }
  //计算/检查 jvm预加载的 java.lang.*等 well-known-class
  //保证java.lang包下类的正确性，不能轻易被修改，替换
  //初始化过field 过滤器
  javaClasses_init();  // must happen after vtable initialization
  stubRoutines_init2(); // note: StubRoutines need 2-phase init

  // Although we'd like to, we can't easily do a heap verify
  // here because the main thread isn't yet a JavaThread, so
  // its TLAB may not be made parseable from the usual interfaces.
  if (VerifyBeforeGC && !UseTLAB &&
      Universe::heap()->total_collections() >= VerifyGCStartAt) {
    Universe::heap()->prepare_for_verify();
    Universe::verify();   // make sure we're starting with a clean slate
  }

  // All the flags that get adjusted by VM_Version_init and os::init_2
  // have been set so dump the flags now.
  if (PrintFlagsFinal) {
    CommandLineFlags::printFlags();
  }

  return JNI_OK;
}


void exit_globals() {
  static bool destructorsCalled = false;
  if (!destructorsCalled) {
    destructorsCalled = true;
    perfMemory_exit();
    if (PrintSafepointStatistics) {
      // Print the collected safepoint statistics.
      SafepointSynchronize::print_stat_on_exit();
    }
    ostream_exit();
  }
}


static bool _init_completed = false;

bool is_init_completed() {
  return _init_completed;
}


void set_init_completed() {
  assert(Universe::is_fully_initialized(), "Should have completed initialization");
  _init_completed = true;
}
