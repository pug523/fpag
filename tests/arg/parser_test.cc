// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/arg/parser.h"

#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "fpag/arg/arg.h"
#include "fpag/arg/command.h"
#include "fpag/arg/error_code.h"
#include "fpag/arg/matches.h"
#include "fpag/arg/parse_status.h"

namespace arg {

TEST_CASE("Parser returns NullMatchesPointer on null matches input",
          "[arg][parser]") {
  Parser parser(CommandBuilder("app", "1.0.0").build());

  SECTION("Null matches with span overload") {
    const std::string_view args[] = {"app"};
    const ParseStatus status = parser.parse(args, nullptr);
    CHECK(status == ParseStatus::Error);
    REQUIRE(parser.errors().size() == 1);
    CHECK(parser.errors()[0].code == ErrorCode::NullMatchesPointer);
  }

  SECTION("Null matches with argc/argv overload") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"app"};
    const ParseStatus status = parser.parse(1, argv, nullptr);
    CHECK(status == ParseStatus::Error);
    REQUIRE(parser.errors().size() == 1);
    CHECK(parser.errors()[0].code == ErrorCode::NullMatchesPointer);
  }

  SECTION("Null matches with parse_partial overload") {
    std::vector<std::string_view> unparsed;
    const std::string_view args[] = {"app"};
    const ParseStatus status = parser.parse_partial(args, nullptr, &unparsed);
    CHECK(status == ParseStatus::Error);
    REQUIRE(parser.errors().size() == 1);
    CHECK(parser.errors()[0].code == ErrorCode::NullMatchesPointer);
  }
}

TEST_CASE("Parser handles raw C-style argc/argv interface", "[arg][parser]") {
  Parser parser(CommandBuilder("app", "1.0.0")
                    .add_arg(ArgBuilder("port").short_name('p').build())
                    .build());

  SECTION("Valid argc and argv pointers") {
    Matches matches;
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"app", "-p", "8080"};
    const ParseStatus status = parser.parse(3, argv, &matches);

    REQUIRE(status == ParseStatus::Success);
    CHECK(matches.get<std::string_view>("port").unwrap() == "8080");
  }

  SECTION("Fails on invalid argc <= 0") {
    Matches matches;
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"app"};

    const ParseStatus status_zero = parser.parse(0, argv, &matches);
    CHECK(status_zero == ParseStatus::Error);
    REQUIRE_FALSE(parser.errors().empty());
    CHECK(parser.errors()[0].code == ErrorCode::InvalidArgCount);

    const ParseStatus status_neg = parser.parse(-1, argv, &matches);
    CHECK(status_neg == ParseStatus::Error);
    REQUIRE_FALSE(parser.errors().empty());
    CHECK(parser.errors()[0].code == ErrorCode::InvalidArgCount);
  }

  SECTION("parse_partial with invalid argc <= 0") {
    Matches matches;
    std::vector<std::string_view> unparsed;
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"app"};

    const ParseStatus status =
        parser.parse_partial(0, argv, &matches, &unparsed);
    CHECK(status == ParseStatus::Error);
    CHECK(parser.errors()[0].code == ErrorCode::InvalidArgCount);
  }
}

TEST_CASE("Parser supports std::span, vector, and array literals",
          "[arg][parser]") {
  Parser parser(
      CommandBuilder("app", "1.0.0")
          .add_arg(
              ArgBuilder("config").long_name("config").short_name('c').build())
          .build());

  SECTION("Explicit std::span<const std::string_view>") {
    Matches matches;
    const std::string_view raw_args[] = {"app", "--config", "sys.json"};
    const std::span<const std::string_view> span_args(raw_args);

    const ParseStatus status = parser.parse(span_args, &matches);
    REQUIRE(status == ParseStatus::Success);
    CHECK(matches.get<std::string_view>("config").unwrap() == "sys.json");
  }

  SECTION("Implicit conversion from std::vector<std::string_view>") {
    Matches matches;
    const std::vector<std::string_view> vec_args = {"app", "-c", "vec.json"};

    const ParseStatus status = parser.parse(vec_args, &matches);
    REQUIRE(status == ParseStatus::Success);
    CHECK(matches.get<std::string_view>("config").unwrap() == "vec.json");
  }

  SECTION("Fixed-size array overload for parse and parse_partial") {
    Matches matches;
    const std::string_view arr_args[] = {"app", "-c", "arr.json"};

    ParseStatus status = parser.parse(arr_args, &matches);
    REQUIRE(status == ParseStatus::Success);
    CHECK(matches.get<std::string_view>("config").unwrap() == "arr.json");

    Matches partial_matches;
    std::vector<std::string_view> unparsed;
    status = parser.parse_partial(arr_args, &partial_matches, &unparsed);
    REQUIRE(status == ParseStatus::Success);
    CHECK(partial_matches.get<std::string_view>("config").unwrap() ==
          "arr.json");
  }
}

