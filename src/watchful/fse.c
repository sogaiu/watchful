#include "../watchful.h"

#ifndef MACOS

watchful_backend_t watchful_fse = {
    .name = "fse",
    .setup = NULL,
    .teardown = NULL,
};

#else

static void callback(
    ConstFSEventStreamRef streamRef,
    void *clientCallBackInfo,
    size_t numEvents,
    void *eventPaths,
    const FSEventStreamEventFlags eventFlags[],
    const FSEventStreamEventId eventIds[])
{
    char **paths = (char **)eventPaths;

    JanetThread *parent = (JanetThread *)clientCallBackInfo;

    for (size_t i = 0; i < numEvents; i++) {
        watchful_event_t *event = (watchful_event_t *)malloc(sizeof(watchful_event_t));

        event->type = 5;

        size_t path_len = strlen(paths[0]) + 1;
        event->path = (char *)malloc(path_len);
        memcpy(event->path, paths[0], path_len);

        janet_thread_send(parent, janet_wrap_pointer(event), 10);
    }
}

static void *loop_runner(void *arg) {
    watchful_stream_t *stream = arg;

    stream->loop = CFRunLoopGetCurrent();

    FSEventStreamScheduleWithRunLoop(
        stream->ref,
        stream->loop,
        kCFRunLoopDefaultMode
    );

    FSEventStreamStart(stream->ref);

    printf("Entering the run loop...\n");
    CFRunLoopRun();
    printf("Leaving the run loop...\n");

    stream->loop = NULL;
    return NULL;
}

static int start_loop(watchful_stream_t *stream) {
    int error = 0;

    pthread_attr_t attr;
    error = pthread_attr_init(&attr);
    if (error) return 1;

    error = pthread_create(&stream->thread, &attr, loop_runner, stream);
    if (error) return 1;

    pthread_attr_destroy(&attr);
    return 0;
}

static int setup(watchful_stream_t *stream) {
    printf("Setting up...\n");

    FSEventStreamContext stream_context;
    memset(&stream_context, 0, sizeof(stream_context));
    stream_context.info = stream->parent;

    CFStringRef path = CFStringCreateWithCString(NULL, (const char *)stream->wm->path, kCFStringEncodingUTF8);
    CFArrayRef pathsToWatch = CFArrayCreate(NULL, (const void **)&path, 1, NULL);

    CFAbsoluteTime latency = 1.0; /* Latency in seconds */

    stream->ref = FSEventStreamCreate(
        NULL,
        &callback,
        &stream_context,
        pathsToWatch,
        kFSEventStreamEventIdSinceNow, /* Or a previous event ID */
        latency,
        /* kFSEventStreamCreateFlagNone /1* Flags explained in reference *1/ */
        kFSEventStreamCreateFlagFileEvents
    );

    int error = start_loop(stream);
    if (error) return 1;

    return 0;
}

static int teardown(watchful_stream_t *stream) {
    printf("Tearing down...\n");

    if (stream->thread) {
        CFRunLoopStop(stream->loop);
        pthread_join(stream->thread, NULL);
    }

    FSEventStreamStop(stream->ref);
    FSEventStreamInvalidate(stream->ref);
    FSEventStreamRelease(stream->ref);

    return 0;
}

watchful_backend_t watchful_fse = {
    .name = "fse",
    .setup = setup,
    .teardown = teardown,
};

#endif
