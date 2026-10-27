#include <qrest_data/external_reader.h>
#include <memory>
#include <stdexcept>
#include "external_import.hpp"
#include "qrest_file.hpp"

struct qrest_external_dataset {
    qrest_data::tools::ExternalDataset data;
    std::string summary;
};
namespace {
thread_local std::string last_error;
}
extern "C" {
int qrest_external_read(const char *path,
                        const char *format,
                        const char *options,
                        qrest_external_dataset **out) {
    last_error.clear();
    if (out)
        *out = nullptr;
    try {
        if (!path || !format || !out)
            throw std::runtime_error(
                "path, format and out_dataset are required");
        auto result = std::make_unique<qrest_external_dataset>();
        result->data = qrest_data::tools::read_external_dataset(
            path, format, options ? options : "{}");
        result->summary =
            qrest_data::tools::external_dataset_summary_json(result->data);
        *out = result.release();
        return 0;
    } catch (const std::exception &e) {
        last_error = e.what();
    } catch (...) {
        last_error = "unknown reader failure";
    }
    return -1;
}
void qrest_external_free(qrest_external_dataset *d) { delete d; }
size_t qrest_external_channel_count(const qrest_external_dataset *d) {
    return d ? d->data.channel_count : 0;
}
size_t qrest_external_sample_count(const qrest_external_dataset *d) {
    return d ? d->data.sample_count : 0;
}
const double *qrest_external_values(const qrest_external_dataset *d) {
    return d ? d->data.channel_sequential_data.data() : nullptr;
}
const char *qrest_external_summary(const qrest_external_dataset *d) {
    return d ? d->summary.c_str() : nullptr;
}
const char *qrest_external_last_error(void) { return last_error.c_str(); }
int qrest_external_write_qrest(const qrest_external_dataset *d,
                               const char *metadata_json,
                               const size_t *targets,
                               size_t count,
                               const char *path,
                               uint16_t source,
                               uint16_t encoding) {
    last_error.clear();
    try {
        if (!d || !metadata_json || !path || (!targets && count))
            throw std::runtime_error(
                "invalid qrest_external_write_qrest arguments");
        const auto metadata = qrest_data::Metadata::from_bytes(metadata_json);
        auto mapping = qrest_data::tools::make_sequential_channel_mapping(
            d->data, metadata);
        if (targets) {
            mapping.clear();
            for (size_t i = 0; i < count; ++i)
                mapping.push_back({i, targets[i]});
        }
        const auto mapped = qrest_data::tools::apply_external_channel_mapping(
            d->data, metadata, mapping);
        qrest_data::tools::require_external_dataset_compatibility(mapped,
                                                                  metadata);
        qrest_data::tools::write_qrest_file(path,
                                            metadata_json,
                                            mapped.channel_sequential_data,
                                            source,
                                            encoding);
        return 0;
    } catch (const std::exception &e) {
        last_error = e.what();
    } catch (...) {
        last_error = "unknown import failure";
    }
    return -1;
}
}
