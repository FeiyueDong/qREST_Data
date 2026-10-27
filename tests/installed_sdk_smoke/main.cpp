#include <qrest_data/external_reader.h>
#include <qrest_data/qrest_data.h>
#include <qrest_data/version.h>
#include <cstring>
#include <qrest_data/data_packet.hpp>
#include <qrest_data/file_header.hpp>
#include <qrest_data/metadata.hpp>

#include <string_view>

int main(int argc, char **argv) {
    qrest_external_dataset *dataset = nullptr;
    if (argc > 1) {
        if (qrest_external_read(argv[1], "sit-dat", nullptr, &dataset) != 0)
            return 2;
        const bool valid =
            qrest_external_channel_count(dataset) == 3
            && qrest_external_sample_count(dataset) == 100
            && qrest_external_values(dataset)[0] == 0.019
            && std::strstr(qrest_external_summary(dataset), "native-extension");
        qrest_external_free(dataset);
        if (!valid)
            return 3;
    } else if (qrest_external_read(nullptr, "mseed", nullptr, &dataset) == 0
               || dataset) {
        return 4;
    }

    const char *runtime_version = qrest_data_version();
    if (runtime_version == nullptr) {
        return 1;
    }
    return std::string_view(runtime_version) == QREST_DATA_VERSION_STRING ? 0
                                                                          : 1;
}
