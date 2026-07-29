// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string>
#include <utility>
#include <vector>

#include "fpag/arg/parse_error.h"
#include "fpag/arg/parse_status.h"
#include "fpag/base/tagged_union.h"
#include "fpag/debug/check.h"

namespace arg {

// Wrapper types to distinguish std::string in TaggedUnion.
struct HelpText {
  std::string value;
};

struct VersionText {
  std::string value;
};

template <typename T>
class ParseResult {
 public:
  using Storage =
      base::AutoTaggedUnion<T, std::vector<ParseError>, HelpText, VersionText>;

  ~ParseResult() = default;

  ParseResult(const ParseResult&) = delete;
  ParseResult& operator=(const ParseResult&) = delete;

  ParseResult(ParseResult&&) noexcept = default;
  ParseResult& operator=(ParseResult&&) noexcept = default;

  static ParseResult make_ok(T&& obj) {
    return ParseResult(Storage(std::move(obj)), ParseStatus::Success);
  }

  static ParseResult make_err(std::vector<ParseError>&& errors) {
    return ParseResult(Storage(std::move(errors)), ParseStatus::Error);
  }

  static ParseResult make_help(std::string&& help) {
    return ParseResult(Storage(HelpText{std::move(help)}),
                       ParseStatus::HelpRequested);
  }

  static ParseResult make_version(std::string&& version) {
    return ParseResult(Storage(VersionText{std::move(version)}),
                       ParseStatus::VersionRequested);
  }

  ParseStatus status() const noexcept { return status_; }

  bool is_ok() const noexcept { return status_ == ParseStatus::Success; }
  bool is_err() const noexcept { return status_ == ParseStatus::Error; }
  bool is_help() const noexcept {
    return status_ == ParseStatus::HelpRequested;
  }
  bool is_version() const noexcept {
    return status_ == ParseStatus::VersionRequested;
  }

  T&& unwrap() && {
    FPAG_DCHECK(is_ok());
    return std::move(storage_).template get<T>();
  }

  std::vector<ParseError>&& unwrap_err() && {
    FPAG_DCHECK(is_err());
    return std::move(storage_).template get<std::vector<ParseError>>();
  }

  std::string&& unwrap_help() && {
    FPAG_DCHECK(is_help());
    return std::move(storage_).template get<HelpText>().value;
  }

  std::string&& unwrap_version() && {
    FPAG_DCHECK(is_version());
    return std::move(storage_).template get<VersionText>().value;
  }

 private:
  ParseResult(Storage&& storage, ParseStatus status) noexcept
      : storage_(std::move(storage)), status_(status) {}

  Storage storage_;
  ParseStatus status_;
};

}  // namespace arg
