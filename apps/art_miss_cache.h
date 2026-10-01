/* Persistent negative artwork cache. Only ENOENT is cached, never decoder,
 * permission or allocation failures. Parent listings invalidate missing-file
 * records when artwork is added. No stat ABI dependency. */
#ifndef SWITCHFROG_ART_MISS_CACHE_H
#define SWITCHFROG_ART_MISS_CACHE_H
#include "library_cache.h"
#include <time.h>
#define SF_ART_MISSES 512
typedef struct {char path[1024];uint64_t signature;} SfArtMiss;
static SfArtMiss sf_art_miss[SF_ART_MISSES];
static int sf_art_loaded,sf_art_dirty;
typedef struct {char path[1024];uint64_t signature;time_t checked;int valid;} SfArtParent;
static SfArtParent sf_art_parents[32];
static int sf_art_parent_signature(const char *path,uint64_t *signature) {
    char parent[1024];if(strlen(path)>=sizeof(parent))return 0;
    strcpy(parent,path);char *slash=strrchr(parent,'/');if(!slash)return 0;*slash=0;
    unsigned slot=(unsigned)(sf_hash(parent,strlen(parent))%32);
    SfArtParent *memo=&sf_art_parents[slot];time_t now=time(NULL);
    if(!strcmp(memo->path,parent) && now>=memo->checked && now-memo->checked<2) {
        *signature=memo->signature;return memo->valid;
    }
    snprintf(memo->path,sizeof(memo->path),"%s",parent);memo->checked=now;memo->valid=0;
    /* Include nearest existing ancestor when images/ itself is absent. */
    while(!sf_directory_signature(parent,0,signature)) {
        if(errno!=ENOENT)return 0;
        slash=strrchr(parent,'/');if(!slash || slash==parent)return 0;*slash=0;
    }
    memo->signature=*signature;memo->valid=1;
    return 1;
}
static void sf_art_cache_load(void) {
    if(sf_art_loaded)return;sf_art_loaded=1;
    FILE *f=fopen(SF_CACHE_DIR "/art-misses.bin","rb");if(!f)return;
    uint64_t magic=0,checksum=0;
    if(fread(&magic,8,1,f)!=1 || magic!=UINT64_C(0x5346415254303031) ||
       fread(&checksum,8,1,f)!=1 || fread(sf_art_miss,sizeof(sf_art_miss),1,f)!=1 ||
       fgetc(f)!=EOF || checksum!=sf_hash(sf_art_miss,sizeof(sf_art_miss))) memset(sf_art_miss,0,sizeof(sf_art_miss));
    fclose(f);
    for(int i=0;i<SF_ART_MISSES;i++) if(!memchr(sf_art_miss[i].path,0,sizeof(sf_art_miss[i].path)))sf_art_miss[i].path[0]=0;
}
static int sf_art_exists(const char *path) {
    sf_art_cache_load();uint64_t signature=0;int known=0;
    unsigned slot=(unsigned)(sf_hash(path,strlen(path))%SF_ART_MISSES);
    SfArtMiss *m=&sf_art_miss[slot];
    if(!strcmp(m->path,path)) {
        known=sf_art_parent_signature(path,&signature);
        if(known && signature==m->signature)return 0;
    }
    if(access(path,F_OK)==0)return 1;
    if(errno==ENOENT && strlen(path)<sizeof(m->path) &&
       (known || sf_art_parent_signature(path,&signature))) {
        strcpy(m->path,path);m->signature=signature;sf_art_dirty=1;
    }
    return 0;
}
static void sf_art_cache_clear(void) {
    memset(sf_art_miss,0,sizeof(sf_art_miss));memset(sf_art_parents,0,sizeof(sf_art_parents));
    sf_art_loaded=1;sf_art_dirty=1;
}
static void sf_art_cache_save(void) {
    if(!sf_art_dirty)return;
    SF_MKDIR(SF_CACHE_DIR);FILE *f=fopen(SF_CACHE_DIR "/art-misses.tmp","wb");if(!f)return;
    uint64_t magic=UINT64_C(0x5346415254303031),checksum=sf_hash(sf_art_miss,sizeof(sf_art_miss));
    int ok=fwrite(&magic,8,1,f)==1 && fwrite(&checksum,8,1,f)==1 && fwrite(sf_art_miss,sizeof(sf_art_miss),1,f)==1;
    if(fflush(f)||ferror(f))ok=0;if(fclose(f))ok=0;
    if(ok && !rename(SF_CACHE_DIR "/art-misses.tmp",SF_CACHE_DIR "/art-misses.bin"))sf_art_dirty=0;
    else remove(SF_CACHE_DIR "/art-misses.tmp");
}
#endif
