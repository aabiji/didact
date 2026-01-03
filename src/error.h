#pragma once

#include <format>
#include <stdexcept>

class Error : public std::runtime_error {
public:
  template <typename... T>
  explicit Error(std::string_view fmt_str, T&&... args)
      : std::runtime_error(
            std::vformat(fmt_str, std::make_format_args(std::forward<T>(args)...))) {}

  explicit Error(std::string msg) : std::runtime_error(msg) {}
};
