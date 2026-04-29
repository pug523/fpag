// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <cstring>
#include <iterator>
#include <string_view>
#include <tuple>
#include <type_traits>

#include "fmt/base.h"
#include "fmt/compile.h"
#include "fmt/format.h"
#include "fpag/base/debug/check.h"
#include "fpag/base/math_util.h"
#include "fpag/base/numeric.h"
#include "fpag/logging/async/codec/codec.h"
#include "fpag/logging/format_buffer.h"
#include "fpag/logging/log_level.h"

namespace logging {

using DeserializeFunction = void (*)(const char* payload_head_ptr,
                                     usize total_payload_size,
                                     format_buffer* fmt_buf);

// For function pointer alignment
static constexpr usize kPayloadAlign = 8;
static constexpr usize kPayloadMinHeaderSize =
    sizeof(usize) + sizeof(DeserializeFunction) +
    base::round_up(sizeof(LogLevel), kPayloadAlign);

template <typename Format, typename... Args>
class Deserializer {
 public:
  using DecodedArgs =
      std::tuple<typename Codec<std::decay_t<Args>>::DecodedType...>;

  static constexpr bool kIsCompiledFmt = fmt::is_compiled_string<Format>::value;
  static constexpr usize kPayloadHeaderSize =
      kPayloadMinHeaderSize +
      (kIsCompiledFmt ? 0 : sizeof(fmt::format_string<>));

  // TODO: recursive packed parameter iteration to update data cursor with
  // offset.
  inline static DecodedArgs decode_args(const char* data, usize size) {
    const char* data_cursor = data;
    usize remaining_size = size;

    if constexpr (sizeof...(Args) == 0) {
      return std::make_tuple();
    } else {
      // clang-format off
      return DecodedArgs {([&data_cursor, &remaining_size] {
        using Arg = std::decay_t<Args>;
        using C = Codec<Arg>;
        using R = typename C::DecodedType;

        if constexpr (C::is_fixed_size()) {
          // Has no size slot.
          constexpr usize kSerializedSize = C::serialized_size();
          FPAG_DCHECK_LE(kSerializedSize, remaining_size);
          R arg = C::decode(data_cursor, kSerializedSize);
          data_cursor += kSerializedSize;
          remaining_size -= kSerializedSize;
          return arg;
        } else {
          // Has size slot.
          usize body_size;

          FPAG_DCHECK_LE(sizeof(body_size), remaining_size);
          std::memcpy(&body_size, data_cursor, sizeof(body_size));
          data_cursor += sizeof(body_size);
          remaining_size -= sizeof(body_size);

          FPAG_DCHECK_LE(body_size, remaining_size);
          R arg = C::decode(data_cursor, body_size);
          data_cursor += body_size;
          remaining_size -= body_size;
          return arg;
        }
      }())...};
      // clang-format on
    }
  }

  // Match with DeserializeFunction
  static void deserialize(const char* payload_head_ptr,
                          usize total_payload_size,
                          format_buffer* fmt_buf) {
    const char* cursor = payload_head_ptr;

    // Already read total payload size and deserializer so skip them.
    cursor += kPayloadMinHeaderSize;

    if constexpr (kIsCompiledFmt) {
      const char* args_payload_ptr = cursor;
      const usize args_payload_size = total_payload_size - kPayloadHeaderSize;
      using CompiledFormat = Format;
      std::apply(
          [&](auto&&... args) {
            fmt::format_to(std::back_inserter(*fmt_buf), CompiledFormat{},
                           args...);
          },
          decode_args(args_payload_ptr, args_payload_size));
    } else {
      const std::string_view fmt_view =
          *reinterpret_cast<const std::string_view*>(cursor);
      cursor += sizeof(fmt_view);
      const char* args_payload_ptr = cursor;
      const usize args_payload_size = total_payload_size - kPayloadHeaderSize;

      std::apply(
          [&](auto&&... args) {
            fmt::vformat_to(std::back_inserter(*fmt_buf), fmt_view,
                            fmt::make_format_args(args...));
          },
          decode_args(args_payload_ptr, args_payload_size));
    }
  }
};

}  // namespace logging
