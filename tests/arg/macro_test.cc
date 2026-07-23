// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/arg/macro.h"

#include <string>
#include <utility>

#include "catch2/catch_test_macros.hpp"
#include "fpag/arg/matches.h"
#include "fpag/arg/parse_status.h"
#include "fpag/arg/parser.h"
#include "fpag/base/numeric.h"

namespace arg {

// NOLINTBEGIN(bugprone-use-after-move)

namespace {

struct Config {
  i32 port = 8080;
  std::string host = "127.0.0.1";
  bool verbose = false;
};
ARGS_FN_DEFINE(
    Config,
    parse_config,
    "app",
    "1.0.0",
    "Sample application",
    ARGS_OPT(Config, port, 'p', "port", "Port to listen on", false),
    ARGS_OPT(Config, host, 'h', "host", "Host address", false),
    ARGS_FLAG(Config, verbose, 'V', "verbose", "Enable verbose logging"))

struct AuthConfig {
  std::string token;
};
ARGS_FN_DEFINE(AuthConfig,
               parse_auth,
               "auth-app",
               "1.0.0",
               "Auth test app",
               ARGS_OPT(AuthConfig, token, 't', "token", "Auth token", true))

struct BundleConfig {
  bool a_flag = false;
  bool b_flag = false;
};
ARGS_FN_DEFINE(BundleConfig,
               parse_bundle,
               "bundle-app",
               "1.0.0",
               "Bundle test app",
               ARGS_FLAG(BundleConfig, a_flag, 'a', "aflag", "A flag"),
               ARGS_FLAG(BundleConfig, b_flag, 'b', "bflag", "B flag"))

struct ChoiceConfig {
  std::string mode = "read";
  i32 threads = 1;
};

// Choices expansion (__VA_ARGS__) and '\0' short names
ARGS_FN_DEFINE(ChoiceConfig,
               parse_choice,
               "choice-app",
               "2.0.0",
               "Choice test app",
               ARGS_OPT(ChoiceConfig,
                        mode,
                        'm',
                        "mode",
                        "Operation mode",
                        false,
                        "read",
                        "write"),
               ARGS_OPT(ChoiceConfig,
                        threads,
                        '\0',
                        "threads",
                        "Number of threads",
                        false))

struct FullConfig {
  std::string config_path = "chrb.toml";
  std::string dim = "overworld";
  i32 num_threads = 4;
  std::string required_src;
};

ARGS_FN_DEFINE(FullConfig,
               parse_full,
               "full-app",
               "1.0.0",
               "Full option test app",
               ARGS_OPT_FULL(FullConfig,
                             config_path,
                             '\0',
                             "config-path",
                             "chrb.toml",
                             "path",
                             "config file path",
                             false),
               ARGS_OPT_FULL(FullConfig,
                             dim,
                             'D',
                             "dim",
                             "overworld",
                             "",
                             "target dimension",
                             false,
                             "overworld",
                             "nether",
                             "end"),
               ARGS_OPT_FULL(FullConfig,
                             num_threads,
                             'j',
                             "threads",
                             "4",
                             "n",
                             "number of worker threads",
                             false),
               ARGS_OPT_FULL(FullConfig,
                             required_src,
                             's',
                             "src",
                             "",
                             "path",
                             "source path",
                             true))

}  // namespace

TEST_CASE("Macro argument parsing", "[arg][macro]") {
  SECTION("Default values when no flags or options are provided") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"app"};
    const i32 argc = static_cast<i32>(std::size(argv));

    auto res = parse_config(argc, argv);
    REQUIRE(res.is_ok());

