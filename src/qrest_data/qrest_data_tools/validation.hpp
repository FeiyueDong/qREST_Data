#ifndef QREST_DATA_TOOLS_VALIDATION_HPP
#define QREST_DATA_TOOLS_VALIDATION_HPP

#include <iosfwd>
#include <string>
#include <vector>

#include "metadata.hpp"
#include "qrest_file.hpp"

namespace qrest_data::tools {

struct ValidationReport {
    std::vector<std::string> errors;
    std::vector<std::string> warnings;

    [[nodiscard]] bool ok() const noexcept { return errors.empty(); }
};

ValidationReport validate_metadata(const Metadata &metadata);
ValidationReport validate_qrest_file(const std::string &path);
ValidationReport validate_text_dataset(const std::string &metadata_path,
                                       const std::string &data_path);

void print_validation_report(std::ostream &out,
                             const std::string &subject,
                             const ValidationReport &report);

} // namespace qrest_data::tools

#endif // QREST_DATA_TOOLS_VALIDATION_HPP
