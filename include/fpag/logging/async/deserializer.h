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
#include "fpag/str/string_interner.h"

namespace logging {

using DeserializeFunction = void (*)(const char* payload_head_ptr,
                                     usize total_payload_size,
                                     format_buffer* fmt_buf,
                                     const str::StringInterner* interner);

// For function pointer alignment
static constexpr usize kPayloadAlign = 8;
static constexpr usize kPayloadMinHeaderSize =
    sizeof(usize) + sizeof(DeserializeFunction) +
    base::round_up(sizeof(LogLevel), kPayloadAlign);

template <typename Format, bool kUseInterner, typename... Args>
class Deserializer {
 public:
  using DecodedArgs =
      std::tuple<typename Codec<std::decay_t<Args>>::DecodedType...>;

  static constexpr bool kIsCompiledFmt = fmt::is_compiled_string<Format>::value;
  static constexpr usize kPayloadHeaderSize =
      kPayloadMinHeaderSize + (kIsCompiledFmt ? 0 : sizeof(std::string_view));

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
          constexpr usize kBodySize = C::body_size();
          FPAG_DCHECK_LE(kBodySize, remaining_size);
          R arg = C::decode(data_cursor, kBodySize);
          data_cursor += kBodySize;
          remaining_size -= kBodySize;
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
                          format_buffer* fmt_buf,
                          const str::StringInterner* interner) {
    // Already read total payload size and deserializer so skip them.
    const char* const read_head = payload_head_ptr + kPayloadMinHeaderSize;

    if constexpr (kIsCompiledFmt) {
      using CompiledFormat = Format;

      const char* const args_head = read_head;
      const usize args_size = total_payload_size - kPayloadHeaderSize;

      std::apply(
          [&](auto&&... args) {
            fmt::format_to(std::back_inserter(*fmt_buf), CompiledFormat{},
                           args...);
          },
          decode_args(args_head, args_size));
    } else {
      std::string_view fmt_view;
      if constexpr (kUseInterner) {
        using StrId = str::StringInterner::StringId;
        const StrId id = *reinterpret_cast<const StrId*>(read_head);
        fmt_view = interner->get(id);
      } else {
        fmt_view = *reinterpret_cast<const std::string_view*>(read_head);
      }

      if constexpr (sizeof...(Args) == 0) {
        // Fast path, just do memcpy.
        const usize old_size = fmt_buf->size();
        fmt_buf->resize(old_size + fmt_view.size());

        std::memcpy(fmt_buf->data() + old_size, fmt_view.data(),
                    fmt_view.size());
      } else {
        const char* const args_head = read_head + sizeof(fmt_view);
        const usize args_size = total_payload_size - kPayloadHeaderSize;

        std::apply(
            [&](auto&&... args) {
              fmt::vformat_to(std::back_inserter(*fmt_buf), fmt_view,
                              fmt::make_format_args(args...));
            },
            decode_args(args_head, args_size));
      }
    }
  }
};

}  // namespace logging
