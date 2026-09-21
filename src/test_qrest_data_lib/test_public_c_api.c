#include <qrest_data/qrest_data.h>

#include <string.h>

int main(void) {
    const char *version = qrest_data_version();
    if (version == NULL) {
        return 1;
    }
    return strcmp(version, QREST_DATA_VERSION_STRING) == 0 ? 0 : 1;
}
