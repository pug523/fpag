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
#include "fpag/arg/command.h"
#include "fpag/base/color_style.h"
#include "fpag/base/numeric.h"
#include "fpag/base/style.h"

namespace arg {

namespace {

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

void render_option_line(const std::back_insert_iterator<std::string> out,
                        std::string_view opt_spec,
                        usize visible_len,
                        usize max_opt_width,
                        std::string_view help_text,
                        bool is_required,
                        base::ColorStyle color_style) {
  const char* italic = base::style_code(base::kItalic, color_style);
  const char* reset = base::style_code(base::kReset, color_style);
  const char* bright_cyan = base::style_code(base::kBrightCyan, color_style);
  const char* gray = base::style_code(base::kGray, color_style);

  // Option specification
  fmt::format_to(out, "{}{}{}", bright_cyan, opt_spec, reset);

  if (visible_len < max_opt_width) {
    fmt::format_to(out, "{:>{}}", "", max_opt_width - visible_len);
  }
  fmt::format_to(out, "  ");

  // Description
  fmt::format_to(out, "{}", help_text);
  if (is_required) {
    fmt::format_to(out, " {}{}[required]{}", gray, italic, reset);
  }
  fmt::format_to(out, "\n");
}

}  // namespace

std::string DefaultHelpFormatter::format(const Command& command,
                                         base::ColorStyle color_style) const {
  std::string result;

  constexpr usize kMargin = 512;
  constexpr usize kEstimatedStrLenPerArgs = 128;
  const usize estimated_size =
      kMargin + (command.args().size() * kEstimatedStrLenPerArgs);
  result.reserve(estimated_size);

  auto out = std::back_inserter(result);

  // Title and about header
  constexpr usize kTerminalWidth = 60;

  const char* bold = base::style_code(base::kBold, color_style);
  const char* underline = base::style_code(base::kUnderline, color_style);
  const char* reset = base::style_code(base::kReset, color_style);

  const char* blue = base::style_code(base::kBlue, color_style);
  const char* bright_magenta =
      base::style_code(base::kBrightMagenta, color_style);

  if (!command.name().empty()) {
    usize pad = (command.name().size() < kTerminalWidth)
                    ? (kTerminalWidth - command.name().size()) / 2
                    : 0;
    fmt::format_to(out, "{:>{}}{}{}{}{}\n\n", "", pad, bold, underline,
                   command.name(), reset);
  }

  if (!command.about().empty()) {
    std::string_view full_about = command.about();
    usize pad = (full_about.size() < kTerminalWidth)
                    ? (kTerminalWidth - full_about.size()) / 2
                    : 0;
    fmt::format_to(out, "{:>{}}{}{}{}\n\n", "", pad, blue, full_about, reset);
  }

  // Usage, Commands, and Options section
  constexpr usize kMinDescriptionMargin = 20;
  if (!command.subcommands().empty()) {
    fmt::format_to(out, "{}{}Usage{}: {}{}{} {}{}[Options]{} {}{}[Command]{}\n",
                   bold, underline, reset, bold, command.name(), reset,
                   bright_magenta, bold, reset, bright_magenta, bold, reset);
    fmt::format_to(out, "\n{}{}Commands{}:\n", bold, underline, reset);
    usize max_command_width = kMinDescriptionMargin;
    for (const Command& sub : command.subcommands()) {
      max_command_width = std::max(max_command_width, sub.name().length());
    }

    for (const Command& sub : command.subcommands()) {
      fmt::format_to(out, "  {:<{}}  {}\n", sub.name(), max_command_width,
                     sub.about());
    }
  } else {
    fmt::format_to(out, "{}{}Usage{}: {}{}{} {}{}[Options]{}\n", bold,
                   underline, reset, bold, command.name(), reset,
                   bright_magenta, bold, reset);
  }
  fmt::format_to(out, "\n{}{}Options{}:\n", bold, underline, reset);

  // Determine alignment column width
  usize max_opt_width = kMinDescriptionMargin;
  for (const auto& arg : command.args()) {
    max_opt_width = std::max(max_opt_width, get_option_spec_len(arg));
  }

  for (const auto& arg : command.args()) {
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

    if (!arg.default_value().empty()) {
      const std::string help =
          fmt::format("{} (default: {})", arg.help(), arg.default_value());
      render_option_line(out, opt_spec, opt_spec.size(), max_opt_width, help,
                         arg.is_required(), color_style);
    } else {
      render_option_line(out, opt_spec, opt_spec.size(), max_opt_width,
                         arg.help(), arg.is_required(), color_style);
    }
  }

  // Built-in flags
  if (command.builtin_enabled()) {
    render_option_line(out, "  -h, --help", 12, max_opt_width,
                       "print this help message", false, color_style);
    if (!command.version().empty()) {
      render_option_line(out, "  -v, --version", 15, max_opt_width,
                         "print version", false, color_style);
    }
  }

  return result;
}

}  // namespace arg