    const Config& cfg = std::move(res).unwrap();
    CHECK(cfg.port == 8080);
    CHECK(cfg.host == "127.0.0.1");
    CHECK_FALSE(cfg.verbose);
  }

  SECTION("Parse options with short names") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"app", "-p", "9090", "-V"};
    const i32 argc = static_cast<i32>(std::size(argv));

    auto res = parse_config(argc, argv);
    REQUIRE(res.is_ok());

    const Config& cfg = std::move(res).unwrap();
    CHECK(cfg.port == 9090);
    CHECK(cfg.host == "127.0.0.1");
    CHECK(cfg.verbose);
  }

  SECTION("Parse options with long names") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"app",    "--port",  "3000",
                          "--host", "0.0.0.0", "--verbose"};
    const i32 argc = static_cast<i32>(std::size(argv));

    auto res = parse_config(argc, argv);
    REQUIRE(res.is_ok());

    const Config& cfg = std::move(res).unwrap();
    CHECK(cfg.port == 3000);
    CHECK(cfg.host == "0.0.0.0");
    CHECK(cfg.verbose);
  }

  SECTION("Long option using '=' syntax") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"app", "--port=3000", "--host=0.0.0.0"};
    const i32 argc = static_cast<i32>(std::size(argv));

    auto res = parse_config(argc, argv);
    REQUIRE(res.is_ok());

    const Config& cfg = std::move(res).unwrap();
    CHECK(cfg.port == 3000);
    CHECK(cfg.host == "0.0.0.0");
  }

  SECTION("Mixed short and long options") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"app", "-p", "5000", "--host=10.0.0.1", "-V"};
    const i32 argc = static_cast<i32>(std::size(argv));

    auto res = parse_config(argc, argv);
    REQUIRE(res.is_ok());

    const Config& cfg = std::move(res).unwrap();
    CHECK(cfg.port == 5000);
    CHECK(cfg.host == "10.0.0.1");
    CHECK(cfg.verbose);
  }

  SECTION("Only some options provided, rest keep defaults") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"app", "--host=192.168.1.1"};
    const i32 argc = static_cast<i32>(std::size(argv));

    auto res = parse_config(argc, argv);
    REQUIRE(res.is_ok());

    const Config& cfg = std::move(res).unwrap();
    CHECK(cfg.port == 8080);
    CHECK(cfg.host == "192.168.1.1");
    CHECK_FALSE(cfg.verbose);
  }

  SECTION("Unknown long option fails") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"app", "--bogus"};
    const i32 argc = static_cast<i32>(std::size(argv));

    auto res = parse_config(argc, argv);
    CHECK_FALSE(res.is_ok());
  }

  SECTION("Unknown short option fails") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"app", "-z"};
    const i32 argc = static_cast<i32>(std::size(argv));

    auto res = parse_config(argc, argv);
    CHECK_FALSE(res.is_ok());
  }

  SECTION("Missing value for long option fails") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"app", "--port"};
    const i32 argc = static_cast<i32>(std::size(argv));

    auto res = parse_config(argc, argv);
    CHECK_FALSE(res.is_ok());
  }

  SECTION("Missing value for short option fails") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"app", "-p"};
    const i32 argc = static_cast<i32>(std::size(argv));

    auto res = parse_config(argc, argv);
    CHECK_FALSE(res.is_ok());
  }

  SECTION("Help flag short-circuits to an error result") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"app", "--help"};
    const i32 argc = static_cast<i32>(std::size(argv));

    auto res = parse_config(argc, argv);
    CHECK_FALSE(res.is_ok());
  }
}

TEST_CASE("Macro required options and validation", "[arg][macro]") {
  SECTION("Fails when required option is missing") {
    struct RequiredConfig {
      std::string key;
    };
    auto binder = ARGS_OPT(RequiredConfig, key, 'k', "key", "API key", true);

    Parser cmd("test_app", "1.0.0");
    std::move(binder).apply_to_parser(&cmd);

    Matches matches;
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"test_app"};
    const i32 argc = static_cast<i32>(std::size(argv));

    const ParseStatus status = cmd.parse(argc, argv, &matches);
    CHECK(status == ParseStatus::Error);
  }

  SECTION("Fails end-to-end when required option is missing") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"auth-app"};
    const i32 argc = static_cast<i32>(std::size(argv));

    auto res = parse_auth(argc, argv);
    CHECK_FALSE(res.is_ok());
  }

  SECTION("Succeeds end-to-end when required option is provided (short)") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"auth-app", "-t", "secret123"};
    const i32 argc = static_cast<i32>(std::size(argv));

    auto res = parse_auth(argc, argv);
    REQUIRE(res.is_ok());
    CHECK(std::move(res).unwrap().token == "secret123");
  }

  SECTION("Succeeds end-to-end when required option is provided (long)") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"auth-app", "--token=secret456"};
    const i32 argc = static_cast<i32>(std::size(argv));

    auto res = parse_auth(argc, argv);
    REQUIRE(res.is_ok());
    CHECK(std::move(res).unwrap().token == "secret456");
  }
}

TEST_CASE("Macro bundled short flags", "[arg][macro]") {
  SECTION("Two boolean flags bundled under one dash both get set") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"bundle-app", "-ab"};
    const i32 argc = static_cast<i32>(std::size(argv));

    auto res = parse_bundle(argc, argv);
    REQUIRE(res.is_ok());

    const BundleConfig& cfg = std::move(res).unwrap();
    CHECK(cfg.a_flag);
    CHECK(cfg.b_flag);
  }

  SECTION("Only one of the bundled flags provided") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"bundle-app", "-a"};
    const i32 argc = static_cast<i32>(std::size(argv));

    auto res = parse_bundle(argc, argv);
    REQUIRE(res.is_ok());

    const BundleConfig& cfg = std::move(res).unwrap();
    CHECK(cfg.a_flag);
    CHECK_FALSE(cfg.b_flag);
  }
}

