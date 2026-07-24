// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#include "fpag/arg/command.h"

#include <span>
#include <string_view>
#include <utility>

#include "catch2/catch_test_macros.hpp"
#include "fpag/arg/arg.h"

namespace arg {

TEST_CASE("Command basic getters and configuration", "[arg][command]") {
  Command const command = CommandBuilder("myapp", "1.2.3")
                        .about("A sample CLI application")
                        .build();

  CHECK(command.name() == "myapp");
  CHECK(command.version() == "1.2.3");
  CHECK(command.about() == "A sample CLI application");
  CHECK(command.builtin_enabled());
}

TEST_CASE("CommandBuilder and Command state construction", "[arg][command]") {
  SECTION("Lvalue and Rvalue builder chaining with subcommands") {
    CommandBuilder sub_builder("sub", "0.1.0");
    sub_builder.about("Subcommand description").builtin_enabled(false);

    Command sub_cmd = std::move(sub_builder).build();

    CommandBuilder root_builder("app", "1.0.0");
    root_builder.about("Main app")
        .add_arg(ArgBuilder("verbose").short_name('v').is_flag().build())
        .add_subcommand(std::move(sub_cmd));

    Command const root_cmd = std::move(root_builder).build();

    CHECK(root_cmd.name() == "app");
    CHECK(root_cmd.version() == "1.0.0");
    CHECK(root_cmd.about() == "Main app");
    CHECK(root_cmd.builtin_enabled());

    REQUIRE(root_cmd.args().size() == 1);
    CHECK(root_cmd.args()[0].name() == "verbose");

    REQUIRE(root_cmd.subcommands().size() == 1);
    const auto& sub = root_cmd.subcommands()[0];
    CHECK(sub.name() == "sub");
    CHECK(sub.version() == "0.1.0");
    CHECK(sub.about() == "Subcommand description");
    CHECK_FALSE(sub.builtin_enabled());
  }
}

TEST_CASE("Command lookup helpers", "[arg][command]") {
  Command const cmd =
      CommandBuilder("app", "1.0.0")
          .add_arg(ArgBuilder("port").short_name('p').long_name("port").build())
          .add_subcommand(CommandBuilder("run").build())
          .build();

  SECTION("find_arg_by_short") {
    CHECK(cmd.find_arg_by_short('p') != nullptr);
    CHECK(cmd.find_arg_by_short('p')->name() == "port");
    CHECK(cmd.find_arg_by_short('x') == nullptr);
  }

  SECTION("find_arg_by_long") {
    CHECK(cmd.find_arg_by_long("port") != nullptr);
    CHECK(cmd.find_arg_by_long("port")->short_name() == 'p');
    CHECK(cmd.find_arg_by_long("unknown") == nullptr);
  }

  SECTION("find_subcommand") {
    CHECK(cmd.find_subcommand("run") != nullptr);
    CHECK(cmd.find_subcommand("run")->name() == "run");
    CHECK(cmd.find_subcommand("walk") == nullptr);
  }
}

}  // namespace arg

