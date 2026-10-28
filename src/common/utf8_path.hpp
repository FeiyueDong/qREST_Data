/**
**            qREST - Quick Response Evaluation for Safety Tagging
**     Institute of Engineering Mechanics, China Earthquake Administration
**                 Copyright 2024 - 2026 QLab, Dong Feiyue
**                          All Rights Reserved.
** Project: qREST
** File: /src/common/utf8_path.hpp
** File Created: Wednesday, 28 October 2026
** Author: Codex
** Last Modified: Wednesday, 28 October 2026
** Modified By: Codex
*/
#pragma once
#include <filesystem>
#include <string>
#include <string_view>
namespace qrest_data::detail {
// File-name strings at Core/CLI/SDK boundaries are UTF-8. Never construct a
// Windows native path from a narrow locale-dependent string or narrow it back.
inline std::filesystem::path path_from_utf8(std::string_view text) {
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}
inline std::string path_to_utf8(const std::filesystem::path &path) {
    const auto text = path.generic_u8string();
    return {text.begin(), text.end()};
}
} // namespace qrest_data::detail