TEST_CASE("Macro handles built-in --help and --version requested statuses",
          "[arg][macro]") {
  SECTION("Help request returns HelpRequested status with help string") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"app", "--help"};
    const i32 argc = static_cast<i32>(std::size(argv));

    auto res = parse_config(argc, argv);
    CHECK(res.is_help());
    CHECK_FALSE(std::move(res).unwrap_help().empty());
  }

  SECTION("Short -h returns HelpRequested status") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"auth-app", "-h"};
    const i32 argc = static_cast<i32>(std::size(argv));

    auto res = parse_auth(argc, argv);
    CHECK(res.is_help());
  }

  SECTION(
      "Version request returns VersionRequested status with version string") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"app", "--version"};
    const i32 argc = static_cast<i32>(std::size(argv));

    auto res = parse_config(argc, argv);
    CHECK(res.is_version());
    CHECK(std::move(res).unwrap_version() == "1.0.0");
  }

  SECTION("Short -v returns VersionRequested status") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"app", "-v"};
    const i32 argc = static_cast<i32>(std::size(argv));

    auto res = parse_config(argc, argv);
    CHECK(res.is_version());
  }
}

TEST_CASE("Macro options with choices validation", "[arg][macro]") {
  SECTION("Valid choice succeeds") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"choice-app", "--mode", "write"};
    const i32 argc = static_cast<i32>(std::size(argv));

    auto res = parse_choice(argc, argv);
    REQUIRE(res.is_ok());
    CHECK(std::move(res).unwrap().mode == "write");
  }

  SECTION("Invalid choice fails parsing") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"choice-app", "--mode", "execute"};
    const i32 argc = static_cast<i32>(std::size(argv));

    auto res = parse_choice(argc, argv);
    CHECK(res.is_err());
  }

  SECTION("Option without short name ('\\0') works") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"choice-app", "--threads", "8"};
    const i32 argc = static_cast<i32>(std::size(argv));

    auto res = parse_choice(argc, argv);
    REQUIRE(res.is_ok());
    CHECK(std::move(res).unwrap().threads == 8);
  }
}

TEST_CASE("Macro ARGS_OPT_FULL tests", "[arg][macro]") {
  SECTION(
      "Parses with required argument and keeps non-overridden default values") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"full-app", "-s", "/path/to/world"};
    const i32 argc = static_cast<i32>(std::size(argv));

    auto res = parse_full(argc, argv);
    REQUIRE(res.is_ok());

    const FullConfig& cfg = std::move(res).unwrap();
    CHECK(cfg.required_src == "/path/to/world");
    CHECK(cfg.config_path == "chrb.toml");
    CHECK(cfg.dim == "overworld");
    CHECK(cfg.num_threads == 4);
  }

  SECTION("Overrides all values, custom value_name and choices") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"full-app", "--config-path", "custom.toml",
                          "-D",       "nether",        "-j",
                          "16",       "--src",         "/path/to/world"};
    const i32 argc = static_cast<i32>(std::size(argv));

    auto res = parse_full(argc, argv);
    REQUIRE(res.is_ok());

    const FullConfig& cfg = std::move(res).unwrap();
    CHECK(cfg.config_path == "custom.toml");
    CHECK(cfg.dim == "nether");
    CHECK(cfg.num_threads == 16);
    CHECK(cfg.required_src == "/path/to/world");
  }

  SECTION("Fails when invalid choice is provided for ARGS_OPT_FULL") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"full-app", "-s", "/path/to/world", "-D",
                          "invalid_dim"};
    const i32 argc = static_cast<i32>(std::size(argv));

    auto res = parse_full(argc, argv);
    CHECK(res.is_err());
  }

  SECTION("Fails when required option in ARGS_OPT_FULL is missing") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"full-app", "-D", "end"};
    const i32 argc = static_cast<i32>(std::size(argv));

    auto res = parse_full(argc, argv);
    CHECK(res.is_err());
  }

  SECTION("Help message formats value_name and choices properly") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"full-app", "--help"};
    const i32 argc = static_cast<i32>(std::size(argv));

    auto res = parse_full(argc, argv);
    REQUIRE(res.is_help());

    const std::string_view help = std::move(res).unwrap_help();
    // Verify value_name placeholder rendering
    CHECK(help.find("--config-path <path>") != std::string_view::npos);
    CHECK(help.find("-j, --threads <n>") != std::string_view::npos);
    // Verify choices list rendering
    CHECK(help.find("-D, --dim <overworld|nether|end>") !=
          std::string_view::npos);
  }
}

TEST_CASE("Macro type conversion failure leaves default value intact or fails",
          "[arg][macro]") {
  SECTION("Invalid type conversion (e.g. string to int) returns error") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"app", "--port", "invalid_number"};
    const i32 argc = static_cast<i32>(std::size(argv));

    auto res = parse_config(argc, argv);
    // Even if parser matches raw string, extraction will fall back safely
    if (res.is_ok()) {
      // Port should remain default if extraction fails
      CHECK(std::move(res).unwrap().port == 8080);
    } else {
      CHECK(res.is_err());
    }
  }
}

// NOLINTEND(bugprone-use-after-move)

}  // namespace arg
