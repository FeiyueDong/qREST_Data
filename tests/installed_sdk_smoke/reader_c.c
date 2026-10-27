#include <qrest_data/external_reader.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    qrest_external_dataset *dataset = NULL;
    if (argc < 2) {
        return qrest_external_read(NULL, "mseed", NULL, &dataset) == -1
                       && dataset == NULL
                       && strlen(qrest_external_last_error()) > 0
                   ? 0
                   : 1;
    }
    if (qrest_external_read(argv[1], "sit-dat", NULL, &dataset)) {
        fprintf(stderr, "%s\n", qrest_external_last_error());
        return 2;
    }
    const int valid =
        qrest_external_channel_count(dataset) == 3
        && qrest_external_sample_count(dataset) == 100
        && qrest_external_values(dataset)[0] == 0.019
        && strstr(qrest_external_summary(dataset), "native-extension") != NULL;
    qrest_external_free(dataset);
    return valid ? 0 : 3;
}
