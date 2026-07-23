// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/arg/macro.h"

#include <string>
#include <utility>

#include "catch2/catch_test_macros.hpp"
#include "fpag/arg/command.h"
#include "fpag/arg/matches.h"
#include "fpag/base/numeric.h"

namespace arg {

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

}  // namespace

TEST_CASE("Macro argument parsing", "[arg][macro]") {
  SECTION("Default values when no flags or options are provided") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"app"};
    const i32 argc = static_cast<i32>(std::size(argv));

    const auto res = parse_config(argc, argv);
    REQUIRE(res.is_ok());

    const Config& cfg = res.unwrap();
    CHECK(cfg.port == 8080);
    CHECK(cfg.host == "127.0.0.1");
    CHECK_FALSE(cfg.verbose);
  }

  SECTION("Parse options with short names") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"app", "-p", "9090", "-V"};
    const i32 argc = static_cast<i32>(std::size(argv));

    const auto res = parse_config(argc, argv);
    REQUIRE(res.is_ok());

    const Config& cfg = res.unwrap();
    CHECK(cfg.port == 9090);
    CHECK(cfg.host == "127.0.0.1");
    CHECK(cfg.verbose == true);
  }

  SECTION("Parse options with long names") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"app",    "--port",  "3000",
                          "--host", "0.0.0.0", "--verbose"};
    const i32 argc = static_cast<i32>(std::size(argv));

    const auto res = parse_config(argc, argv);
    REQUIRE(res.is_ok());

    const Config& cfg = res.unwrap();
    CHECK(cfg.port == 3000);
    CHECK(cfg.host == "0.0.0.0");
    CHECK(cfg.verbose == true);
  }

  SECTION("Long option using '=' syntax") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"app", "--port=3000", "--host=0.0.0.0"};
    const i32 argc = static_cast<i32>(std::size(argv));

    const auto res = parse_config(argc, argv);
    REQUIRE(res.is_ok());

    const Config& cfg = res.unwrap();
    CHECK(cfg.port == 3000);
    CHECK(cfg.host == "0.0.0.0");
  }

  SECTION("Mixed short and long options") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"app", "-p", "5000", "--host=10.0.0.1", "-V"};
    const i32 argc = static_cast<i32>(std::size(argv));

    const auto res = parse_config(argc, argv);
    REQUIRE(res.is_ok());

    const Config& cfg = res.unwrap();
    CHECK(cfg.port == 5000);
    CHECK(cfg.host == "10.0.0.1");
    CHECK(cfg.verbose == true);
  }

  SECTION("Only some options provided, rest keep defaults") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"app", "--host=192.168.1.1"};
    const i32 argc = static_cast<i32>(std::size(argv));

    const auto res = parse_config(argc, argv);
    REQUIRE(res.is_ok());

    const Config& cfg = res.unwrap();
    CHECK(cfg.port == 8080);
    CHECK(cfg.host == "192.168.1.1");
    CHECK_FALSE(cfg.verbose);
  }

  SECTION("Unknown long option fails") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"app", "--bogus"};
    const i32 argc = static_cast<i32>(std::size(argv));

    const auto res = parse_config(argc, argv);
    CHECK_FALSE(res.is_ok());
  }

  SECTION("Unknown short option fails") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"app", "-z"};
    const i32 argc = static_cast<i32>(std::size(argv));

    const auto res = parse_config(argc, argv);
    CHECK_FALSE(res.is_ok());
  }

  SECTION("Missing value for long option fails") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"app", "--port"};
    const i32 argc = static_cast<i32>(std::size(argv));

    const auto res = parse_config(argc, argv);
    CHECK_FALSE(res.is_ok());
  }

  SECTION("Missing value for short option fails") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"app", "-p"};
    const i32 argc = static_cast<i32>(std::size(argv));

    const auto res = parse_config(argc, argv);
    CHECK_FALSE(res.is_ok());
  }

  SECTION("Help flag short-circuits to an error result") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"app", "--help"};
    const i32 argc = static_cast<i32>(std::size(argv));

    const auto res = parse_config(argc, argv);
    CHECK_FALSE(res.is_ok());
  }
}

TEST_CASE("Macro required options and validation", "[arg][macro]") {
  SECTION("Fails when required option is missing") {
    struct RequiredConfig {
      std::string key;
    };
    auto binder = ARGS_OPT(RequiredConfig, key, 'k', "key", "API key", true);

    Command cmd("test_app", "1.0.0");
    std::move(binder).apply_to_command(&cmd);

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

    const auto res = parse_auth(argc, argv);
    CHECK_FALSE(res.is_ok());
  }

  SECTION("Succeeds end-to-end when required option is provided (short)") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"auth-app", "-t", "secret123"};
    const i32 argc = static_cast<i32>(std::size(argv));

    const auto res = parse_auth(argc, argv);
    REQUIRE(res.is_ok());
    CHECK(res.unwrap().token == "secret123");
  }

  SECTION("Succeeds end-to-end when required option is provided (long)") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"auth-app", "--token=secret456"};
    const i32 argc = static_cast<i32>(std::size(argv));

    const auto res = parse_auth(argc, argv);
    REQUIRE(res.is_ok());
    CHECK(res.unwrap().token == "secret456");
  }
}

TEST_CASE("Macro bundled short flags", "[arg][macro]") {
  SECTION("Two boolean flags bundled under one dash both get set") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"bundle-app", "-ab"};
    const i32 argc = static_cast<i32>(std::size(argv));

    const auto res = parse_bundle(argc, argv);
    REQUIRE(res.is_ok());

    const BundleConfig& cfg = res.unwrap();
    CHECK(cfg.a_flag == true);
    CHECK(cfg.b_flag == true);
  }

  SECTION("Only one of the bundled flags provided") {
    // NOLINTNEXTLINE(misc-const-correctness)
    const char* argv[] = {"bundle-app", "-a"};
    const i32 argc = static_cast<i32>(std::size(argv));

    const auto res = parse_bundle(argc, argv);
    REQUIRE(res.is_ok());

    const BundleConfig& cfg = res.unwrap();
    CHECK(cfg.a_flag == true);
    CHECK_FALSE(cfg.b_flag);
  }
}

}  // namespace arg
