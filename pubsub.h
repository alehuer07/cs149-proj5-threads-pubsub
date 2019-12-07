#ifndef PUBSUB_PUBSUB_H
#define PUBSUB_PUBSUB_H

#include <stdint.h>

#define MAX_PLACE_LEN 80
#define MAX_DESCRIPTION_LEN 256

typedef void (*publish_t)(float amount, const char *place, const char *description);
/* if timestamp == -1 that means that there is nothing left to retrieve */
typedef void (*retrieve_t)(int64_t *timestamp_ms, float *amount, char *place, char *description);

/* the init functions will start up a publisher or subscriber with the given
 * arg and function. the function will return when all items have been processed */
typedef void (*pub_init_t)(const char *arg, publish_t);
typedef void (*sub_init_t)(const char *arg, retrieve_t);

#endif //PUBSUB_PUBSUB_H
