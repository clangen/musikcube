
#include <stdio.h>
#include <unistd.h>
#include <core_c/musikcore_c.h>

int main(int argc, char** argv) {
    mcsdk_context* context = NULL;
    mcsdk_context_init(&context);
    if (context) {
        printf("initialized\n");
    }
    mcsdk_map_list ml = mcsdk_svc_metadata_query_albums(context->metadata, "beatles");
    int count = mcsdk_map_list_get_count(ml);
    for (int i = 0; i < count; i++) {
        mcsdk_map m = mcsdk_map_list_get_at(ml, i);
        size_t len = mcsdk_map_get_string(m, "album", NULL, 0);
        char* str = malloc(len * sizeof(char));
        mcsdk_map_get_string(m, "album", str, len);
        printf("%s\n", str);
        free(str);
    }
    mcsdk_map_list_release(ml);
    mcsdk_track_list tl = mcsdk_svc_metadata_query_tracks(
        context->metadata, "a day in the life", mcsdk_no_limit, mcsdk_no_offset);
    mcsdk_svc_playback_play(context->playback, tl, 0);
    mcsdk_track_list_release(tl);
    printf("playing for 5 seconds...");
    for (int i = 0; i < 5; i++) {
        usleep(1000000);
        printf("  %d\n", i + 1);
    }
    printf("done playing, shutting down...\n");
    mcsdk_context_release(&context);
    printf("dead\n");
    return 0;
}
