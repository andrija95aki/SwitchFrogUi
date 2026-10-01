#ifndef SF_PS1_COMPAT_H
#define SF_PS1_COMPAT_H
#include <stdio.h>
#include <string.h>
#include <ctype.h>
/* A bounded CSV reader: quoted commas and escaped quotes, no partial records. */
static int sf_csv_fields(const char *s, char fields[][512], int capacity) {
    int n=0;
    while(*s && n<capacity) {
        int quoted=0,used=0,closed=0;
        if(*s=='"'){quoted=1;s++;}
        while(*s && *s!='\r' && *s!='\n') {
            if(quoted && *s=='"') {
                if(s[1]=='"'){s++;}
                else {s++;closed=1;break;}
            } else if(!quoted && *s==',') break;
            if(used>=511)return -1;
            fields[n][used++]=*s++;
        }
        fields[n++][used]=0;
        if(quoted && !closed)return -1;
        if(*s==','){s++;if(!*s && n<capacity)fields[n++][0]=0;}
        else if(*s && *s!='\r' && *s!='\n')return -1;
        else break;
    }
    return n;
}
static void sf_disc_id(const char *s,char out[16]) {
    unsigned n=0;
    while(*s && n<15) {
        if(isalnum((unsigned char)*s))out[n++]=(char)toupper((unsigned char)*s);
        s++;
    }
    out[n]=0;
}
static int sf_ps1_compat_lookup(const char *csv,const char *id,int index,
                              char fields[15][512]) {
    char wanted[16],line[8192];int found=0;
    sf_disc_id(id,wanted);if(strlen(wanted)!=9)return 0;
    FILE *f=fopen(csv,"rb");if(!f)return 0;
    while(fgets(line,sizeof(line),f)) {
        if(!strchr(line,'\n') && !feof(f)){fclose(f);return 0;}
        if(sf_csv_fields(line,fields,15)!=15)continue;
        char current[16];sf_disc_id(fields[2],current);
        if(!strcmp(current,wanted) && found++==index){fclose(f);return 1;}
    }
    fclose(f);return 0;
}
#endif
