// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "base/mem/page_allocator.h"

#include "base/debug/check.h"
#include "base/numeric.h"
#include "build/build_config.h"

#if FPAG_BUILD_FLAG(IS_OS_WIN)
#include <windows.h>
#else  // POSIX
#include <sys/mman.h>
#endif

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

namespace base {

void* reserve_pages(usize size) {
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  return VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_NOACCESS);
#else
  void* const ptr =
      mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  return ptr != MAP_FAILED ? ptr : nullptr;
#endif
}

bool commit_pages(void* ptr, usize size) {
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  return VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE) != nullptr;
#else
  return mprotect(ptr, size, PROT_READ | PROT_WRITE) == 0;
#endif
}

void decommit_pages(void* ptr, usize size) {
  dcheck(ptr);
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  VirtualFree(ptr, size, MEM_DECOMMIT);
#else
  madvise(ptr, size, MADV_DONTNEED);
  mprotect(ptr, size, PROT_NONE);
#endif
}

void* allocate_pages(usize size) {
  void* const ptr = reserve_pages(size);
  if (ptr && commit_pages(ptr, size)) {
    return ptr;
  }
  if (ptr) {
    free_pages(ptr, size);
  }
  return nullptr;
}

void* allocate_huge_pages(usize size) {
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  void* ptr =
      VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE | MEM_LARGE_PAGES,
                   PAGE_READWRITE);
  return ptr ? ptr : allocate_pages(size);
#elif FPAG_BUILD_FLAG(IS_OS_LINUX)
  void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
  return ptr != MAP_FAILED ? ptr : allocate_pages(size);
#elif FPAG_BUILD_FLAG(IS_OS_MAC)
  void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, VM_FLAGS_SUPERPAGE_SIZE_2MB, 0);
  return ptr == MAP_FAILED ? allocate_pages(size) : ptr;
#else
  return allocate_pages(size);
#endif
}

void free_pages(void* ptr, usize size) {
  dcheck(ptr);
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  VirtualFree(ptr, 0, MEM_RELEASE);
#else
  munmap(ptr, size);
#endif
}

}  // namespace base
