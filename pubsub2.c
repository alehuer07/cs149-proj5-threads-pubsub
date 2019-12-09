#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dlfcn.h>
#include <pthread.h>


#include "pubsub.h"

pthread_cond_t cond;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int pub_count = 0;
int sub_count = 0;
int done = 0;

struct pub_params {
    char *arg; 
    publish_t publish; 
    void (*pub_init)(struct pub_params*); 
};

struct sub_params { 
    char *arg; 
    retrieve_t retrieve;
    void (*sub_init)(struct sub_params*);
};

struct item *listForEachSub[100000];

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

struct pub_params *pub_params; 
struct sub_params *sub_params; 

// return the current time in milliseconds
int64_t getnow_ms() {
    struct timespec res;
    clock_gettime(CLOCK_REALTIME, &res);
    return res.tv_sec * 1000 + res.tv_nsec / 1000000;
}

// publish a purchase to all subscribers. we will make a copy of the strings
// because there is no guarantee they will stick around after the function returns.
void simple_publish(float amount, const char *place, const char *description) {
    //printf("IN SIMPLE PUBLISH\n");
    struct item *i = malloc(sizeof(*i));
    i->timestamp_ms = getnow_ms();
    i->amount = amount;
    //printf("amount %f\n", amount);
    strcpy(i->place, place);
    strcpy(i->description, description);

    // add to end end of the list
    pthread_mutex_lock(&mutex);
    i->next = NULL;
    if (tail == NULL) {
        head = tail = i;
    } else {
        tail->next = i;
        tail = i;
    }
    pthread_cond_signal(&cond); // unblock at least one of the threads that are blocked on the specified condition variable cond (if any threads are blocked on cond).
    pthread_mutex_unlock(&mutex);
}

struct item *current;
// returns the next element of the list of purchases published. normally this would block
// if there is nothing to return and a publisher is still running.
// when all publishers are finished and there is nothing left to return timestamp_ms will be -1
void simple_retrieve(int64_t *timestamp_ms, float *amount, char *place, char *description) {
    // printf("IN SIMPLE RETRIEVE\n");
    if (current == NULL) {
        // printf("TIMESTAMP -1 IN SIMPLE RETRIEVE\n");
        *timestamp_ms = -1;
	    return;
    }
    pthread_mutex_lock(&mutex);
    // printf("AFTER LOCK\n");
    if(!done || current != NULL) { 
        *timestamp_ms = current->timestamp_ms;
        *amount = current->amount;
        strcpy(place, current->place);
        strcpy(description, current->description);
        current = current->next;
    } else { 
        pthread_cond_wait(&cond, &mutex); 
    }
    pthread_mutex_unlock(&mutex);
    printf("END OF SIMPLE RETRIEVE\n");
}

void *pub_start_thread(void* v) { 
//    printf("IN PUB START THREAD\n");
   struct pub_params *params = pub_params; 
   pub_init_t function = (pub_init_t)(params->pub_init); 
   function(params->arg, params->publish);
   
   return 0;
}

void *sub_start_thread(void* v) {
//    printf("IN SUB START THREAD\n");
   struct sub_params *params = sub_params; 
   sub_init_t function = (sub_init_t)(params->sub_init); 
   function(params->arg, params->retrieve);

   return 0;
}

int main(int argc, char **argv)
{
    // printf("START OF MAIN\n");
    if (argc < 2 || (argc % 2) == 0) {
        printf("USAGE: %s pub_sub_so1 param1 pub_sub_so2 param2 ...\n", argv[0]);
        return 2;
    }

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
    // printf("BEFORE THE PUB CHECK\n");
    pthread_t *p = malloc(sizeof(*p) * pub_count); 

    // printf("AFTER MALLOC\n");
    for (int i = 0; i < pub_count; i++) {
        pub_params = malloc(sizeof(*pub_params));  
        pub_params->arg = pubs_arg[i]; 
        pub_params->publish = simple_publish;
        pub_params->pub_init = (void*)pubs[i];

        // void* pub_start_thread;
        pthread_create(&p[i], NULL, (void*)pub_start_thread, NULL); 
        //TODO: do something if pub is also a sub
    }
    
    // printf("BEFORE THE SUB COUNT LIST ADD\n"); 
    for (int i = 0; i < sub_count; i++) { 
        struct item *copy = head; 
        listForEachSub[i] = head; 
    }

    // printf("BEFORE THE SUB RETRIEVE CHECK\n");
    pthread_t *s = malloc(sizeof(*s) * sub_count); 
    for (int i = 0; i < sub_count; i++) {
        struct item *listForSub = listForEachSub[i]; 
        current = listForSub; 

        sub_params = malloc(sizeof(*sub_params));  
        sub_params->arg = subs_arg[i]; 
        sub_params->retrieve = simple_retrieve;
        sub_params->sub_init = (void*)subs[i];
        pthread_create(&s[i], NULL, (void*)sub_start_thread, NULL); 
        // printf("amount for sub: %f\n", listForSub->amount);
        //subs[i](subs_arg[i], simple_retrieve);
    }
    // printf("AFTER THE SUB RETRIEVE CHECK\n");

    for(int i=0; i<pub_count; i++) { 
        pthread_join(p[i], NULL); 
    }

    for(int i=0; i<sub_count; i++) { 
        pthread_join(s[i], NULL); 
    }

    pthread_mutex_lock(&mutex);
    done = 1;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
    // printf("AFTER JOINING");

    return 0;
}
