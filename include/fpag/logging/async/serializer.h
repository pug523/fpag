// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <cstdint>
#include <cstring>
#include <string_view>
#include <type_traits>

#include "fmt/compile.h"
#include "fpag/base/math_util.h"
#include "fpag/base/numeric.h"
#include "fpag/container/spsc_queue.h"
#include "fpag/debug/check.h"
#include "fpag/logging/async/codec/codec.h"
#include "fpag/logging/async/deserializer.h"
#include "fpag/logging/log_level.h"

// NOLINTBEGIN
// clang-format off
#include "fpag/logging/async/codec/basic_copy_codec.h"
#include "fpag/logging/async/codec/string_copy_codec.h"
#include "fpag/logging/async/codec/vector_copy_codec.h"
#include "fpag/str/string_interner.h"
// clang-format on
// NOLINTEND

namespace logging {

template <typename Format, bool kUseInterner, typename... Args>
class Serializer {
 public:
  Serializer() = delete;

  static void serialize_to(LogLevel level,
                           str::StringInterner* interner,
                           container::SpscQueue* queue,
                           Format fmt,
                           Args&&... args) {
    constexpr bool kAreAllArgsFixedSize =
        (Codec<std::decay_t<Args>>::is_fixed_size() && ...);

    using Deserializer = Deserializer<Format, kUseInterner, Args...>;
    static constexpr DeserializeFunction kDeserializeFunc =
        &Deserializer::deserialize;
    using Eqs = container::SpscQueue::EnqueueStatus;

    if constexpr (sizeof...(Args) == 0) {
      void* out_ptr = nullptr;
      const Eqs status = queue->reserve(Deserializer::kPayloadHeaderSize,
                                        &out_ptr, kPayloadAlign);
      if (status == Eqs::Dropped) [[unlikely]] {
        return;
      }

      write_header(static_cast<char*>(out_ptr),
                   Deserializer::kPayloadHeaderSize, kDeserializeFunc, level,
                   fmt, interner);
      queue->commit(Deserializer::kPayloadHeaderSize);
    } else if constexpr (kAreAllArgsFixedSize) {
      constexpr usize kArgsSize =
          (Codec<std::decay_t<Args>>::body_size() + ...);
      constexpr usize kTotalPayloadSize =
          Deserializer::kPayloadHeaderSize + kArgsSize;
      void* out_ptr = nullptr;
      const Eqs status =
          queue->reserve(kTotalPayloadSize, &out_ptr, kPayloadAlign);
      if (status == Eqs::Dropped) [[unlikely]] {
        return;
      }

      write_header(static_cast<char*>(out_ptr), kTotalPayloadSize,
                   kDeserializeFunc, level, fmt, interner);

      char* arg_out_cursor =
          static_cast<char*>(out_ptr) + Deserializer::kPayloadHeaderSize;
      // clang-format off
      ([&] {
        using Codec = Codec<std::decay_t<decltype(args)>>;
        Codec::encode(arg_out_cursor, args);
        arg_out_cursor += Codec::body_size();
      }(), ...);
      // clang-format on

      FPAG_DCHECK(arg_out_cursor - static_cast<const char*>(out_ptr) ==
                  kTotalPayloadSize);
      queue->commit(kTotalPayloadSize);
    } else {
      usize args_body_size = 0;
      // clang-format off
      // NOLINTBEGIN(whitespace/braces,whitespace/line_length)
      ([&] {
        using Codec = Codec<std::decay_t<decltype(args)>>;
        if constexpr (Codec::is_fixed_size()) {
          args_body_size += Codec::body_size();
        } else {
          // A variable length argument crosses with a size slot in front of
          // its body.
          args_body_size += sizeof(usize) + Codec::encoded_size(args);
        }
      }(), ...);
      // NOLINTEND(whitespace/braces,whitespace/line_length)
      // clang-format on

      void* out_ptr = nullptr;
      const usize total_payload_size =
          Deserializer::kPayloadHeaderSize + args_body_size;
      const Eqs status =
          queue->reserve(total_payload_size, &out_ptr, kPayloadAlign);
      if (status == Eqs::Dropped) [[unlikely]] {
        return;
      }

      char* const payload_head = static_cast<char*>(out_ptr);
      write_header(payload_head, total_payload_size, kDeserializeFunc, level,
                   fmt, interner);

      char* arg_out_cursor = payload_head + Deserializer::kPayloadHeaderSize;
      // clang-format off
      // NOLINTBEGIN(whitespace/braces,whitespace/line_length)
      ([&] {
        using Codec = Codec<std::decay_t<decltype(args)>>;
        if constexpr (Codec::is_fixed_size()) {
          // A fixed size argument has no size slot: the deserializer derives
          // its size from the type.
          Codec::encode(arg_out_cursor, args);
          arg_out_cursor += Codec::body_size();
        } else {
          const usize written =
              Codec::encode(arg_out_cursor + sizeof(written), args);
          std::memcpy(arg_out_cursor, &written, sizeof(written));
          arg_out_cursor += sizeof(written) + written;
        }
      }(), ...);
      // NOLINTEND(whitespace/braces,whitespace/line_length)
      // clang-format on

      // The encode pass must write exactly what the size pass reserved: more
      // would overrun the slot, less would leave a hole in the record.
      FPAG_DCHECK_EQ(static_cast<usize>(arg_out_cursor - payload_head),
                     total_payload_size);
      queue->commit(total_payload_size);
    }
  }

 private:
  inline static constexpr void write_header(char* out_ptr,
                                            usize total_payload_size,
                                            DeserializeFunction func,
                                            LogLevel level,
                                            Format fmt,
                                            str::StringInterner* interner) {
    char* out_cursor = out_ptr;

    FPAG_DCHECK(reinterpret_cast<uintptr_t>(out_cursor) % 8 == 0);

    std::memcpy(out_cursor, &total_payload_size, sizeof(total_payload_size));
    out_cursor += base::round_up(sizeof(total_payload_size), alignof(usize));
    std::memcpy(out_cursor, &func, sizeof(func));
    out_cursor += base::round_up(sizeof(func), alignof(DeserializeFunction));
    std::memcpy(out_cursor, &level, sizeof(level));
    if constexpr (!fmt::is_compiled_string<Format>::value) {
      out_cursor += base::round_up(sizeof(level), kPayloadAlign);

      const std::string_view fmt_string = static_cast<std::string_view>(fmt);
      if constexpr (kUseInterner) {
        const str::StringInterner::StringId id = interner->intern(fmt_string);
        std::memcpy(out_cursor, &id, sizeof(id));
      } else {
        std::memcpy(out_cursor, &fmt_string, sizeof(fmt_string));
      }
    }
  }
};

}  // namespace logging
