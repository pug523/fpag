// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/arg/arg.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "fpag/arg/error_code.h"
#include "fpag/arg/error_formatter.h"
#include "fpag/arg/parse_error.h"
#include "fpag/term/color_style.h"

namespace arg {

namespace {

// Whether every style a line opens is closed before the line ends. A style
// that survives its line paints whatever comes after it.
bool styles_balanced(std::string_view text) {
  isize open = 0;
  usize i = 0;
  while (i < text.size()) {
    if (text[i] == '\n') {
      if (open != 0) {
        return false;
      }
      ++i;
      continue;
    }
    if (text[i] == '\x1b' && i + 1 < text.size() && text[i + 1] == '[') {
      const usize end = text.find('m', i + 2);
      if (end == std::string_view::npos) {
        return false;
      }
      const std::string_view parameters = text.substr(i + 2, end - i - 2);
      // One `0` closes every attribute, so it clears the count rather than
      // closing one of them.
      open = parameters == "0" ? 0 : open + 1;
      i = end + 1;
      continue;
    }
    ++i;
  }
  return open == 0;
}

}  // namespace

// The message is bold on purpose, and the line has to turn it off: leaving
// it on painted the hint under the errors and anything printed after.
TEST_CASE("The error formatter closes every style it opens",
          "[arg][error]") {
  const std::vector<ParseError> errors = {
      ParseError{ErrorCode::UnknownLongOption, "--frobnicator"},
  };
  const std::string text = DefaultErrorFormatter{}.operator()(
      "demo", errors, term::ColorStyle::Ansi16);

  CHECK(text.find("\x1b[") != std::string::npos);
  CHECK(styles_balanced(text));
}

TEST_CASE("Arg default state", "[arg][arg]") {
  const Arg a = ArgBuilder("port").build();

  CHECK(a.name() == "port");
  CHECK_FALSE(a.short_name().has_value());
  CHECK(a.long_name() == "port");
  CHECK(a.help().empty());
  CHECK_FALSE(a.is_required());
  CHECK_FALSE(a.is_flag());
}

TEST_CASE("ArgBuilder fluent API on an rvalue chains all fields",
          "[arg][arg]") {
  const Arg a = ArgBuilder("port")
                    .short_name('p')
                    .long_name("port")
                    .help("Port to listen on")
                    .required(true)
                    .is_flag(false)
                    .build();

  CHECK(a.name() == "port");
  std::optional<char> s = a.short_name();
  REQUIRE(s.has_value());
  CHECK(*s == 'p');  // NOLINT(bugprone-unchecked-optional-access)
  CHECK(a.long_name() == "port");
  CHECK(a.help() == "Port to listen on");
  CHECK(a.is_required());
  CHECK_FALSE(a.is_flag());
}

TEST_CASE("ArgBuilder fluent API on a named lvalue also chains", "[arg][arg]") {
  ArgBuilder b("verbose");
  const Arg a = std::move(b.short_name('v')
                              .long_name("verbose")
                              .help("Enable verbose")
                              .is_flag(true))
                    .build();

  CHECK(a.name() == "verbose");
  std::optional<char> s = a.short_name();
  REQUIRE(s.has_value());
  CHECK(*s == 'v');  // NOLINT(bugprone-unchecked-optional-access)
  CHECK(a.long_name() == "verbose");
  CHECK(a.is_flag());
  // required() never called -> stays at default
  CHECK_FALSE(a.is_required());
}

TEST_CASE("ArgBuilder required() defaults to true when called with no argument",
          "[arg][arg]") {
  const Arg a = ArgBuilder("key").required().build();
  CHECK(a.is_required());
}

TEST_CASE("ArgBuilder is_flag() defaults to true when called with no argument",
          "[arg][arg]") {
  const Arg a = ArgBuilder("debug").is_flag().build();
  CHECK(a.is_flag());
}

TEST_CASE("ArgBuilder required(false) explicitly clears the flag",
          "[arg][arg]") {
  const Arg a = ArgBuilder("key").required(true).required(false).build();
  CHECK_FALSE(a.is_required());
}

TEST_CASE("Arg move construction preserves all fields", "[arg][arg]") {
  Arg a = ArgBuilder("host")
              .short_name('h')
              .long_name("host")
              .required(true)
              .build();
  const Arg b = std::move(a);

  CHECK(b.name() == "host");
  std::optional<char> s = b.short_name();
  REQUIRE(s.has_value());
  CHECK(*s == 'h');  // NOLINT(bugprone-unchecked-optional-access)
  CHECK(b.long_name() == "host");
  CHECK(b.is_required());
}

TEST_CASE("ArgBuilder name() respects explicit name override on rvalue",
          "[arg][arg]") {
  // If 'name' is provided, it should be used instead of long_name
  const Arg a = std::move(ArgBuilder("long-name").name("custom-name")).build();
  CHECK(a.name() == "custom-name");
  CHECK(a.long_name() == "long-name");
}

TEST_CASE("ArgBuilder name() respects explicit name override on lvalue",
          "[arg][arg]") {
  ArgBuilder b("long-name");
  b.name("custom-name");
  const Arg a = std::move(b).build();

  CHECK(a.name() == "custom-name");
  CHECK(a.long_name() == "long-name");
}

TEST_CASE("Arg default_value defaults to empty", "[arg][arg]") {
  const Arg a = ArgBuilder("port").build();
  CHECK(a.default_value().empty());
}

TEST_CASE("ArgBuilder allows setting default_value", "[arg][arg]") {
  const Arg a = ArgBuilder("port").default_value("8080").build();
  CHECK(a.default_value() == "8080");
}

TEST_CASE("Arg choices are stored and retrievable", "[arg][arg]") {
  const std::string modes[] = {"read", "write", "execute"};
  const Arg a = ArgBuilder("mode").choices(modes).build();

  const std::span<const std::string> choices = a.choices();
  REQUIRE(choices.size() == 3);
  CHECK(choices[0] == modes[0]);
  CHECK(choices[1] == modes[1]);
  CHECK(choices[2] == modes[2]);
}

TEST_CASE("Arg choices default to empty span", "[arg][arg]") {
  const Arg a = ArgBuilder("port").build();
  CHECK(a.choices().empty());
}

TEST_CASE("Arg fluent builder supports chaining choices", "[arg][arg]") {
  const std::string choices[] = {"info", "warn", "error"};
  const Arg a = ArgBuilder("level").choices(choices).required().build();

  CHECK(a.choices().size() == 3);
  CHECK(a.is_required());
}

}  // namespace arg
