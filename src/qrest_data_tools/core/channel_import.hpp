/**
**            qREST - Quick Response Evaluation for Safety Tagging
**     Institute of Engineering Mechanics, China Earthquake Administration
**                 Copyright 2024 - 2026 QLab, Dong Feiyue
**                          All Rights Reserved.
** Project: qREST
** File: /src/qrest_data_tools/core/channel_import.hpp
** File Created: Wednesday, 28 October 2026
** Author: Codex
** Last Modified: Wednesday, 28 October 2026
** Modified By: Codex
*/
#pragma once
#include "external_import.hpp"
namespace qrest_data::tools {
// Every scale multiplies the returned reader samples to produce m/s^2.
// Reader defaults are explicit; an uncalibrated stream uses a user-editable 1,
// recorded as a default declaration rather than native calibration.
struct ChannelImport {
    ExternalDataset data;
    std::string source, format;
    bool transpose_text{};
    std::vector<double> scales;
    std::vector<std::string> scale_origins;
};
ChannelImport read_channel_import(const std::string &path,
                                  const std::string &format,
                                  bool transpose_text = false);
// Shared exact-time guard. Empty means the source can enter a draft.
std::string channel_import_error(const ChannelImport &);
std::string channel_import_start_time(const ExternalDataset &);
} // namespace qrest_data::tools