TEST_CASE("Parser parses long options and short options", "[arg][parser]") {
  Parser parser(CommandBuilder("app", "1.0.0")
                    .add_arg(ArgBuilder("port")
                                 .short_name('p')
                                 .long_name("port")
                                 .help("Port number")
                                 .build())
                    .add_arg(ArgBuilder("host")
                                 .short_name('h')
                                 .long_name("host")
                                 .help("Host address")
                                 .build())
                    .build());

  SECTION("Separate arguments for short options") {
    Matches matches;
    const std::string_view args[] = {"app", "-p", "8080", "-h", "localhost"};

    const ParseStatus status = parser.parse(args, &matches);
    REQUIRE(status == ParseStatus::Success);
    CHECK(matches.get<std::string_view>("port").unwrap() == "8080");
    CHECK(matches.get<std::string_view>("host").unwrap() == "localhost");
  }

  SECTION("Inline short options (-p8080)") {
    Matches matches;
    const std::string_view args[] = {"app", "-p8080"};

    const ParseStatus status = parser.parse(args, &matches);
    REQUIRE(status == ParseStatus::Success);
    CHECK(matches.get<std::string_view>("port").unwrap() == "8080");
  }

  SECTION("Long options with '=' syntax") {
    Matches matches;
    const std::string_view args[] = {"app", "--port=9000", "--host=127.0.0.1"};

    const ParseStatus status = parser.parse(args, &matches);
    REQUIRE(status == ParseStatus::Success);
    CHECK(matches.get<std::string_view>("port").unwrap() == "9000");
    CHECK(matches.get<std::string_view>("host").unwrap() == "127.0.0.1");
  }

  SECTION("Ignores empty elements in arguments") {
    Matches matches;
    const std::string_view args[] = {"app", "", "--port", "8080", ""};

    const ParseStatus status = parser.parse(args, &matches);
    REQUIRE(status == ParseStatus::Success);
    CHECK(matches.get<std::string_view>("port").unwrap() == "8080");
  }
}

TEST_CASE("Parser handles boolean flags and flag bundling", "[arg][parser]") {
  Parser parser(
      CommandBuilder("app", "1.0.0")
          .add_arg(ArgBuilder("verbose")
                       .short_name('v')
                       .long_name("verbose")
                       .is_flag()
                       .build())
          .add_arg(ArgBuilder("all").short_name('a').is_flag().build())
          .add_arg(ArgBuilder("force").short_name('f').is_flag().build())
          .build());

  SECTION("Bundled short flags (-vaf)") {
    Matches matches;
    const std::string_view args[] = {"app", "-vaf"};

    const ParseStatus status = parser.parse(args, &matches);
    REQUIRE(status == ParseStatus::Success);
    CHECK(matches.has("verbose"));
    CHECK(matches.has("all"));
    CHECK(matches.has("force"));
  }

  SECTION("Error if a flag is given a value using '='") {
    Matches matches;
    const std::string_view args[] = {"app", "--verbose=true"};

    const ParseStatus status = parser.parse(args, &matches);
    CHECK(status == ParseStatus::Error);
    REQUIRE_FALSE(parser.errors().empty());
    CHECK(parser.errors()[0].code == ErrorCode::FlagTakesNoValue);
  }
}

