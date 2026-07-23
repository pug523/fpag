// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/arg/help_formatter.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string>
#include <string_view>

#include "fmt/base.h"
#include "fmt/core.h"
#include "fmt/format.h"
#include "fpag/arg/arg.h"
#include "fpag/arg/parser.h"
#include "fpag/base/color_mode.h"
#include "fpag/base/numeric.h"
#include "fpag/base/style.h"

namespace arg {

namespace {

inline const char* s(const char* code, base::ColorMode mode) noexcept {
  return base::style_code(code, mode);
}

// Calculates visible length of option column.
usize get_option_spec_len(const Arg& arg) noexcept {
  usize len = 2;
  if (arg.short_name()) {
    len += 2;
    if (!arg.long_name().empty()) {
      len += 2;
    }
  } else {
    len += 4;
  }

  if (!arg.long_name().empty()) {
    len += 2 + arg.long_name().size();
  }

  if (!arg.is_flag()) {
    if (!arg.choices().empty()) {
      // calculates size of " <choice1|choice2>"
      len += 3;  // " " + "<" + ">"
      for (usize i = 0; i < arg.choices().size(); ++i) {
        if (i > 0) {
          len += 1;  // "|"
        }
        len += arg.choices()[i].size();
      }
    } else {
      // calculates size of " <value_name>"
      const std::string_view vname =
          arg.value_name().empty() ? "value" : arg.value_name();
      len += 3 + vname.size();  // " " + "<" + ">" + vname
    }
  }
  return len;
}

}  // namespace

std::string_view HelpFormatter::format(const Parser& parser,
                                       base::ColorMode color_mode) & {
  if (formatted_str_.empty()) {
    return reformat(parser, color_mode);
  }
  return formatted_str_;
}

void HelpFormatter::render_option_line(std::string_view opt_spec,
                                       usize visible_len,
                                       usize max_opt_width,
                                       std::string_view help_text,
                                       bool is_required,
                                       base::ColorMode c) {
  auto out = std::back_inserter(formatted_str_);

  // Option specification
  fmt::format_to(out, "{}{}{}", s(base::kBrightCyan, c), opt_spec,
                 s(base::kReset, c));

  if (visible_len < max_opt_width) {
    fmt::format_to(out, "{:>{}}", "", max_opt_width - visible_len);
  }
  fmt::format_to(out, "  ");

  // Description
  fmt::format_to(out, "{}", help_text);
  if (is_required) {
    fmt::format_to(out, " {}{}[required]{}", s(base::kGray, c),
                   s(base::kItalic, c), s(base::kReset, c));
  }
  fmt::format_to(out, "\n");
}

std::string_view HelpFormatter::reformat(const Parser& parser,
                                         base::ColorMode color_mode) & {
  formatted_str_.clear();

  constexpr usize kMargin = 512;
  constexpr usize kEstimatedStrLenPerArgs = 128;
  const usize estimated_size =
      kMargin + (parser.args().size() * kEstimatedStrLenPerArgs);
  formatted_str_.reserve(estimated_size);

  auto out = std::back_inserter(formatted_str_);

  // Title and about header
  constexpr usize kTerminalWidth = 60;

  if (!parser.name().empty()) {
    usize pad = (parser.name().size() < kTerminalWidth)
                    ? (kTerminalWidth - parser.name().size()) / 2
                    : 0;
    fmt::format_to(out, "{:>{}}{}{}{}\n\n", "", pad,
                   s(base::kBrightRed, color_mode), parser.name(),
                   s(base::kReset, color_mode));
  }

  if (!parser.about().empty()) {
    std::string full_about = fmt::format("=-=-= {} =-=-=", parser.about());
    usize pad = (full_about.size() < kTerminalWidth)
                    ? (kTerminalWidth - full_about.size()) / 2
                    : 0;
    fmt::format_to(out, "{:>{}}{}{}{}\n\n", "", pad,
                   s(base::kBrightYellow, color_mode), full_about,
                   s(base::kReset, color_mode));
  }

  // Usage and Options section
  fmt::format_to(out, "Usage: {}{}{} {}{}[Options]{}\n\nOptions:\n",
                 s(base::kBrightGreen, color_mode), parser.name(),
                 s(base::kReset, color_mode),
                 s(base::kBrightMagenta, color_mode),
                 s(base::kBold, color_mode), s(base::kReset, color_mode));

  // Determine alignment column width
  usize max_opt_width = 32;
  for (const auto& arg : parser.args()) {
    max_opt_width = std::max(max_opt_width, get_option_spec_len(arg));
  }
  max_opt_width = std::max(max_opt_width, static_cast<usize>(14));

  for (const auto& arg : parser.args()) {
    std::string opt_spec;
    opt_spec.reserve(64);

    opt_spec += "  ";
    if (arg.short_name()) {
      opt_spec += '-';
      opt_spec += *arg.short_name();
      if (!arg.long_name().empty()) {
        opt_spec += ", ";
      }
    } else {
      opt_spec += "    ";
    }

    if (!arg.long_name().empty()) {
      opt_spec += "--";
      opt_spec += arg.long_name();
    }

    if (!arg.is_flag()) {
      opt_spec += " <";
      if (!arg.choices().empty()) {
        for (usize i = 0; i < arg.choices().size(); ++i) {
          if (i > 0) {
            opt_spec += "|";
          }
          opt_spec += arg.choices()[i];
        }
      } else {
        opt_spec += arg.value_name().empty() ? "value" : arg.value_name();
      }
      opt_spec += ">";
    }

    std::string help_text(arg.help());
    if (!arg.default_value().empty()) {
      if (!help_text.empty()) {
        help_text += " ";
      }
      help_text += fmt::format("(default: {})", arg.default_value());
    }

    render_option_line(opt_spec, opt_spec.size(), max_opt_width, help_text,
                       arg.is_required(), color_mode);
  }

  // Built-in flags
  if (parser.builtin_enabled()) {
    render_option_line("  -h, --help", 12, max_opt_width,
                       "print this help message", false, color_mode);
    if (!parser.version().empty()) {
      render_option_line("  -v, --version", 15, max_opt_width, "print version",
                         false, color_mode);
    }
  }

  return formatted_str_;
}

}  // namespace arg
