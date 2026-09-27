/* Run from frogui/ so production font search paths find the bundled fonts. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int fail_alloc,short_read,alloc_calls;
static void *test_malloc(size_t size){alloc_calls++;return fail_alloc?NULL:malloc(size);}
static size_t test_fread(void *p,size_t size,size_t n,FILE *f){
    return fread(p,size,short_read&&n?n-1:n,f);
}
#define malloc test_malloc
#define fread test_fread
#include "../frogui/font.c"
#undef malloc
#undef fread
static uint16_t pixels[640*480];
static void assert_visible(void){
    memset(pixels,0,sizeof(pixels));
    font_draw_text_scaled(pixels,640,480,10,10,"ABC SPACE DEL ENTER",0xffff,78);
    unsigned ink=0;for(unsigned i=0;i<sizeof(pixels)/sizeof(*pixels);i++)ink+=pixels[i]!=0;
    assert(ink>0);assert(font_measure_text("ABC")>0);
}
int main(void){
    /* Cold start with no SD font or heap must still render usable labels. */
    fail_alloc=1;font_init();assert(!font_loaded);assert_visible();
    memset(pixels,0,sizeof(pixels));
    font_draw_mono_text(pixels,640,480,10,10,"$ command_",0xffff,115);
    unsigned ink=0;for(unsigned i=0;i<640*480;i++)ink+=pixels[i]!=0;assert(ink);
    fail_alloc=0;font_init();assert(font_loaded);assert_visible();
    unsigned char *original=font_buffer;float scale=font_scale;
    font_load_file("missing-test-font.ttf");assert(font_buffer==original);assert_visible();
    fail_alloc=1;font_load_file("monogram.ttf");assert(font_buffer==original);assert_visible();
    fail_alloc=0;short_read=1;font_load_file("monogram.ttf");
    assert(font_buffer==original);assert_visible();short_read=0;
    int calls=alloc_calls;font_load_file("GamePocket-Regular-ZeroKern.ttf");
    assert(calls==alloc_calls);assert(font_scale==scale);
    unsigned char bad[28]={0,1,0,0,0,1};
    assert(!font_file_valid(bad,5));bad[23]=100;assert(!font_file_valid(bad,sizeof(bad)));
    const char *fonts[]={"monogram.ttf","SpaceMono-Regular.ttf","Audiowide-Regular.ttf",
      "AtkinsonHyperlegible-Regular.ttf","Bungee-Regular.ttf","ChakraPetch-Regular.ttf",
      "Quantico-Regular.ttf","Rajdhani-Regular.ttf","Righteous-Regular.ttf",
      "ShareTechMono-Regular.ttf","Tomorrow-Regular.ttf","GamePocket-Regular-ZeroKern.ttf"};
    for(unsigned f=0;f<sizeof(fonts)/sizeof(*fonts);f++){
        font_load_file(fonts[f]);assert(!strcmp(loaded_font_name,fonts[f]));
        for(int size=70;size<=130;size+=30){
            font_set_size_percent(size);float before=font_scale;
            int width=font_measure_text("MENU LABELS");
            for(int repeat=0;repeat<20;repeat++){
                for(int percent=62;percent<=100;percent+=6){
                    font_draw_text_scaled(pixels,640,480,10,10,"!@# QWERTY SPACE DEL ENTER",0xffff,percent);
                    assert(font_measure_text_scaled("QWERTY",percent)>0);
                }
                assert_visible();assert(font_scale==before);
                assert(font_measure_text("MENU LABELS")==width);
            }
        }
    }
    printf("PASS: 12 fonts, 3 sizes, 720 keyboard cycles; missing/short-read/allocation failures; no-heap text fallback; stable label metrics\n");
    return 0;
}
