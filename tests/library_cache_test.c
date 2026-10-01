#include <assert.h>
#define SF_CACHE_DIR "cache"
#include "../apps/art_miss_cache.h"
static void put(const char *path,const char *text){FILE *f=fopen(path,"wb");assert(f);fputs(text,f);fclose(f);}
int main(void) {
    SF_MKDIR("roms");SF_MKDIR("roms/gba");SF_MKDIR("cache");
    put("roms/gba/game.gba","test");uint64_t a,b;
    assert(sf_directory_signature("roms",6,&a));
    const char names[2][32]={"game.gba","other.gba"};
    sf_cache_store("roms",0,a,names,32,2);
    int n=0;void *data=sf_cache_load("roms",0,a,32,&n);
    assert(data && n==2 && !memcmp(data,names,sizeof(names)));free(data);
    put("roms/gba/new.gba","new");assert(sf_directory_signature("roms",6,&b));assert(a!=b);
    assert(!sf_cache_load("roms",0,b,32,&n));
    assert(!sf_cache_load("roms",0,a,31,&n));
    char cf[1200];sf_cache_name("roms",0,cf,sizeof(cf));put(cf,"broken");
    assert(!sf_cache_load("roms",0,a,32,&n));
    assert(!sf_art_exists("roms/gba/images/game.png"));
    sf_art_cache_save();memset(sf_art_miss,0,sizeof(sf_art_miss));sf_art_loaded=0;
    assert(!sf_art_exists("roms/gba/images/game.png"));
    SF_MKDIR("roms/gba/images");put("roms/gba/images/game.png","image");
    memset(sf_art_parents,0,sizeof(sf_art_parents)); /* advance validation interval */
    assert(sf_art_exists("roms/gba/images/game.png"));
    sf_art_cache_clear();assert(sf_art_exists("roms/gba/images/game.png"));
    remove("roms/gba/images/game.png");rmdir("roms/gba/images");
    remove("roms/gba/game.gba");remove("roms/gba/new.gba");rmdir("roms/gba");rmdir("roms");
    remove(cf);remove("cache/art-misses.bin");rmdir("cache");
    puts("PASS: directory invalidation, bad cache rejection, persistent artwork misses and new covers");return 0;
}
