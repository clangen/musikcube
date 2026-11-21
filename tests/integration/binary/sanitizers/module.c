/* deliberately different global sizes expose stale poison when dylib addresses
are reused. the indexed accessor verifies global redzones remain instrumented. */
#ifndef MODULE_NAME
#define MODULE_NAME "short module"
#endif

static char bytes[4] = { 1, 2, 3, 4 };

/* return storage whose global redzone must disappear when this module unloads. */
const char* module_name(void) { return MODULE_NAME; }
/* the negative probe deliberately supplies the first invalid index. */
int module_byte(unsigned index) {
    /* use a pointer so the global-redzone probe reaches ASan rather than
    stopping first at UBSan's separate array-bounds check. */
    char* volatile data = bytes;
    return data[index];
}
