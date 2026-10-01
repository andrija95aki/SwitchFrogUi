#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <dlfcn.h>
#include <stdarg.h>
struct game_info {const char *path;const void *data;size_t size;const char *meta;};
static bool environment(unsigned cmd,void *data) {
    static const char *dir=".";
    if(cmd==9 || cmd==30 || cmd==31){*(const char**)data=dir;return true;}
    if(cmd==10)return true;
    if(cmd==17){*(bool*)data=false;return true;}
    if(cmd==18 || cmd==11)return true;
    return false;
}
static void video(const void*a,unsigned b,unsigned c,size_t d){}
static void audio(int16_t a,int16_t b){}
static size_t batch(const int16_t*a,size_t n){return n;}
static void poll(void){}
static int16_t input(unsigned a,unsigned b,unsigned c,unsigned d){return 0;}
#define GET(name,type) type name=(type)dlsym(lib,#name);assert(name)
int main(int argc,char **argv) {
    assert(argc==2);
    void *lib=dlopen(argv[1],RTLD_NOW);if(!lib){puts(dlerror());return 1;}
    typedef void(*VoidFn)(void);typedef void(*SetEnv)(bool(*)(unsigned,void*));
    typedef bool(*LoadFn)(const struct game_info*);
    GET(retro_set_environment,SetEnv);GET(retro_init,VoidFn);GET(retro_deinit,VoidFn);
    GET(retro_load_game,LoadFn);GET(retro_unload_game,VoidFn);
    ((void(*)(void*))dlsym(lib,"retro_set_video_refresh"))((void*)video);
    ((void(*)(void*))dlsym(lib,"retro_set_audio_sample"))((void*)audio);
    ((void(*)(void*))dlsym(lib,"retro_set_audio_sample_batch"))((void*)batch);
    ((void(*)(void*))dlsym(lib,"retro_set_input_poll"))((void*)poll);
    ((void(*)(void*))dlsym(lib,"retro_set_input_state"))((void*)input);
    retro_set_environment(environment);retro_init();
    /* Valid empty ZIP directory, no ROM data. Known driver must reject it. */
    const unsigned char zip[22]={'P','K',5,6};
    FILE*f=fopen("pacman.zip","wb");assert(f);assert(fwrite(zip,1,sizeof(zip),f)==sizeof(zip));assert(!fclose(f));
    struct game_info game={"./pacman.zip",NULL,0,NULL};
    assert(!retro_load_game(&game));
    retro_unload_game();retro_deinit();dlclose(lib);remove("pacman.zip");
    puts("PASS: MAME2000 missing ROM load returns false and unload is safe");return 0;
}
