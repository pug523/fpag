// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <cstring>
#include <string_view>
#include <type_traits>

#include "base/numeric.h"
#include "base/spsc_queue.h"
#include "logging/async/codec/codec.h"
#include "logging/async/deserializer.h"
#include "logging/log_level.h"
#include "str/format_util.h"

// NOLINTBEGIN
// clang-format off
#include "logging/async/codec/basic_copy_codec.h"
#include "logging/async/codec/string_copy_codec.h"
#include "logging/async/codec/vector_copy_codec.h"
// clang-format on
// NOLINTEND

namespace logging {

template <typename... Args>
class Serializer {
 public:
  Serializer() = delete;

  static constexpr void serialize_to(LogLevel level,
                                     base::SpscQueue* queue,
                                     str::format_string<Args&&...> fmt,
                                     Args&&... args) {
    constexpr bool kAllArgsFixedSize =
        (Codec<std::decay_t<Args>>::is_fixed_size() && ...);

    constexpr DeserializeFunction kDeserializeFunc =
        &Deserializer<Args...>::deserialize;

    if constexpr (sizeof...(Args) == 0) {
      void* out_ptr = nullptr;
      queue->reserve(kPayloadHeaderSize, &out_ptr);
      write_header(static_cast<char*>(out_ptr), kPayloadHeaderSize,
                   kDeserializeFunc, level, fmt);
      queue->commit(kPayloadHeaderSize);
    } else if constexpr (kAllArgsFixedSize) {
      constexpr usize kArgsSize =
          (Codec<std::decay_t<Args>>::serialized_size() + ...);
      constexpr usize kTotalPayloadSize = kPayloadHeaderSize + kArgsSize;
      void* out_ptr = nullptr;
      queue->reserve(kTotalPayloadSize, &out_ptr);
      write_header(static_cast<char*>(out_ptr), kTotalPayloadSize,
                   kDeserializeFunc, level, fmt);

      char* arg_out_cursor = static_cast<char*>(out_ptr) + kPayloadHeaderSize;
      // clang-format off
      ([&] {
        using Codec = Codec<std::decay_t<decltype(args)>>;
        Codec::encode(arg_out_cursor, args);
        arg_out_cursor += Codec::serialized_size();
      }(), ...);
      // clang-format on

      queue->commit(kTotalPayloadSize);
    } else {
      char args_buf[512];
      usize args_serialized_size = 0;

      char* arg_out_cursor = args_buf;
      // clang-format off
      ([&] {
        using Codec = Codec<std::decay_t<decltype(args)>>;
        const usize written = Codec::encode(arg_out_cursor, args);
        args_serialized_size += written;
        arg_out_cursor += written;
      }(), ...);
      // clang-format on

      void* out_ptr = nullptr;
      const usize total_payload_size =
          kPayloadHeaderSize + args_serialized_size;
      queue->reserve(total_payload_size, &out_ptr);
      write_header(static_cast<char*>(out_ptr), total_payload_size,
                   kDeserializeFunc, level, fmt);
      std::memcpy(static_cast<char*>(out_ptr) + kPayloadHeaderSize, args_buf,
                  args_serialized_size);

      queue->commit(total_payload_size);
    }
  }

 private:
  inline static constexpr void write_header(
      char* out_ptr,
      usize total_payload_size,
      DeserializeFunction func,
      LogLevel level,
      str::format_string<Args...> fmt_string) {
    char* out_cursor = out_ptr;

    std::memcpy(out_cursor, &total_payload_size, sizeof(total_payload_size));
    out_cursor += sizeof(total_payload_size);
    std::memcpy(out_cursor, &func, sizeof(func));
    out_cursor += sizeof(func);
    std::memcpy(out_cursor, &level, sizeof(level));
    out_cursor += sizeof(level);
    std::memcpy(out_cursor, &fmt_string, sizeof(fmt_string));
  }
};

}  // namespace logging
