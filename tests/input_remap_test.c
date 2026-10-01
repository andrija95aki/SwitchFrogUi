/* Host regression tests use the actual input implementation. No device needed. */
#include <assert.h>
#include "../frogui/input.c"

static void unique(void) {
    unsigned seen = 0;
    for (int i=0;i<FROG_BTN_COUNT;i++) {
        int bit=input_get_raw_bit((FrogButton)i);
        assert(bit>=0 && bit<=FROG_RAW_MAX_BIT);
        assert(!(seen & (1u<<bit)));
        seen |= 1u<<bit;
    }
}
int main(void) {
    input_reset_defaults(); unique();
    assert(FROG_BTN_COUNT == 15);
    input_set_ext_raw(1u << 16);
    for (int i=0;i<3;i++) input_update();
    assert(input_was_pressed(FROG_BTN_FN));
    input_set_ext_raw(0); input_update();
    assert(!input_is_pressed(FROG_BTN_FN));
    input_set_raw_bit(FROG_BTN_B, input_default_raw_bit(FROG_BTN_A));
    assert(input_get_raw_bit(FROG_BTN_A)==14);
    assert(input_get_raw_bit(FROG_BTN_B)==13); unique();
    for(int i=0;i<FROG_BTN_COUNT;i++) for(int j=0;j<FROG_BTN_COUNT;j++) {
        input_set_raw_bit((FrogButton)i,input_default_raw_bit((FrogButton)j)); unique();
    }
    input_reset_defaults();
    input_set_raw_bit(FROG_BTN_B,13);
    input_set_ext_raw(1u<<14);
    for(int i=0;i<4;i++) input_update();
    assert(input_physical_was_pressed(FROG_BTN_B));
    assert(input_is_pressed(FROG_BTN_A));
    assert(!input_is_pressed(FROG_BTN_B));
    input_update(); assert(!input_physical_was_pressed(FROG_BTN_B));
    input_set_ext_raw(0); input_update();
    input_reset_defaults(); unique();
    assert(input_get_raw_bit(FROG_BTN_B)==14);
    /* Old malformed wizard data must never make navigation unreachable. */
    FILE *f=fopen("switchfrog-keymap-test.tmp","w"); assert(f);
    fputs("A=14\nB=14\nUP=-1\n",f); fclose(f);
    assert(input_load_remap("switchfrog-keymap-test.tmp")==0); unique();
    assert(input_get_raw_bit(FROG_BTN_UP)==4);
    assert(input_save_remap("switchfrog-keymap-test.tmp")==0);
    /* A legacy 14-button file gains FN without disturbing valid swaps. */
    f=fopen("switchfrog-keymap-test.tmp","w");assert(f);
    fputs("A=14\nB=13\n",f);fclose(f);
    assert(input_load_remap("switchfrog-keymap-test.tmp")==0);unique();
    assert(input_get_raw_bit(FROG_BTN_A)==14 && input_get_raw_bit(FROG_BTN_FN)==16);
    remove("switchfrog-keymap-test.tmp");
    puts("PASS: remap B, swap uniqueness, physical recovery and malformed maps");
    return 0;
}
