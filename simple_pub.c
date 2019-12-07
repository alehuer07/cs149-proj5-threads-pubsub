#define _DEFAULT_SOURCE
#include "pubsub.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void pub_init(const char *arg, publish_t publish) {
    int count = strtol(arg, 0, 10);
    if (count <= 0) {
        printf("the argument to %s must be an integer greater than zero!\n", __FILE__);
	exit(2);
    }
    for (int i = 0; i < count; i++) {
	publish((float)i, "simple place", "periodic purchase");
	// sleep 100ms
	usleep(100000);
    }
}
