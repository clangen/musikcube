
#include <stdio.h>
#include <core_c/musikcore_c.h>

int main(int argc, char** argv) {
    mcsdk_context* context = NULL;
    mcsdk_context_init(&context);
    if (context) {
        printf("omg");
    }

    mcsdk_context_release(&context);
    return 0;
}
