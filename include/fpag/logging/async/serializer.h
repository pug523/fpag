// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <cstdint>
#include <cstring>
#include <string_view>
#include <type_traits>

#include "fmt/compile.h"
#include "fpag/base/debug/check.h"
#include "fpag/base/math_util.h"
#include "fpag/base/numeric.h"
#include "fpag/base/spsc_queue.h"
#include "fpag/logging/async/codec/codec.h"
#include "fpag/logging/async/deserializer.h"
#include "fpag/logging/log_level.h"

// NOLINTBEGIN
// clang-format off
#include "fpag/logging/async/codec/basic_copy_codec.h"
#include "fpag/logging/async/codec/string_copy_codec.h"
#include "fpag/logging/async/codec/vector_copy_codec.h"
// clang-format on
// NOLINTEND

namespace logging {

template <typename Format, typename... Args>
class Serializer {
 public:
  Serializer() = delete;

  static void serialize_to(LogLevel level,
                           base::SpscQueue* queue,
                           Format fmt,
                           Args&&... args) {
    constexpr bool kAreAllArgsFixedSize =
        (Codec<std::decay_t<Args>>::is_fixed_size() && ...);

    using Deserializer = Deserializer<Format, Args...>;
    static constexpr DeserializeFunction kDeserializeFunc =
        &Deserializer::deserialize;

    const std::string_view fmt_str = static_cast<std::string_view>(fmt);

    if constexpr (sizeof...(Args) == 0) {
      void* out_ptr = nullptr;
      queue->reserve(Deserializer::kPayloadHeaderSize, &out_ptr, kPayloadAlign);
      write_header(static_cast<char*>(out_ptr),
                   Deserializer::kPayloadHeaderSize, kDeserializeFunc, level,
                   fmt_str);
      queue->commit(Deserializer::kPayloadHeaderSize);
    } else if constexpr (kAreAllArgsFixedSize) {
      constexpr usize kArgsSize =
          (Codec<std::decay_t<Args>>::serialized_size() + ...);
      constexpr usize kTotalPayloadSize =
          Deserializer::kPayloadHeaderSize + kArgsSize;
      void* out_ptr = nullptr;
      queue->reserve(kTotalPayloadSize, &out_ptr, kPayloadAlign);
      // write_header(static_cast<char*>(out_ptr), kTotalPayloadSize,
      //              kDeserializeFunc, level, fmt_str);

      write_header(static_cast<char*>(out_ptr), kTotalPayloadSize,
                   kDeserializeFunc, level, fmt_str);

      char* arg_out_cursor =
          static_cast<char*>(out_ptr) + Deserializer::kPayloadHeaderSize;
      // clang-format off
      ([&] {
        using Codec = Codec<std::decay_t<decltype(args)>>;
        Codec::encode(arg_out_cursor, args);
        arg_out_cursor += Codec::serialized_size();
      }(), ...);
      // clang-format on

      FPAG_DCHECK(arg_out_cursor - static_cast<const char*>(out_ptr) ==
                  kTotalPayloadSize);
      queue->commit(kTotalPayloadSize);
    } else {
      constexpr usize kDynamicSizeArgsBufSize = 4096;
      char args_buf[kDynamicSizeArgsBufSize];
      usize args_serialized_size = 0;

      char* arg_out_cursor = args_buf;
      // clang-format off
      // NOLINTBEGIN(whitespace/braces,whitespace/line_length)
      ([&] {
        using Codec = Codec<std::decay_t<decltype(args)>>;
        const usize written = Codec::encode(arg_out_cursor + sizeof(written), args);
        std::memcpy(arg_out_cursor, &written, sizeof(written));

        args_serialized_size += sizeof(written) + written;
        arg_out_cursor += sizeof(written) + written;

        FPAG_DCHECK_LE(args_serialized_size, kDynamicSizeArgsBufSize);
      }(), ...);
      // NOLINTEND(whitespace/braces,whitespace/line_length)
      // clang-format on

      void* out_ptr = nullptr;
      const usize total_payload_size =
          Deserializer::kPayloadHeaderSize + args_serialized_size;
      queue->reserve(total_payload_size, &out_ptr, kPayloadAlign);
      write_header(static_cast<char*>(out_ptr), total_payload_size,
                   kDeserializeFunc, level, fmt_str);
      std::memcpy(
          static_cast<char*>(out_ptr) + Deserializer::kPayloadHeaderSize,
          args_buf, args_serialized_size);

      queue->commit(total_payload_size);
    }
  }

 private:
  inline static constexpr void write_header(char* out_ptr,
                                            usize total_payload_size,
                                            DeserializeFunction func,
                                            LogLevel level,
                                            std::string_view fmt_string) {
    char* out_cursor = out_ptr;

    FPAG_DCHECK(reinterpret_cast<uintptr_t>(out_cursor) % 8 == 0);

    std::memcpy(out_cursor, &total_payload_size, sizeof(total_payload_size));
    out_cursor += base::round_up(sizeof(total_payload_size), alignof(usize));
    std::memcpy(out_cursor, &func, sizeof(func));
    out_cursor += base::round_up(sizeof(func), alignof(DeserializeFunction));
    std::memcpy(out_cursor, &level, sizeof(level));
    if constexpr (!fmt::is_compiled_string<Format>::value) {
      out_cursor += base::round_up(sizeof(level), kPayloadAlign);
      std::memcpy(out_cursor, &fmt_string, sizeof(fmt_string));
    }
  }
};

}  // namespace logging
