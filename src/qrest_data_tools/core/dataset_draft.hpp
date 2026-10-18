/**
**            qREST - Quick Response Evaluation for Safety Tagging
**     Institute of Engineering Mechanics, China Earthquake Administration
**                 Copyright 2024 - 2026 QLab, Dong Feiyue
**                          All Rights Reserved.
** Project: qREST
** File: /src/qrest_data_tools/core/dataset_draft.hpp
** File Created: Sunday, 18 October 2026
** Author: Codex
** Last Modified: Sunday, 18 October 2026
** Modified By: Codex
*/
#pragma once
#include <filesystem>
#include <functional>
#include <memory>
#include "external_import.hpp"

namespace qrest_data::tools {
// Values are always channel sequential. Metadata edits share immutable samples.
// Source describes provenance only; it never controls draft ownership.
struct DraftSource {
    std::string path, dataset_id, snapshot_id, description;
    bool normalized_physical{}, removed{};
};
class DatasetDraft {
public:
    using Channel = Metadata::InstrumentInfoStruct::ChannelStruct;
    enum class ImportAction { Replace, Append };
    static DatasetDraft from_file(const std::filesystem::path &path);
    static DatasetDraft
    from_values(Metadata metadata,
                std::shared_ptr<const std::vector<double>> values,
                DraftSource source);
    const Metadata &metadata() const { return metadata_; }
    const std::vector<double> &values() const { return *values_; }
    const auto &values_handle() const { return values_; }
    const DraftSource &source() const { return source_; }
    bool dirty() const { return dirty_; }
    std::uint16_t encoding() const { return encoding_; }
    std::uint16_t source_id() const { return source_id_; }
    void describe_source(DraftSource source) { source_ = std::move(source); }
    void source_removed() { source_.removed = true; }
    void replace_metadata(Metadata metadata);
    void apply_json(const std::string &json);
    void edit_channel(std::size_t column, Channel channel);
    void add_channel(Channel channel, std::vector<double> samples = {});
    void duplicate_channel(std::size_t column);
    void select_channels(const std::vector<std::size_t> &order);
    void import_data(const ExternalDataset &data,
                     const std::vector<std::size_t> &order,
                     ImportAction action,
                     Metadata metadata,
                     bool physical_values);
    ValidationReport
    validate(ValidationMode mode = ValidationMode::Final) const;
    // Writes/verifies in a private sibling directory, publishes via an
    // exclusive hard link. Unsupported filesystems fail closed; no
    // rename-overwrite fallback.
    void save_new(const std::filesystem::path &destination,
                  const std::function<void(const std::filesystem::path &)>
                      &verify = {}) const;

private:
    Metadata metadata_;
    std::shared_ptr<const std::vector<double>> values_{
        std::make_shared<const std::vector<double>>()};
    DraftSource source_;
    bool dirty_{};
    std::uint16_t encoding_{1}, source_id_{};
};
void normalize_draft_metadata(Metadata &metadata);
} // namespace qrest_data::tools
