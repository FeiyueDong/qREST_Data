#ifndef QREST_DATA_TOOLS_VALIDATION_HPP
#define QREST_DATA_TOOLS_VALIDATION_HPP

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

#include "metadata.hpp"
#include "qrest_file.hpp"

namespace qrest_data::tools {

enum class ValidationMode {
    Draft,
    Final,
};

struct ValidationOptions {
    ValidationMode mode{ValidationMode::Final};
};

struct ValidationReport {
    std::vector<std::string> errors;
    std::vector<std::string> warnings;

    [[nodiscard]] bool ok() const noexcept { return errors.empty(); }
};

ValidationReport validate_metadata(const Metadata &metadata,
                                   ValidationOptions options = {});
ValidationReport
validate_qrest_content(const Metadata &metadata,
                       std::uint16_t packet_channel_count,
                       std::uint16_t packet_sampling_rate,
                       std::uint32_t packet_data_point_count,
                       std::uint64_t packet_timestamp_ms,
                       std::size_t channel_sequential_value_count,
                       ValidationOptions options = {});
ValidationReport validate_qrest_file(const std::string &path);
ValidationReport validate_text_dataset(const std::string &metadata_path,
                                       const std::string &data_path);

void print_validation_report(std::ostream &out,
                             const std::string &subject,
                             const ValidationReport &report);

} // namespace qrest_data::tools

#endif // QREST_DATA_TOOLS_VALIDATION_HPP
