
#include <stdio.h>
#include <core_c/musikcore_c.h>

int main(int argc, char** argv) {
    mcsdk_context* context = NULL;
    mcsdk_context_init(&context);
    if (context) {
        printf("initialized\n");
    }
    mcsdk_map_list ml = mcsdk_svc_metadata_query_albums(context->metadata, "alice");
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
    mcsdk_context_release(&context);
    return 0;
}
