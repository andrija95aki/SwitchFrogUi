#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
int sf_unicode_text(uint16_t*,int,int,int,int,const char*,uint16_t,int,const char*);
static uint16_t guarded[640*480+2];
int main(void) {
    const char *samples[]={"Zażółć gęślą jaźń", "مرحبا بالعالم 123", "שלום עולם", "Русский текст", "日本語", "bad \xff utf8"};
    for(int cycle=0;cycle<60;cycle++)for(unsigned i=0;i<sizeof(samples)/sizeof(*samples);i++) {
        memset(guarded,0,sizeof(guarded));guarded[0]=guarded[640*480+1]=0xace1;
        int size=14+cycle%30;
        int measured=sf_unicode_text(NULL,0,0,0,0,samples[i],0xffff,size,"alium_Nunito.ttf");
        int drawn=sf_unicode_text(guarded+1,640,480,12,16,samples[i],0xffff,size,"alium_Nunito.ttf");
        assert(measured>0 && measured==drawn);
        unsigned ink=0;for(int p=1;p<=640*480;p++)ink+=guarded[p]!=0;
        assert(ink>0);
        sf_unicode_text(guarded+1,640,480,-20,470,samples[i],0xffff,size,"alium_Nunito.ttf");
        assert(guarded[0]==0xace1 && guarded[640*480+1]==0xace1);
    }
    char longtext[1024];memset(longtext,'a',sizeof(longtext)-1);longtext[1023]=0;
    assert(sf_unicode_text(NULL,0,0,0,0,longtext,0,20,"alium_Nunito.ttf")==-1);
    puts("PASS: 360 Unicode/RTL size cycles, draw/measure, clipping and bounds");
    return 0;
}
