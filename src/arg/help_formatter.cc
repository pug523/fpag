// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/arg/help_formatter.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "fmt/base.h"
#include "fmt/core.h"
#include "fpag/arg/arg.h"
#include "fpag/arg/command.h"
#include "fpag/base/numeric.h"
#include "fpag/term/color_style.h"
#include "fpag/term/style.h"

namespace arg {

namespace {

// Formats option flags/arguments cleanly into a buffer
// (e.g., "  -c, --config <file>")
std::string format_option_spec(const Arg& arg) {
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
          opt_spec += '|';
        }
        opt_spec += arg.choices()[i];
      }
    } else {
      opt_spec += arg.value_name().empty() ? "value" : arg.value_name();
    }
    opt_spec += '>';
  }

  return opt_spec;
}

void render_option_line(std::back_insert_iterator<std::string> out,
                        std::string_view opt_spec,
                        usize max_opt_width,
                        std::string_view help_text,
                        std::string_view default_value,
                        bool is_required,
                        term::ColorStyle color_style) {
  const char* italic = term::style_code(term::kItalic, color_style);
  const char* reset = term::style_code(term::kReset, color_style);
  const char* bright_cyan = term::style_code(term::kBrightCyan, color_style);
  const char* gray = term::style_code(term::kGray, color_style);

  // Option specification with padding
  const usize visible_len = opt_spec.size();
  fmt::format_to(out, "{}{}{}", bright_cyan, opt_spec, reset);

  if (visible_len < max_opt_width) {
    fmt::format_to(out, "{:>{}}", "", max_opt_width - visible_len);
  }
  fmt::format_to(out, "  ");

  // Description
  fmt::format_to(out, "{}", help_text);
  if (!default_value.empty()) {
    fmt::format_to(out, " (default: {})", default_value);
  }
  if (is_required) {
    fmt::format_to(out, " {}{}[required]{}", gray, italic, reset);
  }
  fmt::format_to(out, "\n");
}

}  // namespace

std::string DefaultHelpFormatter::operator()(
    const Command& command,
    term::ColorStyle color_style) const {
  std::string result;

  constexpr usize kMargin = 512;
  constexpr usize kEstimatedStrLenPerArgs = 128;
  const usize estimated_size =
      kMargin + (command.args().size() * kEstimatedStrLenPerArgs);
  result.reserve(estimated_size);

  auto out = std::back_inserter(result);

  constexpr usize kTerminalWidth = 60;

  const char* bold = term::style_code(term::kBold, color_style);
  const char* underline = term::style_code(term::kUnderline, color_style);
  const char* reset = term::style_code(term::kReset, color_style);
  const char* blue = term::style_code(term::kBlue, color_style);
  const char* bright_magenta =
      term::style_code(term::kBrightMagenta, color_style);

  // Title section
  if (!command.name().empty()) {
    usize pad = (command.name().size() < kTerminalWidth)
                    ? (kTerminalWidth - command.name().size()) / 2
                    : 0;
    fmt::format_to(out, "{:>{}}{}{}{}{}\n\n", "", pad, bold, underline,
                   command.name(), reset);
  }

  // About section
  if (!command.about().empty()) {
    std::string_view full_about = command.about();
    usize pad = (full_about.size() < kTerminalWidth)
                    ? (kTerminalWidth - full_about.size()) / 2
                    : 0;
    fmt::format_to(out, "{:>{}}{}{}{}\n\n", "", pad, blue, full_about, reset);
  }

  // Usage section
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

  // Calculate option alignment column width
  usize max_opt_width = kMinDescriptionMargin;
  std::vector<std::string> opt_specs;
  opt_specs.reserve(command.args().size());

  for (const auto& arg : command.args()) {
    opt_specs.push_back(format_option_spec(arg));
    max_opt_width = std::max(max_opt_width, opt_specs.back().size());
  }

  // Render user options
  for (usize i = 0; i < command.args().size(); ++i) {
    const auto& arg = command.args()[i];
    render_option_line(out, opt_specs[i], max_opt_width, arg.help(),
                       arg.default_value(), arg.is_required(), color_style);
  }

  // Render built-in options
  if (command.builtin_enabled()) {
    render_option_line(out, "  -h, --help", max_opt_width, "Print help message",
                       "", false, color_style);
    if (!command.version().empty()) {
      render_option_line(out, "  -v, --version", max_opt_width,
                         "Print version information", "", false, color_style);
    }
  }

  return result;
}

}  // namespace arg
