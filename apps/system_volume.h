/* Shared cubevol volume ABI; upstream TreeFrogUI PicoArch, adapted for
 * SwitchFrog's perceptual curve. Polling never writes persistent storage. */
#ifndef SF_SYSTEM_VOLUME_H
#define SF_SYSTEM_VOLUME_H
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
struct sf_volume_request { unsigned short flag, id, len, pad; void *buf; };
static int sf_volume_gain(int level) {
    static const unsigned char gain[]={0,1,2,3,4,5,6,7,8,10,13,16,20,25,32,40,50,63,75,87,100};
    if(level<=0)return 0;
    if(level>=100)return 100;
    int p=level/5,r=level%5;
    return (gain[p]*(5-r)+gain[p+1]*r+2)/5;
}
static int sf_volume_read(void) {
    unsigned char buf[260]={0};
    int fd=open("/dev/persistentmem",O_RDONLY);
    if(fd<0)return -1;
    struct sf_volume_request req={3,0,260,0,buf};
    int rc=ioctl(fd,0x400c2602u,&req);
    close(fd);
    return rc==0 && buf[0]<=100 ? buf[0] : -1;
}
static int sf_volume_write(int level) {
    if(level<0 || level>100)return -1;
    unsigned char byte=(unsigned char)level;
    int current=sf_volume_read();
    /* Unknown hardware/ABI: use the legacy software-gain path, no blind SET. */
    if(current<0)return -1;
    if(current!=level) {
        int fd=open("/dev/persistentmem",O_RDWR);
        if(fd<0)return -1;
        struct sf_volume_request req={3,0,1,0,&byte};
        int rc=ioctl(fd,0x800c2603u,&req);
        close(fd);
        if(rc<0)return -1;
    }
    int fd=open("/dev/sndC0i2so",O_WRONLY);
    if(fd>=0){ioctl(fd,0x8001080bu,&byte);close(fd);}
    return 0;
}
#endif
