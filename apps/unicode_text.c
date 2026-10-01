/* Optional bounded Unicode renderer. ASCII stays on the existing transactional
 * STB/fallback path; failures here must never invalidate that path.
 * Shaping pipeline inspired by TreeFrogUI: FreeType + HarfBuzz + SheenBidi. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>
#include <SheenBidi/SheenBidi.h>
#ifndef SF_FONT_ROOT
#define SF_FONT_ROOT "/mnt/sdcard/frogui/fonts"
#endif
typedef struct { FT_Face ft; hb_font_t *hb; int size; } Face;
static FT_Library library;
static Face faces[3];
static char primary_name[256];
static hb_buffer_t *buffer;
static int open_face(Face *out,const char *name) {
    char path[512];FT_Face ft;
    if(!name || strchr(name,'/') || strchr(name,'\\'))return 0;
    snprintf(path,sizeof(path),SF_FONT_ROOT "/%s",name);
    if(FT_New_Face(library,path,0,&ft))return 0;
    hb_font_t *hb=hb_ft_font_create_referenced(ft);
    if(hb==hb_font_get_empty()){FT_Done_Face(ft);return 0;}
    if(out->hb)hb_font_destroy(out->hb);
    if(out->ft)FT_Done_Face(out->ft);
    out->ft=ft;out->hb=hb;out->size=0;return 1;
}
static int prepare(const char *name,int pixels) {
    if(!library && FT_Init_FreeType(&library))return 0;
    if(!buffer)buffer=hb_buffer_create();
    if(!buffer || !hb_buffer_allocation_successful(buffer))return 0;
    if(strcmp(primary_name,name) && open_face(&faces[0],name))
        snprintf(primary_name,sizeof(primary_name),"%s",name);
    if(!faces[1].ft)open_face(&faces[1],"DejaVuSans.ttf");
    if(!faces[2].ft)open_face(&faces[2],"TreeFrogUnicode.ttf");
    int count=0;
    for(int i=0;i<3;i++)if(faces[i].ft) {
        count++;
        if(faces[i].size!=pixels) {
            if(FT_Set_Pixel_Sizes(faces[i].ft,0,pixels))return 0;
            hb_ft_font_changed(faces[i].hb);faces[i].size=pixels;
        }
    }
    return count>0;
}
static int pick(uint32_t cp) {
    for(int i=0;i<3;i++)if(faces[i].ft && FT_Get_Char_Index(faces[i].ft,cp))return i;
    for(int i=0;i<3;i++)if(faces[i].ft)return i;
    return -1;
}
static uint32_t utf8(const unsigned char **p) {
    unsigned c=*(*p)++;if(c<128)return c;
    unsigned need=c>=0xc2&&c<0xe0?1:c>=0xe0&&c<0xf0?2:c>=0xf0&&c<0xf5?3:0;
    if(!need)return 0xfffd;
    uint32_t value=c&((1u<<(6-need))-1),minimum=need==1?0x80:need==2?0x800:0x10000;
    for(unsigned i=0;i<need;i++) {
        if((**p&0xc0)!=0x80)return 0xfffd;
        value=(value<<6)|(*(*p)++&0x3f);
    }
    return value<minimum || value>0x10ffff || (value>=0xd800&&value<=0xdfff)?0xfffd:value;
}
static void blend(uint16_t *dst,uint16_t color,unsigned a) {
    unsigned b=*dst,ia=255-a;
    *dst=(uint16_t)(((((color>>11)*a+(b>>11)*ia)/255)<<11)|
        (((((color>>5)&63)*a+((b>>5)&63)*ia)/255)<<5)|
        (((color&31)*a+(b&31)*ia)/255));
}
/* Return width, or -1 for a recoverable error. Bounded 512-codepoint lines and
 * 8 lines prevent malformed labels consuming unbounded RAM/time. */
