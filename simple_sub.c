#include "pubsub.h"
#include <stdio.h>

void sub_init(const char *arg, retrieve_t retrieve) {
    int count = 0;
    int64_t timestamp_ms;
    float amount;
    char place[MAX_PLACE_LEN];
    char description[MAX_DESCRIPTION_LEN];
    retrieve(&timestamp_ms, &amount, place, description);
    while (timestamp_ms != -1) {
        printf("%ld: $%f @%s for %s\n", timestamp_ms, amount, place, description);
        retrieve(&timestamp_ms, &amount, place, description);
    }
}

