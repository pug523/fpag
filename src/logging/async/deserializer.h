// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <cstring>
#include <tuple>
#include <type_traits>

#include "base/numeric.h"
#include "logging/async/codec/codec.h"
#include "logging/log_level.h"
#include "str/format_util.h"

namespace logging {

using DeserializeFunction = usize (*)(const char* payload_head_ptr,
                                      usize total_payload_size,
                                      char* format_buf,
                                      usize format_buf_size);

constexpr usize kPayloadHeaderSize = sizeof(DeserializeFunction) +
                                     sizeof(usize) + sizeof(LogLevel) +
                                     sizeof(str::format_string<>);

template <typename... Args>
class Deserializer {
 public:
  using DecodedArgs =
      std::tuple<typename Codec<std::decay_t<Args>>::DecodedType...>;

  // TODO: recursive packed parameter iteration to update data cursor with
  // offset.
  inline static DecodedArgs decode_args(const char* data, usize size) {
    DecodedArgs args_tuple{Codec<std::decay_t<Args>>::decode(data, size)...};
    return args_tuple;
  }

  // Match with DeserializeFunction
  static usize deserialize(const char* payload_head_ptr,
                           usize total_payload_size,
                           char* format_buf,
                           usize format_buf_size) {
    const char* cursor = payload_head_ptr;

    // Already read total payload size and deserializer so skip them.
    cursor += sizeof(total_payload_size) + sizeof(DeserializeFunction) +
              sizeof(LogLevel);

    // TODO: do not copy format string on runtime, it should be a compile-time
    // constant which can be embedded in the binary

    // Copy the format string into a properly aligned raw buffer, then
    // reinterpret it as a const str::format_string<Args...>.
    alignas(str::format_string<Args...>) char
        fmt_storage[sizeof(str::format_string<Args...>)];
    std::memcpy(fmt_storage, cursor, sizeof(fmt_storage));
    const str::format_string<Args...>& fmt =
        *reinterpret_cast<const str::format_string<Args...>*>(fmt_storage);
    cursor += sizeof(fmt);

    const char* args_payload_ptr = cursor;
    const usize args_payload_size = total_payload_size - kPayloadHeaderSize;

    usize result_size;
    std::apply(
        [&](auto&&... args) {
          result_size = static_cast<usize>(
              str::format_to_n(format_buf, format_buf_size, fmt, args...).size);
        },
        decode_args(args_payload_ptr, args_payload_size));
    return result_size;
  }
};

}  // namespace logging
