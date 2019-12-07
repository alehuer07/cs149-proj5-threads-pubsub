#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dlfcn.h>

#include "pubsub.h"

// each item will have an amount, place of purchase, and description.
// the timestamp will be filled in by the pubsub engine when publish is called.
// next is used to build a linked list
struct item {
    int64_t timestamp_ms;
    float amount;
    char place[MAX_PLACE_LEN];
    char description[MAX_DESCRIPTION_LEN];
    struct item *next;
};

// this is a simple implementation of a linked list.
// we don't need to worry about thread safety because this simple implementation 
// is single threaded
struct item *head = NULL;
// keeping track of the tail simplifies the code a bit and makes the implementation
// more efficient
struct item *tail = NULL;

// return the current time in milliseconds
int64_t getnow_ms() {
    struct timespec res;
    clock_gettime(CLOCK_REALTIME, &res);
    return res.tv_sec * 1000 + res.tv_nsec / 1000000;
}

// publish a purchase to all subscribers. we will make a copy of the strings
// because there is no guarantee they will stick around after the function returns.
void simple_publish(float amount, const char *place, const char *description) {
    struct item *i = malloc(sizeof(*i));
    i->timestamp_ms = getnow_ms();
    i->amount = amount;
    strcpy(i->place, place);
    strcpy(i->description, description);

    // add to end end of the list
    i->next = NULL;
    if (tail == NULL) {
        head = tail = i;
    } else {
        tail->next = i;
        tail = i;
    }
}

struct item *current;
// returns the next element of the list of purchases published. normally this would block
// if there is nothing to return and a publisher is still running.
// when all publishers are finished and there is nothing left to return timestamp_ms will be -1
void simple_retrieve(int64_t *timestamp_ms, float *amount, char *place, char *description) {
    if (current == NULL) {
        *timestamp_ms = -1;
	return;
    }
    *timestamp_ms = current->timestamp_ms;
    *amount = current->amount;
    strcpy(place, current->place);
    strcpy(description, current->description);
    current = current->next;
}

int main(int argc, char **argv)
{
    if (argc < 2 || (argc % 2) == 0) {
        printf("USAGE: %s pub_sub_so1 param1 pub_sub_so2 param2 ...\n", argv[0]);
        return 2;
    }
    int pub_count = 0;
    int sub_count = 0;

    // we are allocating for the maximum possible, probably every
    // argument will not be both a pub and a sub
    pub_init_t *pubs = malloc(sizeof(*pubs) * (argc/2));
    sub_init_t *subs = malloc(sizeof(*subs) * (argc/2));
    char **pubs_arg = malloc(sizeof(*pubs_arg) * (argc/2));
    char **subs_arg = malloc(sizeof(*subs_arg) * (argc/2));

    // we load in all the libraries specified on the command line. the library may
    // have a publisher, subscriber, or both!
    for (int i = 1; i < argc; i += 2) {
        void *dh = dlopen(argv[i], RTLD_LAZY);
        if (dh == NULL) {
            fprintf(stderr, "%s\n", dlerror());
            continue;
        }
        pub_init_t p = dlsym(dh, "pub_init");
        sub_init_t s = dlsym(dh, "sub_init");
        if (p) {
            pubs_arg[pub_count] = argv[i+1];
            pubs[pub_count++] = p;

        }
        if (s) {
            subs_arg[sub_count] = argv[i+1];
            subs[sub_count++] = s;
        }
    }

    // do all the pubs first (this might fail if the pubs are also subs...)
    for (int i = 0; i < pub_count; i++) {
        pubs[i](pubs_arg[i], simple_publish);
    }

    for (int i = 0; i < sub_count; i++) {
        current = head;
        subs[i](subs_arg[i], simple_retrieve);
    }

    return 0;
}
