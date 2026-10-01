#include <assert.h>
#define FILTERS_FILE "switchfrog-filter-test.tmp"
#include "../frogui/ext_filter.c"
int main(void) {
    remove(FILTERS_FILE);ext_filter_load();
    assert(!ext_filter_should_hide("PS1",".BIN"));
    assert(!ext_filter_should_hide("ps1","CUE"));
    assert(ext_filter_should_hide("ps1","png"));
    assert(!ext_filter_should_hide("gba","gba"));
    assert(ext_filter_toggle_ext("gba","GBA"));
    assert(!ext_filter_should_hide("GBA",".gba"));
    assert(ext_filter_should_hide("gba","txt"));
    ext_filter_set_enabled("gba",0);assert(!ext_filter_should_hide("gba","txt"));
    ext_filter_load();assert(!ext_filter_get_enabled("gba"));
    assert(ext_filter_has_ext("gba","gba"));
    ext_filter_set_enabled("gba",1);ext_filter_load();
    assert(ext_filter_should_hide("gba","txt"));
    assert(!ext_filter_toggle_ext("gba","extension-too-long"));
    remove(FILTERS_FILE);puts("PASS: platform filters, persistence, case, BIN preservation");return 0;
}
