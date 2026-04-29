// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string_view>

#include "fmt/compile.h"

template <typename T>
concept Logger = requires(T& logger) {
  logger.flush();

  logger.trace("{}", 168);
  logger.debug("{}", 168);
  logger.info("{}", 168);
  logger.warn("{}", 168);
  logger.error("{}", 168);
  logger.fatal("{}", 168);

  logger.trace(std::string_view{});
  logger.debug(std::string_view{});
  logger.info(std::string_view{});
  logger.warn(std::string_view{});
  logger.error(std::string_view{});
  logger.fatal(std::string_view{});

  logger.trace(FMT_COMPILE("{}"), 8000);
  logger.debug(FMT_COMPILE("{}"), 8000);
  logger.info(FMT_COMPILE("{}"), 8000);
  logger.warn(FMT_COMPILE("{}"), 8000);
  logger.error(FMT_COMPILE("{}"), 8000);
  logger.fatal(FMT_COMPILE("{}"), 8000);
};
