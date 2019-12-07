#include "pubsub.h"
#include <stdio.h>

void pub_init(const char *arg, publish_t publish) {
    // simple doesn't use the arg...
    publish(1, "one", "first");
    publish(2, "two", "second");
    publish(3, "three", "third");
}

void sub_init(const char *arg, retrieve_t retrieve) {
    int count = 0;
    uint64_t timestamp_ms;
    float amount;
    char place[MAX_PLACE_LEN];
    char description[MAX_DESCRIPTION_LEN];
    retrieve(&timestamp_ms, &amount, place, description);
    while (timestamp_ms != -1) {
        count++;
        retrieve(&timestamp_ms, &amount, place, description);
    }
    printf("saw %d items\n", count);
}

