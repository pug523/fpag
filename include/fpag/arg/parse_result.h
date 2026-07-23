// Copyright 2026 pugur
// This source code is licensed under the Apache License, Version 2.0
// which can be found in the LICENSE file.

#pragma once

#include <string_view>
#include <utility>
#include <vector>

#include "fpag/arg/parse_error.h"
#include "fpag/arg/parse_status.h"
#include "fpag/base/debug/check.h"

namespace arg {

template <typename T>
class ParseResult {
 public:
  ~ParseResult() { reset(); }

  ParseResult(const ParseResult&) = delete;
  ParseResult& operator=(const ParseResult&) = delete;

  ParseResult(ParseResult&& other) noexcept : status_(other.status_) {
    construct_from(std::move(other));
  }

  ParseResult& operator=(ParseResult&& other) noexcept {
    if (this != &other) {
      reset();
      status_ = other.status_;
      construct_from(std::move(other));
    }
    return *this;
  }

  inline static ParseResult make_ok(T&& obj) {
    ParseResult res;
    res.status_ = ParseStatus::Success;
    new (&res.data_.obj) T(std::move(obj));
    return res;
  }

  inline static ParseResult make_err(std::vector<ParseError>&& errors) {
    ParseResult res;
    res.status_ = ParseStatus::Error;
    new (&res.data_.errors) std::vector<ParseError>(std::move(errors));
    return res;
  }

  inline static ParseResult make_help(std::string_view help) {
    ParseResult res;
    res.status_ = ParseStatus::HelpRequested;
    res.data_.help = help;
    return res;
  }

  inline static ParseResult make_version(std::string_view version) {
    ParseResult res;
    res.status_ = ParseStatus::VersionRequested;
    res.data_.version = version;
    return res;
  }

  inline ParseStatus status() const { return status_; }

  inline bool is_ok() const { return status_ == ParseStatus::Success; }
  inline bool is_err() const { return status_ == ParseStatus::Error; }
  inline bool is_help() const { return status_ == ParseStatus::HelpRequested; }
  inline bool is_version() const {
    return status_ == ParseStatus::VersionRequested;
  }

  T&& unwrap() && {
    FPAG_DCHECK_EQ(status_, ParseStatus::Success);
    return std::move(data_.obj);
  }

  std::vector<ParseError>&& unwrap_err() && {
    FPAG_DCHECK_EQ(status_, ParseStatus::Error);
    return std::move(data_.errors);
  }

  std::string_view unwrap_help() && {
    FPAG_DCHECK_EQ(status_, ParseStatus::HelpRequested);
    return data_.help;
  }

  std::string_view unwrap_version() && {
    FPAG_DCHECK_EQ(status_, ParseStatus::VersionRequested);
    return data_.version;
  }

 private:
  ParseStatus status_;
  union Storage {
    T obj;
    std::vector<ParseError> errors;
    std::string_view help;
    std::string_view version;

    Storage() noexcept {}
    ~Storage() noexcept {}
  } data_;

  ParseResult() noexcept = default;
  void reset() noexcept {
    switch (status_) {
      case ParseStatus::Success: data_.obj.~T(); break;
      case ParseStatus::Error:
        using ErrorVec = std::vector<ParseError>;
        data_.errors.~ErrorVec();
        break;
      case ParseStatus::HelpRequested:
      case ParseStatus::VersionRequested: break;
    }
  }

  void construct_from(ParseResult&& other) noexcept {
    switch (status_) {
      case ParseStatus::Success:
        new (&data_.obj) T(std::move(other.data_.obj));
        break;
      case ParseStatus::Error:
        new (&data_.errors)
            std::vector<ParseError>(std::move(other.data_.errors));
        break;
      case ParseStatus::HelpRequested: data_.help = other.data_.help; break;
      case ParseStatus::VersionRequested:
        data_.version = other.data_.version;
        break;
    }
  }
};

}  // namespace arg