int sf_unicode_text(uint16_t *fb,int w,int h,int x,int y,const char *text,
                    uint16_t color,int pixels,const char *name) {
    if(!text || !name || pixels<4 || pixels>120 || !prepare(name,pixels))return -1;
    int max_width=0,lines=0;
    const unsigned char *p=(const unsigned char*)text;
    while(*p && lines++<8) {
        uint32_t cp[512];int face_id[512];unsigned count=0;
        while(*p && *p!='\n' && count<512){cp[count]=utf8(&p);face_id[count]=pick(cp[count]);count++;}
        if(*p && *p!='\n')return -1;
        if(*p=='\n')p++;
        SBCodepointSequence seq={SBStringEncodingUTF32,cp,count};
        SBAlgorithmRef algo=SBAlgorithmCreate(&seq);
        if(!algo)return -1;
        SBParagraphRef para=SBAlgorithmCreateParagraph(algo,0,count,SBLevelDefaultLTR);
        SBLineRef line=para?SBParagraphCreateLine(para,0,count):NULL;
        int cursor=0;
        if(line) {
            const SBRun *runs=SBLineGetRunsPtr(line);
            for(SBUInteger r=0;r<SBLineGetRunCount(line);r++) {
                unsigned start=(unsigned)runs[r].offset,end=start+(unsigned)runs[r].length;
                int rtl=runs[r].level&1;
                unsigned remaining=end-start;
                while(remaining) {
                    unsigned a=rtl?start+remaining-1:end-remaining,b;
                    int fi=face_id[a];
                    if(rtl){b=a+1;while(a>start && face_id[a-1]==fi)a--;}
                    else {b=a+1;while(b<end && face_id[b]==fi)b++;}
                    remaining-=b-a;
                    if(fi<0)continue;
                    Face *face=&faces[fi];
                    hb_buffer_clear_contents(buffer);
                    hb_buffer_add_utf32(buffer,cp,count,a,b-a);
                    hb_buffer_set_direction(buffer,rtl?HB_DIRECTION_RTL:HB_DIRECTION_LTR);
                    hb_buffer_guess_segment_properties(buffer);
                    hb_shape(face->hb,buffer,NULL,0);
                    if(!hb_buffer_allocation_successful(buffer)){SBLineRelease(line);SBParagraphRelease(para);SBAlgorithmRelease(algo);return -1;}
                    unsigned n;
                    hb_glyph_info_t *info=hb_buffer_get_glyph_infos(buffer,&n);
                    hb_glyph_position_t *pos=hb_buffer_get_glyph_positions(buffer,NULL);
                    for(unsigned g=0;g<n;g++) {
                        if(fb && !FT_Load_Glyph(face->ft,info[g].codepoint,FT_LOAD_DEFAULT) &&
                           !FT_Render_Glyph(face->ft->glyph,FT_RENDER_MODE_NORMAL)) {
                            FT_GlyphSlot glyph=face->ft->glyph;FT_Bitmap *bm=&glyph->bitmap;
                            int dx=x+((cursor+pos[g].x_offset)>>6)+glyph->bitmap_left;
                            int dy=y+(face->ft->size->metrics.ascender>>6)-glyph->bitmap_top-(pos[g].y_offset>>6);
                            if(bm->pixel_mode==FT_PIXEL_MODE_GRAY)
                            for(unsigned row=0;row<bm->rows;row++) {
                                int yy=dy+(int)row;if(yy<0||yy>=h)continue;
                                const unsigned char *src=bm->buffer+(bm->pitch<0?(bm->rows-1-row)*(-bm->pitch):row*bm->pitch);
                                for(unsigned col=0;col<bm->width;col++) {
                                    int xx=dx+(int)col;if(xx>=0&&xx<w&&src[col])blend(&fb[yy*w+xx],color,src[col]);
                                }
                            }
                        }
                        cursor+=pos[g].x_advance;
                    }
                }
            }
            SBLineRelease(line);
        }
        if(para)SBParagraphRelease(para);
        SBAlgorithmRelease(algo);
        if((cursor+32)/64>max_width)max_width=(cursor+32)/64;
        y+=pixels+4;
    }
    return max_width;
}
