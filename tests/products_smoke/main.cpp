#include <qrest_data/data_packet.hpp>
#include <qrest_data/file_header.hpp>
#include <qrest_data/metadata.hpp>
#include <qrest_data/qrest_data.h>
#include <qrest_data/version.h>

#include <string_view>

int main() {
    const char *runtime_version = qrest_data_version();
    if (runtime_version == nullptr) {
        return 1;
    }
    constexpr std::string_view parent_version = "9.9.9";
    const std::string_view header_version = QREST_DATA_VERSION_STRING;
    if (header_version == parent_version ||
        std::string_view(runtime_version) != header_version) {
        return 1;
    }
    return 0;
}