TEST_CASE("Parser parse_partial feature", "[arg][parser]") {
  Parser parser(
      CommandBuilder("app", "1.0.0")
          .add_arg(
              ArgBuilder("output").short_name('o').long_name("output").build())
          .add_arg(ArgBuilder("verbose").short_name('v').is_flag().build())
          .build());

  SECTION("Collects unknown long/short flags and positionals into unparsed") {
    Matches matches;
    std::vector<std::string_view> unparsed;
    const std::string_view args[] = {"app", "--unknown-long", "-o", "out.txt",
                                     "-z",  "pos1",           "-v"};

    const ParseStatus status = parser.parse_partial(args, &matches, &unparsed);

    REQUIRE(status == ParseStatus::Success);
    CHECK(matches.get<std::string_view>("output").unwrap() == "out.txt");
    CHECK(matches.has("verbose"));

    REQUIRE(unparsed.size() == 3);
    CHECK(unparsed[0] == "--unknown-long");
    CHECK(unparsed[1] == "-z");
    CHECK(unparsed[2] == "pos1");
  }

  SECTION("Passes unparsed vector as nullptr safely") {
    Matches matches;
    const std::string_view args[] = {"app", "--unknown", "-o", "out.txt"};

    const ParseStatus status = parser.parse_partial(args, &matches, nullptr);
    REQUIRE(status == ParseStatus::Success);
    CHECK(matches.get<std::string_view>("output").unwrap() == "out.txt");
  }

  SECTION("Preserves double-dash and remaining args in partial mode") {
    Matches matches;
    std::vector<std::string_view> unparsed;
    const std::string_view args[] = {"app", "-v", "--", "rest1", "rest2"};

    const ParseStatus status = parser.parse_partial(args, &matches, &unparsed);
    REQUIRE(status == ParseStatus::Success);
    CHECK(matches.has("verbose"));

    REQUIRE(unparsed.size() == 3);
    CHECK(unparsed[0] == "--");
    CHECK(unparsed[1] == "rest1");
    CHECK(unparsed[2] == "rest2");
  }
}

TEST_CASE("Parser positionals accumulation and double dash", "[arg][parser]") {
  Parser parser(CommandBuilder("app", "1.0.0")
                    .add_arg(ArgBuilder("output").short_name('o').build())
                    .build());

  SECTION("Interleaved positionals and flags") {
    Matches matches;
    const std::string_view args[] = {"app", "input1.txt", "-o", "out.txt",
                                     "input2.txt"};

    const ParseStatus status = parser.parse(args, &matches);
    REQUIRE(status == ParseStatus::Success);
    CHECK(matches.get<std::string_view>("output").unwrap() == "out.txt");

    const auto& pos = matches.positionals();
    REQUIRE(pos.size() == 2);
    CHECK(pos[0] == "input1.txt");
    CHECK(pos[1] == "input2.txt");
  }

  SECTION("Double dash (--) forces remaining args as positionals") {
    Matches matches;
    const std::string_view args[] = {"app", "--", "-o", "not_an_option.txt"};

    const ParseStatus status = parser.parse(args, &matches);
    REQUIRE(status == ParseStatus::Success);
    CHECK_FALSE(matches.has("output"));

    const auto& pos = matches.positionals();
    REQUIRE(pos.size() == 2);
    CHECK(pos[0] == "-o");
    CHECK(pos[1] == "not_an_option.txt");
  }
}

TEST_CASE("Parser handles choices restriction", "[arg][parser]") {
  Parser parser(CommandBuilder("app", "1.0.0")
                    .add_arg(ArgBuilder("mode")
                                 .long_name("mode")
                                 .short_name('m')
                                 .choices({"fast", "slow", "balanced"})
                                 .build())
                    .build());

  SECTION("Valid choice passes") {
    Matches matches;
    const std::string_view args[] = {"app", "--mode", "fast"};

    const ParseStatus status = parser.parse(args, &matches);
    REQUIRE(status == ParseStatus::Success);
    CHECK(matches.get<std::string_view>("mode").unwrap() == "fast");
  }

  SECTION("Invalid choice fails") {
    Matches matches;
    const std::string_view args[] = {"app", "--mode", "ultra"};

    const ParseStatus status = parser.parse(args, &matches);
    CHECK(status == ParseStatus::Error);
    REQUIRE_FALSE(parser.errors().empty());
    CHECK(parser.errors()[0].code == ErrorCode::InvalidChoice);
  }
}

TEST_CASE("Parser required arguments verification", "[arg][parser]") {
  Parser parser(CommandBuilder("app", "1.0.0")
                    .add_arg(ArgBuilder("config")
                                 .long_name("config")
                                 .short_name('c')
                                 .required(true)
                                 .build())
                    .build());

  SECTION("Fails when required arg is absent") {
    Matches matches;
    const std::string_view args[] = {"app"};

    const ParseStatus status = parser.parse(args, &matches);
    CHECK(status == ParseStatus::Error);
    REQUIRE_FALSE(parser.errors().empty());
    CHECK(parser.errors()[0].code == ErrorCode::MissingRequiredArgument);
  }

  SECTION("Succeeds when required arg is present") {
    Matches matches;
    const std::string_view args[] = {"app", "-c", "settings.json"};

    const ParseStatus status = parser.parse(args, &matches);
    REQUIRE(status == ParseStatus::Success);
    CHECK(matches.get<std::string_view>("config").unwrap() == "settings.json");
  }
}

