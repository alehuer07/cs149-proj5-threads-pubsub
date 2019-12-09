#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dlfcn.h>

#include <pthread.h>

#include "pubsub.h"

// each item will have an amount, place of purchase, and description.
// the timestamp will be filled in by the pubsub engine when publish is called.
// next is used to build a linked list
struct item
{
    int64_t timestamp_ms;
    float amount;
    char place[MAX_PLACE_LEN];
    char description[MAX_DESCRIPTION_LEN];
    struct item *next;
};

//Todo: Make a global list and store the heads of each subscriber list

// this is a simple implementation of a linked list.
// we don't need to worry about thread safety because this simple implementation
// is single threaded
struct item *head = NULL;
// keeping track of the tail simplifies the code a bit and makes the implementation
// more efficient
struct item *tail = NULL;

// return the current time in milliseconds
int64_t getnow_ms()
{
    printf("Begin getnow_ms\n");
    struct timespec res;
    clock_gettime(CLOCK_REALTIME, &res);
    return res.tv_sec * 1000 + res.tv_nsec / 1000000;
}
typedef struct pub_arguments
{
    char *arg;
    publish_t publish;
    void *init_function;
} pub_struct;

typedef struct sub_arguments
{
    char *arg;
    retrieve_t retrieve;
    void *init_function;
} sub_struct;

// Delegate method to create pub threads
// @param args - a struct containing the arguments to the pub_init function
void *
start_pub_thread(pub_struct *args)
{

    printf("Begin start_pub_thread\n");
    pub_struct *pubargs = args;

    pub_init_t function = (pub_init_t)args->init_function;

    function(args->arg, args->publish);
}

// Delegate method to create sub threads;
// @param args - a struct containing the arguments to the sub_init function
void *start_sub_thread(sub_struct *args)
{
    printf("Begin start_sub_thread\n");
    sub_struct *subargs = args;

    sub_init_t function = (sub_init_t)args->init_function;

    function(args->arg, args->retrieve);
}

// publish a purchase to all subscribers. we will make a copy of the strings
// because there is no guarantee they will stick around after the function returns.
void simple_publish(float amount, const char *place, const char *description)
{
    printf("Begin simple_publish\n");
    struct item *i = malloc(sizeof(*i));
    i->timestamp_ms = getnow_ms();
    i->amount = amount;
    strcpy(i->place, place);
    strcpy(i->description, description);

    // add to end end of the list
    i->next = NULL;
    if (tail == NULL)
    {
        head = tail = i;
    }
    else
    {
        tail->next = i;
        tail = i;
    }
}

struct item *current;
// returns the next element of the list of purchases published. normally this would block
// if there is nothing to return and a publisher is still running.
// when all publishers are finished and there is nothing left to return timestamp_ms will be -1
void simple_retrieve(int64_t *timestamp_ms, float *amount, char *place, char *description)
{
    printf("Begin simple_retrieve\n");
    if (current == NULL)
    {
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
    printf("START MAIN\n");

    if (argc < 2 || (argc % 2) == 0)
    {
        printf("USAGE: %s pub_sub_so1 param1 pub_sub_so2 param2 ...\n", argv[0]);
        return 2;
    }

    //* starting the pub and sub count to zero,
    //* using this as an index for inserting into our "arrays" below
    int *pub_count_ptr = malloc(sizeof(int));
    int *sub_count_ptr = malloc(sizeof(int));

    int pub_count = *pub_count_ptr;
    int sub_count = *pub_count_ptr;

    pub_count = 0;
    sub_count = 0;

    // we are allocating for the maximum possible, probably every
    // argument will not be both a pub and a sub
    pub_init_t *pubs = malloc(sizeof(*pubs) * (argc / 2));
    sub_init_t *subs = malloc(sizeof(*subs) * (argc / 2));
    char **pubs_arg = malloc(sizeof(*pubs_arg) * (argc / 2));
    char **subs_arg = malloc(sizeof(*subs_arg) * (argc / 2));

    printf("AFTER ARRAYS\n");

    // we load in all the libraries specified on the command line. the library may
    // have a publisher, subscriber, or both!
    for (int i = 1; i < argc; i += 2)
    {
        void *dh = dlopen(argv[i], RTLD_LAZY);
        if (dh == NULL)
        {
            fprintf(stderr, "%s\n", dlerror());
            continue;
        }
        //* retrieving the pub_init or sub_init function from the simple files, if they have them.
        pub_init_t p = dlsym(dh, "pub_init");
        sub_init_t s = dlsym(dh, "sub_init");
        if (p)
        {
            pubs_arg[pub_count] = argv[i + 1]; // storing the pub arguments into the pub argument "array"
            pubs[pub_count++] = p;             // storing what we got from p (pub_init function) into pubs function array, then incrementing pubcount
        }
        if (s)
        {
            subs_arg[sub_count] = argv[i + 1]; // storing the sub arguments into the sub argument "array"
            subs[sub_count++] = s;             // storing what we got from s (sub_init function) into subs function array, then incrementing pubcount
        }
    }

    printf("FINISHED POPULATING ARRAYS\n");

    // do all the pubs first (this might fail if the pubs are also subs...)
    //* starting up all the pubs and then the subs
    //! inside the for loops, the pub_init and sub_init functions are being called

    // ------------ Publisher Threads ----------------
    pthread_t *publishers = malloc(sizeof(*publishers) * pub_count);

    for (int i = 0; i < pub_count; i++)
    {
        // Instantiating publisher argument struct
        pub_struct *pubarguments = malloc(sizeof(pub_struct));
        pubarguments->arg = pubs_arg[i];
        pubarguments->publish = simple_publish;
        pubarguments->init_function = (void *)pubs[i];

        void* start_pub_thread;

        pthread_create(&publishers[i], NULL, start_pub_thread, &pubarguments);
    }
    // ------------------------------------------------
    printf("FINISHED PUB THREADS\n");

    // ------------ Subscriber Threads ----------------
    pthread_t *subscribers = malloc(sizeof(*subscribers) * sub_count);

    for (int i = 0; i < sub_count; i++)
    {
        // Instantiating subscriber argument struct
        sub_struct *subarguments = malloc(sizeof(sub_struct));
        subarguments->arg = subs_arg[i];
        subarguments->retrieve = simple_retrieve;
        subarguments->init_function = (void *)subs[i];

        void* start_sub_thread;

        pthread_create(&subscribers[i], NULL, start_sub_thread, &subarguments);
    }
    // ------------------------------------------------
    printf("AFTER INIT THREADS\n");


    printf("JUST BEFORE JOINS\n");
    for (int i = 0; i < pub_count; i++)
    {
        printf("GOT INTO PUB JOIN\n");
        // pthread_join(publishers[i], NULL); //! the second parameter is used for a return, may want to use for end condition checking?
    }
    for (int i = 0; i < sub_count; i++)
    {
        printf("GOT INTO SUB JOIN\n");
        // pthread_join(subscribers[i], NULL);
    }

     printf("AFTER JOIN\n");



    return 0;
}
