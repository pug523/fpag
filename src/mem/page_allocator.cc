// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "mem/page_allocator.h"

#include <cstdint>

#include "base/debug/check.h"
#include "base/numeric.h"
#include "build/build_config.h"

#if FPAG_BUILD_FLAG(IS_OS_WIN)
#include <windows.h>
#elif FPAG_BUILD_FLAG(IS_OS_POSIX)
#include <sys/mman.h>
#include <unistd.h>
#else
#error "Unsupported platform for page_allocator"
#endif

#if FPAG_BUILD_FLAG(IS_OS_MAC)
#include <fcntl.h>
#include <mach/vm_statistics.h>
#endif

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

namespace mem {

void* reserve_pages(usize size) {
  check(is_page_aligned_size(size));
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  return VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_NOACCESS);
#else
  void* const ptr =
      mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  return ptr != MAP_FAILED ? ptr : nullptr;
#endif
}

bool commit_pages(void* ptr, usize size) {
  dcheck(ptr);
  check(is_page_aligned_ptr(ptr));
  check(is_page_aligned_size(size));
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  return VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE) != nullptr;
#else
  return mprotect(ptr, size, PROT_READ | PROT_WRITE) == 0;
#endif
}

void decommit_pages(void* ptr, usize size) {
  dcheck(ptr);
  check(is_page_aligned_ptr(ptr));
  check(is_page_aligned_size(size));
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  VirtualFree(ptr, size, MEM_DECOMMIT);
#else
  madvise(ptr, size, MADV_DONTNEED);
  mprotect(ptr, size, PROT_NONE);
#endif
}

void* allocate_pages(usize size) {
  check(is_page_aligned_size(size));
  void* const ptr = reserve_pages(size);
  if (ptr) {
    if (commit_pages(ptr, size)) {
      return ptr;
    } else {
      free_pages(ptr, size);
    }
  }
  return nullptr;
}

void* allocate_huge_pages(usize size) {
  check(is_huge_page_aligned_size(size));
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  void* const ptr =
      VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE | MEM_LARGE_PAGES,
                   PAGE_READWRITE);
  return ptr ? ptr : allocate_pages(size);
#elif FPAG_BUILD_FLAG(IS_OS_LINUX)
  void* const ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
  return ptr != MAP_FAILED ? ptr : allocate_pages(size);
#elif FPAG_BUILD_FLAG(IS_OS_MAC)
  void* const ptr =
      mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
           VM_FLAGS_SUPERPAGE_SIZE_2MB, 0);
  return ptr == MAP_FAILED ? allocate_pages(size) : ptr;
#else
  return allocate_pages(size);
#endif
}

void* allocate_aliased_pages(usize size) {
  check(is_page_aligned_size(size));
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  HANDLE file_mapping =
      CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                        static_cast<DWORD>(size), nullptr);
  if (!file_mapping) {
    return allocate_pages(size);
  }

  // Try to find a contiguous region large enough for 2x mapping.
  void* base = nullptr;
  for (i32 i = 0; i < 16; ++i) {
    void* placeholder = VirtualAlloc2(nullptr, nullptr, size * 2,
                                      MEM_RESERVE | MEM_RESERVE_PLACEHOLDER,
                                      PAGE_NOACCESS, nullptr, 0);
    if (!placeholder) {
      break;
    }
    // Split the placeholder into two halves.
    if (!VirtualFree(placeholder, size,
                     MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER)) {
      VirtualFree(placeholder, 0, MEM_RELEASE);
      break;
    }
    void* view1 =
        MapViewOfFile3(file_mapping, nullptr, placeholder, 0, size,
                       MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, nullptr, 0);
    if (!view1) {
      VirtualFree(placeholder, 0, MEM_RELEASE);
      VirtualFree(static_cast<char*>(placeholder) + size, 0, MEM_RELEASE);
      break;
    }
    void* view2 = MapViewOfFile3(
        file_mapping, nullptr, static_cast<char*>(placeholder) + size, 0, size,
        MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, nullptr, 0);
    if (!view2) {
      UnmapViewOfFile(view1);
      VirtualFree(static_cast<char*>(placeholder) + size, 0, MEM_RELEASE);
      break;
    }
    base = view1;
    break;
  }
  CloseHandle(file_mapping);
  return base ? base : allocate_pages(size);

#elif FPAG_BUILD_FLAG(IS_OS_LINUX)
  i32 fd = memfd_create("aliased_pages", 0);
  if (fd == -1) {
    return allocate_pages(size);
  }
  if (ftruncate(fd, static_cast<off_t>(size)) == -1) {
    close(fd);
    return allocate_pages(size);
  }
  // Reserve 2x virtual space.
  void* const base =
      mmap(nullptr, size * 2, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (base == MAP_FAILED) {
    close(fd);
    return allocate_pages(size);
  }
  // Map the same physical pages twice, consecutively.
  void* const view1 =
      mmap(base, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
  void* const view2 =
      mmap(static_cast<char*>(base) + size, size, PROT_READ | PROT_WRITE,
           MAP_SHARED | MAP_FIXED, fd, 0);
  close(fd);
  if (view1 == MAP_FAILED || view2 == MAP_FAILED) {
    munmap(base, size * 2);
    return allocate_pages(size);
  }
  return base;

#elif FPAG_BUILD_FLAG(IS_OS_MAC)
  i32 fd = shm_open(SHM_ANON, O_RDWR | O_CREAT, 0600);
  if (fd == -1) {
    return allocate_pages(size);
  }
  if (ftruncate(fd, static_cast<off_t>(size)) == -1) {
    close(fd);
    return allocate_pages(size);
  }
  void* const base =
      mmap(nullptr, size * 2, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (base == MAP_FAILED) {
    close(fd);
    return allocate_pages(size);
  }
  void* const view1 =
      mmap(base, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
  void* const view2 =
      mmap(static_cast<char*>(base) + size, size, PROT_READ | PROT_WRITE,
           MAP_SHARED | MAP_FIXED, fd, 0);
  close(fd);
  if (view1 == MAP_FAILED || view2 == MAP_FAILED) {
    munmap(base, size * 2);
    return allocate_pages(size);
  }
  return base;
#endif
}

void free_pages(void* ptr, usize size) {
  dcheck(ptr);
  check(is_page_aligned_ptr(ptr));
  check(is_page_aligned_size(size));
#if FPAG_BUILD_FLAG(IS_OS_WIN)
  (void)size;
  VirtualFree(ptr, 0, MEM_RELEASE);
#else
  munmap(ptr, size);
#endif
}

}  // namespace mem
