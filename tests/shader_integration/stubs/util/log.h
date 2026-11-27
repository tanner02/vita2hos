#pragma once
#include <gxm/types.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <cstdio>
#include <cassert>
#include <string>
#include <utility>
template <typename... Args> void shader_test_log(std::string_view message, Args &&...args)
{
    auto text = fmt::format(fmt::runtime(message), std::forward<Args>(args)...);
    std::fprintf(stderr, "%s\n", text.c_str());
}
#define LOG_TRACE(...) ((void)0)
#define LOG_DEBUG(...) ((void)0)
#define LOG_INFO shader_test_log
#define LOG_INFO_ONCE shader_test_log
#define LOG_WARN shader_test_log
#define LOG_ERROR shader_test_log
#define LOG_CRITICAL shader_test_log
template <typename T> std::string log_hex(T value)
{ return fmt::format("0x{:X}", static_cast<uint64_t>(value)); }
template <typename T> std::string log_hex_full(T value) { return log_hex(value); }
template <> struct fmt::formatter<SceGxmAttributeFormat> : formatter<uint32_t> {
    auto format(SceGxmAttributeFormat value, format_context &ctx) const
    { return formatter<uint32_t>::format(static_cast<uint32_t>(value), ctx); }
};
