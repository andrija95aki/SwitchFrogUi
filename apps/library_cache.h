/* SwitchFrogUI directory snapshots. No stat ABI dependency: fingerprints use
 * directory names/types, not struct stat from a different MIPS libc. Errors
 * disable caching; paths and data are checked before accepting a disk cache. */
#ifndef SWITCHFROG_LIBRARY_CACHE_H
#define SWITCHFROG_LIBRARY_CACHE_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#ifdef _WIN32
#define SF_MKDIR(path) mkdir(path)
#else
#define SF_MKDIR(path) mkdir(path,0777)
#endif

#ifndef SF_CACHE_DIR
#define SF_CACHE_DIR "/mnt/sdcard/frogui/.cache"
#endif
static uint64_t sf_hash(const void *data,size_t len) {
    const unsigned char *p=data;uint64_t h=UINT64_C(14695981039346656037);
    while(len--) {h^=*p++;h*=UINT64_C(1099511628211);}return h;
}
static uint64_t sf_file_hash(const char *path) {
    FILE *f=fopen(path,"rb");if(!f)return 0;
    unsigned char data[512];size_t n;uint64_t h=0;
    while((n=fread(data,1,sizeof(data),f))) h=(h<<1)^sf_hash(data,n);
    int failed=ferror(f);fclose(f);return failed ? 0 : h;
}
static int sf_directory_signature(const char *path,int recursive,uint64_t *signature) {
    if(recursive<0)return 0;
    DIR *d=opendir(path);if(!d)return 0;
    struct dirent *e;uint64_t sum=0,xors=0,count=0;int ok=1;
    for(;;) {
        errno=0;e=readdir(d);if(!e){if(errno)ok=0;break;}
        if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;
        uint64_t h=sf_hash(e->d_name,strlen(e->d_name));
        char child[1024];
        if(snprintf(child,sizeof(child),"%s/%s",path,e->d_name)>=(int)sizeof(child)){ok=0;break;}
        int isdir=0;
#ifdef DT_DIR
        if(e->d_type==DT_LNK){ok=0;break;} /* no cached symlink traversals */
        isdir=e->d_type==DT_DIR;
        if(e->d_type==DT_UNKNOWN)
#endif
        {DIR *probe=opendir(child);if(probe){isdir=1;closedir(probe);}}
        h^=(uint64_t)isdir<<63;
        if(isdir && recursive && e->d_name[0]!='.' &&
           strcasecmp(e->d_name,"images") && strcasecmp(e->d_name,"Imgs") &&
           strcasecmp(e->d_name,"media") && strcasecmp(e->d_name,"boxart")) {
            uint64_t sub;
            if(!sf_directory_signature(child,recursive-1,&sub)){ok=0;break;}
            h^=sub;
        }
        sum+=h;xors^=h;count++;
    }
    closedir(d);*signature=sum^(xors<<1)^count;return ok;
}
typedef struct { uint64_t magic,signature,checksum; uint32_t item_size,count;char path[1024]; } SfCacheHeader;
static void sf_cache_name(const char *path,unsigned mode,char *out,size_t n) {
    snprintf(out,n,SF_CACHE_DIR "/%016llx-%u.dir",(unsigned long long)sf_hash(path,strlen(path)),mode);
}
static void *sf_cache_load(const char *path,unsigned mode,uint64_t signature,size_t size,int *count) {
    char file[1200];sf_cache_name(path,mode,file,sizeof(file));
    FILE *f=fopen(file,"rb");if(!f)return NULL;
    SfCacheHeader h;void *data=NULL;
    if(fread(&h,sizeof(h),1,f)!=1 || h.magic!=UINT64_C(0x5346444952433031) ||
       !memchr(h.path,0,sizeof(h.path)) || strcmp(h.path,path) || h.signature!=signature ||
       h.item_size!=size || !h.count || h.count>16000 || size>1024)goto done;
    size_t bytes=(size_t)h.count*size;
    data=malloc(bytes);if(!data)goto done;
    if(fread(data,1,bytes,f)!=bytes || fgetc(f)!=EOF || ferror(f) || sf_hash(data,bytes)!=h.checksum) {
        free(data);data=NULL;goto done;
    }
    *count=(int)h.count;
done:fclose(f);return data;
}
static void sf_cache_store(const char *path,unsigned mode,uint64_t signature,const void *data,size_t size,int count) {
    if(count<1 || count>16000 || strlen(path)>=1024 || size>1024)return;
    char file[1200],temporary[1210];sf_cache_name(path,mode,file,sizeof(file));
    snprintf(temporary,sizeof(temporary),"%s.tmp",file);
    SF_MKDIR(SF_CACHE_DIR);
    FILE *f=fopen(temporary,"wb");if(!f)return;
    SfCacheHeader h={0};h.magic=UINT64_C(0x5346444952433031);h.signature=signature;
    h.item_size=(uint32_t)size;h.count=(uint32_t)count;snprintf(h.path,sizeof(h.path),"%s",path);
    size_t bytes=size*(size_t)count;h.checksum=sf_hash(data,bytes);
    int ok=fwrite(&h,sizeof(h),1,f)==1 && fwrite(data,1,bytes,f)==bytes;
    if(fflush(f)||ferror(f))ok=0;if(fclose(f))ok=0;
    if(!ok || rename(temporary,file))remove(temporary);
}
#endif
