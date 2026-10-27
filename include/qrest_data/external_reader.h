#ifndef QREST_DATA_EXTERNAL_READER_H
#define QREST_DATA_EXTERNAL_READER_H
#include <stddef.h>
#include <stdint.h>
#if defined(_WIN32)
#if defined(QREST_DATA_READERS_BUILD_SHARED)
#define QREST_READERS_API __declspec(dllexport)
#else
#define QREST_READERS_API __declspec(dllimport)
#endif
#else
#define QREST_READERS_API __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif

/* Link qrest_data_readers. The existing qrest_data_lib C ABI is unchanged.
 * Formats: mseed, sit-dat, sit-txt. options_json is NULL or a JSON object:
 * unit: m/s2|cm/s2; counts: bool; sensitivity: count/SI-unit;
 * quantity: Acceleration|Velocity|Displacement; gap_policy:
 * fill-nan|error|ignore; group_index: unsigned; include_dimensionless: bool;
 * timezone: +08:00 (TXT only). counts excludes unit/sensitivity/quantity. TXT
 * accepts only unit/timezone. Missing calibration returns RawCounts, never
 * assumed acceleration.
 */
typedef struct qrest_external_dataset qrest_external_dataset;
QREST_READERS_API int qrest_external_read(const char *path,
                                          const char *format,
                                          const char *options_json,
                                          qrest_external_dataset **out_dataset);
QREST_READERS_API void qrest_external_free(qrest_external_dataset *dataset);
QREST_READERS_API size_t
qrest_external_channel_count(const qrest_external_dataset *dataset);
QREST_READERS_API size_t
qrest_external_sample_count(const qrest_external_dataset *dataset);
/* Borrowed channel-major doubles and UTF-8 JSON; valid until dataset is freed.
 * Raw counts are exactly representable as doubles. NaN gaps remain explicit.
 * NULL dataset -> NULL pointer / zero count. No caller frees borrowed memory.
 */
QREST_READERS_API const double *
qrest_external_values(const qrest_external_dataset *dataset);
QREST_READERS_API const char *
qrest_external_summary(const qrest_external_dataset *dataset);
/* target_channels contains one zero-based target per source channel
 * (permutation). NULL + mapping_count=0 requests the compatibility sequential
 * mapping. Validates dimensions, finite data, time, quantity, units, Scale and
 * known axes before writing. metadata_json is complete user-supplied qREST
 * metadata.
 */
QREST_READERS_API int
qrest_external_write_qrest(const qrest_external_dataset *dataset,
                           const char *metadata_json,
                           const size_t *target_channels,
                           size_t mapping_count,
                           const char *output_path,
                           uint16_t source_id,
                           uint16_t encoding);
/* Thread-local UTF-8 error, valid until the next read/write on this thread.
 * read/write return 0 on success and -1 on error; exceptions never cross C ABI.
 */
QREST_READERS_API const char *qrest_external_last_error(void);
#ifdef __cplusplus
}
#endif
#endif