TEST_CASE("Parser builtin help and version triggers", "[arg][parser]") {
  Parser parser(CommandBuilder("app", "1.0.0").about("My App").build());

  SECTION("Trigger builtin help (--help) and help_message") {
    Matches matches;
    const std::string_view args[] = {"app", "--help"};

    const ParseStatus status = parser.parse(args, &matches);
    CHECK(status == ParseStatus::HelpRequested);
    CHECK_FALSE(parser.help_message().empty());
  }

  SECTION("Trigger builtin help (-h)") {
    Matches matches;
    const std::string_view args[] = {"app", "-h"};

    const ParseStatus status = parser.parse(args, &matches);
    CHECK(status == ParseStatus::HelpRequested);
  }

  SECTION("Trigger builtin version (--version)") {
    Matches matches;
    const std::string_view args[] = {"app", "--version"};

    const ParseStatus status = parser.parse(args, &matches);
    CHECK(status == ParseStatus::VersionRequested);
    CHECK(parser.root_command().version() == "1.0.0");
  }

  SECTION("Trigger builtin version (-v)") {
    Matches matches;
    const std::string_view args[] = {"app", "-v"};

    const ParseStatus status = parser.parse(args, &matches);
    CHECK(status == ParseStatus::VersionRequested);
  }

  SECTION("Trigger builtin help (--help) and rvalue help_message") {
    Matches matches;
    const std::string_view args[] = {"app", "--help"};

    const ParseStatus status = parser.parse(args, &matches);
    CHECK(status == ParseStatus::HelpRequested);
    CHECK_FALSE(std::move(parser).help_message().empty());
  }

  SECTION("Disabled builtins treat --help as unknown option") {
    Parser custom_parser(
        CommandBuilder("app", "1.0.0").builtin_enabled(false).build());
    Matches matches;
    const std::string_view args[] = {"app", "--help"};

    const ParseStatus status = custom_parser.parse(args, &matches);
    CHECK(status == ParseStatus::Error);
    REQUIRE_FALSE(custom_parser.errors().empty());
    CHECK(custom_parser.errors()[0].code == ErrorCode::UnknownLongOption);

    // Test rvalue and lvalue error_message
    CHECK_FALSE(custom_parser.error_message().empty());
    CHECK_FALSE(std::move(custom_parser).error_message().empty());
  }
}

TEST_CASE("Parser reports errors for unknown and missing options",
          "[arg][parser]") {
  Parser parser(CommandBuilder("app", "1.0.0")
                    .add_arg(ArgBuilder("file").short_name('f').build())
                    .build());

  SECTION("Unknown long option") {
    Matches matches;
    const std::string_view args[] = {"app", "--unknown"};

    const ParseStatus status = parser.parse(args, &matches);
    CHECK(status == ParseStatus::Error);
    REQUIRE_FALSE(parser.errors().empty());
    CHECK(parser.errors()[0].code == ErrorCode::UnknownLongOption);
  }

  SECTION("Unknown short option") {
    Matches matches;
    const std::string_view args[] = {"app", "-z"};

    const ParseStatus status = parser.parse(args, &matches);
    CHECK(status == ParseStatus::Error);
    REQUIRE_FALSE(parser.errors().empty());
    CHECK(parser.errors()[0].code == ErrorCode::UnknownShortOption);
  }

  SECTION("Missing value at end of args (long option)") {
    Parser p2(CommandBuilder("app", "1.0.0")
                  .add_arg(ArgBuilder("file").long_name("file").build())
                  .build());
    Matches matches;
    const std::string_view args[] = {"app", "--file"};

    const ParseStatus status = p2.parse(args, &matches);
    CHECK(status == ParseStatus::Error);
    REQUIRE_FALSE(p2.errors().empty());
    CHECK(p2.errors()[0].code == ErrorCode::MissingValueForOption);
  }

  SECTION("Missing value at end of args (short option)") {
    Matches matches;
    const std::string_view args[] = {"app", "-f"};

    const ParseStatus status = parser.parse(args, &matches);
    CHECK(status == ParseStatus::Error);
    REQUIRE_FALSE(parser.errors().empty());
    CHECK(parser.errors()[0].code == ErrorCode::MissingValueForOption);
  }
}

}  // namespace arg
