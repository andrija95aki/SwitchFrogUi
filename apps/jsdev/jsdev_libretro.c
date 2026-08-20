#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "../../frogui/libretro.h"
#include "third_party/duktape/duktape.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "../../frogui/stb_truetype.h"

#define WIDTH 640
#define HEIGHT 480
#define FPS 60.0
#define AUDIO_RATE 44100
#define AUDIO_FRAMES (AUDIO_RATE / 60)
#define MAX_SCRIPT_SIZE (1024 * 1024)
#define MAX_VOICES 8
#define MAX_FONT_CACHES 4

static retro_environment_t environ_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

static uint16_t *framebuffer;
static duk_context *js;
static char script_path[1024];
static char project_name[128];
static char storage_path[1024];
static char runtime_error[1024];
static uint64_t frame_number;
static uint16_t input_now, input_before;

typedef struct {
    bool active;
    double phase;
    double step;
    int frames_left;
    float volume;
    int wave;
    uint32_t noise;
} Voice;
static Voice voices[MAX_VOICES];

typedef struct {
    unsigned char *bitmap;
    int width, height, xoff, yoff, advance;
} Glyph;
typedef struct {
    int size;
    Glyph glyphs[95];
} FontCache;
static unsigned char *font_data;
static stbtt_fontinfo font_info;
static bool font_ready;
static FontCache font_caches[MAX_FONT_CACHES];
static int font_cache_next;

static const char *button_names[16] = {
    "B", "Y", "SELECT", "START", "UP", "DOWN", "LEFT", "RIGHT",
    "A", "X", "L1", "R1", "L2", "R2", "L3", "R3"
};

static void set_error(const char *message) {
    if (!message) message = "Unknown JavaScript error";
    snprintf(runtime_error, sizeof(runtime_error), "%s", message);
    fprintf(stderr, "JSDev: %s\n", runtime_error);
}

static void mkdir_p(const char *path) {
    char copy[1024];
    size_t length;
    if (!path || !*path) return;
    snprintf(copy, sizeof(copy), "%s", path);
    length = strlen(copy);
    if (length && copy[length - 1] == '/') copy[length - 1] = '\0';
    for (char *p = copy + 1; *p; p++) {
        if (*p != '/') continue;
        *p = '\0'; mkdir(copy, 0755); *p = '/';
    }
    mkdir(copy, 0755);
}

static void sanitize_component(const char *source, char *target, size_t capacity) {
    size_t out = 0;
    if (!source) source = "project";
    for (; *source && out + 1 < capacity; source++) {
        unsigned char c = (unsigned char)*source;
        target[out++] = (isalnum(c) || c == '-' || c == '_' || c == '.') ? (char)c : '_';
    }
    if (!out && capacity > 1) snprintf(target, capacity, "data");
    else target[out] = '\0';
}

static uint16_t rgb565(unsigned color) {
    unsigned r = (color >> 16) & 255, g = (color >> 8) & 255, b = color & 255;
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static unsigned parse_color(duk_context *ctx, duk_idx_t index, unsigned fallback) {
    if (duk_is_number(ctx, index)) return duk_get_uint(ctx, index) & 0xffffffu;
    if (duk_is_string(ctx, index)) {
        const char *s = duk_get_string(ctx, index);
        if (*s == '#') s++;
        if (!strcasecmp(s, "white")) return 0xffffff;
        if (!strcasecmp(s, "black")) return 0x000000;
        if (!strcasecmp(s, "red")) return 0xff3344;
        if (!strcasecmp(s, "green")) return 0x35e07a;
        if (!strcasecmp(s, "blue")) return 0x4388ff;
        if (!strcasecmp(s, "yellow")) return 0xffdd40;
        if (!strcasecmp(s, "cyan")) return 0x30e6ee;
        if (!strcasecmp(s, "magenta")) return 0xf25cff;
        return (unsigned)strtoul(s, NULL, 16) & 0xffffffu;
    }
    return fallback;
}

static void put_pixel(int x, int y, uint16_t color) {
    if ((unsigned)x < WIDTH && (unsigned)y < HEIGHT) framebuffer[y * WIDTH + x] = color;
}

static void draw_line(int x0, int y0, int x1, int y1, uint16_t color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    for (;;) {
        put_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int twice = error * 2;
        if (twice >= dy) { error += dy; x0 += sx; }
        if (twice <= dx) { error += dx; y0 += sy; }
    }
}

static void draw_rect(int x, int y, int w, int h, uint16_t color, bool filled) {
    if (w < 0) { x += w; w = -w; }
    if (h < 0) { y += h; h = -h; }
    if (filled) {
        int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
        int x1 = x + w > WIDTH ? WIDTH : x + w, y1 = y + h > HEIGHT ? HEIGHT : y + h;
        for (int py = y0; py < y1; py++)
            for (int px = x0; px < x1; px++) framebuffer[py * WIDTH + px] = color;
    } else if (w > 0 && h > 0) {
        draw_line(x, y, x + w - 1, y, color);
        draw_line(x, y + h - 1, x + w - 1, y + h - 1, color);
        draw_line(x, y, x, y + h - 1, color);
        draw_line(x + w - 1, y, x + w - 1, y + h - 1, color);
    }
}

static void draw_circle(int cx, int cy, int radius, uint16_t color, bool filled) {
    int x = radius, y = 0, error = 1 - radius;
    if (radius < 0) radius = -radius;
    while (x >= y) {
        if (filled) {
            draw_line(cx - x, cy + y, cx + x, cy + y, color);
            draw_line(cx - x, cy - y, cx + x, cy - y, color);
            draw_line(cx - y, cy + x, cx + y, cy + x, color);
            draw_line(cx - y, cy - x, cx + y, cy - x, color);
        } else {
            put_pixel(cx + x, cy + y, color); put_pixel(cx + y, cy + x, color);
            put_pixel(cx - y, cy + x, color); put_pixel(cx - x, cy + y, color);
            put_pixel(cx - x, cy - y, color); put_pixel(cx - y, cy - x, color);
            put_pixel(cx + y, cy - x, color); put_pixel(cx + x, cy - y, color);
        }
        y++;
        if (error < 0) error += 2 * y + 1;
        else { x--; error += 2 * (y - x + 1); }
    }
}

static void free_font_cache(FontCache *cache) {
    for (int i = 0; i < 95; i++) { free(cache->glyphs[i].bitmap); cache->glyphs[i].bitmap = NULL; }
    cache->size = 0;
}

static void load_font(void) {
    static const char *paths[] = {
        "/mnt/sdcard/frogui/fonts/SpaceMono-Regular.ttf",
        "/mnt/sdcard/frogui/fonts/ShareTechMono-Regular.ttf",
        "/mnt/sdcard/frogui/fonts/monogram.ttf", NULL
    };
    for (int i = 0; paths[i] && !font_ready; i++) {
        FILE *file = fopen(paths[i], "rb");
        long length;
        if (!file) continue;
        fseek(file, 0, SEEK_END); length = ftell(file); rewind(file);
        if (length <= 0 || !(font_data = malloc((size_t)length))) { fclose(file); continue; }
        if (fread(font_data, 1, (size_t)length, file) != (size_t)length) {
            fclose(file); free(font_data); font_data = NULL; continue;
        }
        fclose(file);
        font_ready = stbtt_InitFont(&font_info, font_data, stbtt_GetFontOffsetForIndex(font_data, 0)) != 0;
        if (!font_ready) { free(font_data); font_data = NULL; }
    }
}

static FontCache *get_font_cache(int size) {
    if (size < 8) size = 8;
    if (size > 72) size = 72;
    for (int i = 0; i < MAX_FONT_CACHES; i++) if (font_caches[i].size == size) return &font_caches[i];
    FontCache *cache = &font_caches[font_cache_next++ % MAX_FONT_CACHES];
    free_font_cache(cache); cache->size = size;
    if (!font_ready) load_font();
    if (!font_ready) return cache;
    float scale = stbtt_ScaleForPixelHeight(&font_info, (float)size);
    for (int c = 32; c <= 126; c++) {
        Glyph *glyph = &cache->glyphs[c - 32];
        int advance, bearing;
        stbtt_GetCodepointHMetrics(&font_info, c, &advance, &bearing);
        glyph->advance = (int)(advance * scale + 0.5f);
        glyph->bitmap = stbtt_GetCodepointBitmap(&font_info, 0, scale, c,
            &glyph->width, &glyph->height, &glyph->xoff, &glyph->yoff);
    }
    return cache;
}

static void draw_text(int x, int y, const char *text, uint16_t color, int size) {
    FontCache *cache = get_font_cache(size);
    int origin = x, baseline = y + size;
    if (!font_ready || !text) return;
    while (*text) {
        unsigned char c = (unsigned char)*text++;
        if (c == '\n') { x = origin; baseline += size + 3; continue; }
        if (c < 32 || c > 126) c = '?';
        Glyph *glyph = &cache->glyphs[c - 32];
        for (int gy = 0; gy < glyph->height; gy++) for (int gx = 0; gx < glyph->width; gx++) {
            int px = x + glyph->xoff + gx, py = baseline + glyph->yoff + gy;
            unsigned alpha = glyph->bitmap ? glyph->bitmap[gy * glyph->width + gx] : 0;
            if (!alpha || (unsigned)px >= WIDTH || (unsigned)py >= HEIGHT) continue;
            uint16_t *dest = &framebuffer[py * WIDTH + px];
            if (alpha == 255) *dest = color;
            else {
                int fr=(color>>11)&31, fg=(color>>5)&63, fb=color&31;
                int br=(*dest>>11)&31, bg=(*dest>>5)&63, bb=*dest&31, inv=255-(int)alpha;
                *dest=(uint16_t)((((fr*(int)alpha+br*inv)/255)<<11)|
                    (((fg*(int)alpha+bg*inv)/255)<<5)|((fb*(int)alpha+bb*inv)/255));
            }
        }
        x += glyph->advance;
    }
}

static duk_ret_t api_on(duk_context *ctx) {
    char property[80];
    const char *event = duk_require_string(ctx, 0);
    duk_require_function(ctx, 1);
    snprintf(property, sizeof(property), "callbacks:%s", event);
    duk_push_global_stash(ctx);
    if (!duk_get_prop_string(ctx, -1, property) || !duk_is_array(ctx, -1)) {
        duk_pop(ctx); duk_push_array(ctx); duk_dup(ctx, -1); duk_put_prop_string(ctx, -3, property);
    }
    duk_uarridx_t index = (duk_uarridx_t)duk_get_length(ctx, -1);
    duk_dup(ctx, 1); duk_put_prop_index(ctx, -2, index);
    return 0;
}

static duk_ret_t api_clear(duk_context *ctx) {
    uint16_t color = rgb565(parse_color(ctx, 0, 0));
    for (int i = 0; i < WIDTH * HEIGHT; i++) framebuffer[i] = color;
    return 0;
}
static duk_ret_t api_pixel(duk_context *ctx) {
    put_pixel(duk_to_int(ctx,0),duk_to_int(ctx,1),rgb565(parse_color(ctx,2,0xffffff))); return 0;
}
static duk_ret_t api_line(duk_context *ctx) {
    draw_line(duk_to_int(ctx,0),duk_to_int(ctx,1),duk_to_int(ctx,2),duk_to_int(ctx,3),
        rgb565(parse_color(ctx,4,0xffffff))); return 0;
}
static duk_ret_t api_rect(duk_context *ctx) {
    draw_rect(duk_to_int(ctx,0),duk_to_int(ctx,1),duk_to_int(ctx,2),duk_to_int(ctx,3),
        rgb565(parse_color(ctx,4,0xffffff)), duk_get_boolean_default(ctx,5,1)); return 0;
}
static duk_ret_t api_circle(duk_context *ctx) {
    draw_circle(duk_to_int(ctx,0),duk_to_int(ctx,1),duk_to_int(ctx,2),
        rgb565(parse_color(ctx,3,0xffffff)), duk_get_boolean_default(ctx,4,1)); return 0;
}
static duk_ret_t api_text(duk_context *ctx) {
    draw_text(duk_to_int(ctx,0),duk_to_int(ctx,1),duk_safe_to_string(ctx,2),
        rgb565(parse_color(ctx,3,0xffffff)), duk_get_int_default(ctx,4,18)); return 0;
}
static duk_ret_t api_time(duk_context *ctx) { duk_push_number(ctx, (duk_double_t)frame_number * 1000.0 / FPS); return 1; }

static int button_id(const char *name) {
    for (int i = 0; i < 16; i++) if (!strcasecmp(name, button_names[i])) return i;
    if (!strcasecmp(name,"L")) return RETRO_DEVICE_ID_JOYPAD_L;
    if (!strcasecmp(name,"R")) return RETRO_DEVICE_ID_JOYPAD_R;
    return -1;
}
static duk_ret_t api_is_down(duk_context *ctx) {
    int id = button_id(duk_require_string(ctx,0));
    duk_push_boolean(ctx, id >= 0 && (input_now & (1u << id))); return 1;
}

static duk_ret_t api_beep(duk_context *ctx) {
    double frequency = duk_get_number_default(ctx,0,440.0);
    int duration = duk_get_int_default(ctx,1,150);
    double volume = duk_get_number_default(ctx,2,0.25);
    const char *wave = duk_get_string_default(ctx,3,"square");
    if (frequency < 20) frequency = 20; if (frequency > 12000) frequency = 12000;
    if (duration < 1) duration = 1; if (duration > 10000) duration = 10000;
    if (volume < 0) volume = 0; if (volume > 1) volume = 1;
    int slot = 0;
    for (int i = 0; i < MAX_VOICES; i++) if (!voices[i].active) { slot = i; break; }
    Voice *voice = &voices[slot];
    memset(voice,0,sizeof(*voice)); voice->active=true; voice->step=frequency/AUDIO_RATE;
    voice->frames_left=duration*AUDIO_RATE/1000; voice->volume=(float)volume; voice->noise=0x12345678u+slot;
    voice->wave = !strcasecmp(wave,"sine") ? 1 : !strcasecmp(wave,"triangle") ? 2 :
        !strcasecmp(wave,"noise") ? 3 : 0;
    return 0;
}
static duk_ret_t api_stop_audio(duk_context *ctx) { (void)ctx; memset(voices,0,sizeof(voices)); return 0; }

static int storage_file(const char *key, char *path, size_t capacity) {
    char safe[160]; sanitize_component(key,safe,sizeof(safe));
    return snprintf(path,capacity,"%s/%s.json",storage_path,safe) < (int)capacity;
}
static duk_ret_t api_storage_set(duk_context *ctx) {
    char path[1200], temp[1240]; FILE *file;
    const char *key = duk_require_string(ctx,0);
    if (!storage_file(key,path,sizeof(path))) { duk_push_false(ctx); return 1; }
    duk_dup(ctx,1); duk_json_encode(ctx,-1);
    const char *json = duk_get_string(ctx,-1);
    snprintf(temp,sizeof(temp),"%s.tmp",path);
    file=fopen(temp,"wb");
    if (!file) { duk_pop(ctx); duk_push_false(ctx); return 1; }
    fwrite(json,1,strlen(json),file); fflush(file); fsync(fileno(file)); fclose(file);
    if (rename(temp,path)!=0) { unlink(temp); duk_pop(ctx); duk_push_false(ctx); return 1; }
    duk_pop(ctx); duk_push_true(ctx); return 1;
}
static duk_ret_t api_storage_get(duk_context *ctx) {
    char path[1200]; FILE *file; long length; char *data;
    const char *key=duk_require_string(ctx,0);
    if (!storage_file(key,path,sizeof(path)) || !(file=fopen(path,"rb"))) {
        if (duk_get_top(ctx)>1) duk_dup(ctx,1); else duk_push_undefined(ctx); return 1;
    }
    fseek(file,0,SEEK_END); length=ftell(file); rewind(file);
    if (length<0 || length>262144 || !(data=malloc((size_t)length+1))) {
        fclose(file); if(duk_get_top(ctx)>1)duk_dup(ctx,1);else duk_push_undefined(ctx); return 1;
    }
    size_t got=fread(data,1,(size_t)length,file); fclose(file); data[got]='\0';
    duk_push_lstring(ctx,data,got); free(data); duk_json_decode(ctx,-1); return 1;
}
static duk_ret_t api_storage_remove(duk_context *ctx) {
    char path[1200]; const char *key=duk_require_string(ctx,0);
    duk_push_boolean(ctx,storage_file(key,path,sizeof(path)) && (unlink(path)==0 || errno==ENOENT)); return 1;
}
static duk_ret_t api_console_log(duk_context *ctx) {
    FILE *file; duk_idx_t top=duk_get_top(ctx);
    char path[1200]; snprintf(path,sizeof(path),"%s/console.log",storage_path);
    file=fopen(path,"ab");
    if (file) {
        for (duk_idx_t i=0;i<top;i++) fprintf(file,"%s%s",i?" ":"",duk_safe_to_string(ctx,i));
        fputc('\n',file); fclose(file);
    }
    return 0;
}

static void put_c_function(duk_context *ctx, const char *name, duk_c_function fn, duk_idx_t nargs) {
    duk_push_c_function(ctx,fn,nargs); duk_put_prop_string(ctx,-2,name);
}

static void register_api(duk_context *ctx) {
    duk_push_object(ctx);
    put_c_function(ctx,"on",api_on,2); put_c_function(ctx,"time",api_time,0);

    duk_push_object(ctx);
    duk_push_int(ctx,WIDTH); duk_put_prop_string(ctx,-2,"width");
    duk_push_int(ctx,HEIGHT); duk_put_prop_string(ctx,-2,"height");
    put_c_function(ctx,"clear",api_clear,1); put_c_function(ctx,"pixel",api_pixel,3);
    put_c_function(ctx,"line",api_line,5); put_c_function(ctx,"rect",api_rect,6);
    put_c_function(ctx,"circle",api_circle,5); put_c_function(ctx,"text",api_text,5);
    duk_put_prop_string(ctx,-2,"graphics");

    duk_push_object(ctx); put_c_function(ctx,"isDown",api_is_down,1); duk_put_prop_string(ctx,-2,"input");
    duk_push_object(ctx); put_c_function(ctx,"beep",api_beep,4); put_c_function(ctx,"stopAll",api_stop_audio,0);
    duk_put_prop_string(ctx,-2,"audio");
    duk_push_object(ctx); put_c_function(ctx,"get",api_storage_get,2); put_c_function(ctx,"set",api_storage_set,2);
    put_c_function(ctx,"remove",api_storage_remove,1); duk_put_prop_string(ctx,-2,"storage");
    duk_put_global_string(ctx,"JSDev");

    duk_push_object(ctx); put_c_function(ctx,"log",api_console_log,DUK_VARARGS); duk_put_global_string(ctx,"console");
}

static const char *bootstrap_source =
"(function(g){\n"
" var timers=[], nextTimer=1;\n"
" g.setTimeout=function(fn,ms){timers.push({id:nextTimer,fn:fn,left:+ms||0,every:0});return nextTimer++;};\n"
" g.setInterval=function(fn,ms){ms=Math.max(1,+ms||0);timers.push({id:nextTimer,fn:fn,left:ms,every:ms});return nextTimer++;};\n"
" g.clearTimeout=g.clearInterval=function(id){for(var i=timers.length-1;i>=0;i--)if(timers[i].id===id)timers.splice(i,1);};\n"
" JSDev.on('update',function(dt){for(var i=timers.length-1;i>=0;i--){var t=timers[i];t.left-=dt*1000;"
"if(t.left<=0){t.fn();if(t.every)t.left+=t.every;else timers.splice(i,1);}}});\n"
"})(this);\n";

static void call_event_number(const char *event, double value, bool with_value) {
    char property[80]; duk_idx_t top;
    if (!js || runtime_error[0]) return;
    top=duk_get_top(js); snprintf(property,sizeof(property),"callbacks:%s",event);
    duk_push_global_stash(js);
    if (!duk_get_prop_string(js,-1,property) || !duk_is_array(js,-1)) { duk_set_top(js,top); return; }
    duk_uarridx_t count=(duk_uarridx_t)duk_get_length(js,-1);
    for (duk_uarridx_t i=0;i<count;i++) {
        if (!duk_get_prop_index(js,-1,i) || !duk_is_callable(js,-1)) { duk_pop(js); continue; }
        if (with_value) duk_push_number(js,value);
        if (duk_pcall(js,with_value?1:0)!=DUK_EXEC_SUCCESS) {
            set_error(duk_safe_to_string(js,-1)); duk_set_top(js,top); return;
        }
        duk_pop(js);
    }
    duk_set_top(js,top);
}

static void call_event_string(const char *event, const char *value) {
    char property[80]; duk_idx_t top;
    if (!js || runtime_error[0]) return;
    top=duk_get_top(js); snprintf(property,sizeof(property),"callbacks:%s",event);
    duk_push_global_stash(js);
    if (!duk_get_prop_string(js,-1,property) || !duk_is_array(js,-1)) { duk_set_top(js,top); return; }
    duk_uarridx_t count=(duk_uarridx_t)duk_get_length(js,-1);
    for (duk_uarridx_t i=0;i<count;i++) {
        if (!duk_get_prop_index(js,-1,i) || !duk_is_callable(js,-1)) { duk_pop(js); continue; }
        duk_push_string(js,value);
        if (duk_pcall(js,1)!=DUK_EXEC_SUCCESS) { set_error(duk_safe_to_string(js,-1)); duk_set_top(js,top); return; }
        duk_pop(js);
    }
    duk_set_top(js,top);
}

static int read_file(const char *path, char **data_out, size_t *length_out) {
    FILE *file=fopen(path,"rb"); long length; char *data;
    if(!file)return 0; fseek(file,0,SEEK_END);length=ftell(file);rewind(file);
    if(length<0||length>MAX_SCRIPT_SIZE||!(data=malloc((size_t)length+1))){fclose(file);return 0;}
    size_t got=fread(data,1,(size_t)length,file);fclose(file);data[got]='\0';
    *data_out=data;*length_out=got;return 1;
}

static void audio_run(void) {
    static int16_t samples[AUDIO_FRAMES * 2];
    for (int i=0;i<AUDIO_FRAMES;i++) {
        float mixed=0;
        for(int v=0;v<MAX_VOICES;v++) if(voices[v].active) {
            Voice *voice=&voices[v]; float sample;
            if(voice->wave==1) sample=(float)sinf((float)(voice->phase*6.28318530718));
            else if(voice->wave==2) sample=(float)(4.0*fabs(voice->phase-0.5)-1.0);
            else if(voice->wave==3) { voice->noise=voice->noise*1664525u+1013904223u; sample=((voice->noise>>16)&65535)/32768.0f-1.0f; }
            else sample=voice->phase<0.5?1.0f:-1.0f;
            mixed+=sample*voice->volume; voice->phase+=voice->step; if(voice->phase>=1.0)voice->phase-=1.0;
            if(--voice->frames_left<=0)voice->active=false;
        }
        if(mixed>1)mixed=1;if(mixed<-1)mixed=-1;
        samples[i*2]=samples[i*2+1]=(int16_t)(mixed*14000.0f);
    }
    if(audio_batch_cb)audio_batch_cb(samples,AUDIO_FRAMES);
    else if(audio_cb)for(int i=0;i<AUDIO_FRAMES;i++)audio_cb(samples[i*2],samples[i*2+1]);
}

void retro_init(void) { framebuffer=calloc(WIDTH*HEIGHT,sizeof(uint16_t)); }
void retro_deinit(void) {
    if(js){duk_destroy_heap(js);js=NULL;} free(framebuffer);framebuffer=NULL;
    for(int i=0;i<MAX_FONT_CACHES;i++)free_font_cache(&font_caches[i]);
    free(font_data);font_data=NULL;font_ready=false;
}
unsigned retro_api_version(void){return RETRO_API_VERSION;}
void retro_set_environment(retro_environment_t cb){environ_cb=cb;}
void retro_set_video_refresh(retro_video_refresh_t cb){video_cb=cb;}
void retro_set_audio_sample(retro_audio_sample_t cb){audio_cb=cb;}
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb){audio_batch_cb=cb;}
void retro_set_input_poll(retro_input_poll_t cb){input_poll_cb=cb;}
void retro_set_input_state(retro_input_state_t cb){input_state_cb=cb;}
void retro_get_system_info(struct retro_system_info *info){
    memset(info,0,sizeof(*info));info->library_name="SwitchFrogUI JSDev";info->library_version="1.0";
    info->valid_extensions="js";info->need_fullpath=true;info->block_extract=false;
}
void retro_get_system_av_info(struct retro_system_av_info *info){
    memset(info,0,sizeof(*info));info->geometry.base_width=WIDTH;info->geometry.base_height=HEIGHT;
    info->geometry.max_width=WIDTH;info->geometry.max_height=HEIGHT;info->geometry.aspect_ratio=4.0f/3.0f;
    info->timing.fps=FPS;info->timing.sample_rate=AUDIO_RATE;
}
void retro_reset(void){frame_number=0;input_now=input_before=0;memset(voices,0,sizeof(voices));}

bool retro_load_game(const struct retro_game_info *info){
    char *source=NULL;size_t length=0;unsigned format=RETRO_PIXEL_FORMAT_RGB565;
    runtime_error[0]='\0';frame_number=0;memset(voices,0,sizeof(voices));
    if(environ_cb&&!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT,&format)){set_error("RGB565 is not supported");return false;}
    if(!framebuffer||!info||!info->path){set_error("No JavaScript project was selected");return false;}
    snprintf(script_path,sizeof(script_path),"%s",info->path);
    const char *base=strrchr(script_path,'/');base=base?base+1:script_path;
    snprintf(project_name,sizeof(project_name),"%s",base);char *dot=strrchr(project_name,'.');if(dot)*dot='\0';
    char safe_project[128];sanitize_component(project_name,safe_project,sizeof(safe_project));
    snprintf(storage_path,sizeof(storage_path),"/mnt/sdcard/roms/JSDev/.data/%s",safe_project);mkdir_p(storage_path);
    if(!read_file(script_path,&source,&length)){set_error("Could not read script (maximum size is 1 MiB)");return false;}
    js=duk_create_heap_default();if(!js){free(source);set_error("Could not create JavaScript heap");return false;}
    register_api(js);
    if(duk_peval_string(js,bootstrap_source)!=DUK_EXEC_SUCCESS){set_error(duk_safe_to_string(js,-1));duk_pop(js);free(source);return true;}
    duk_pop(js);
    if(duk_peval_lstring(js,source,length)!=DUK_EXEC_SUCCESS)set_error(duk_safe_to_string(js,-1));
    duk_pop(js);free(source);
    call_event_number("load",0,false);return true;
}
void retro_unload_game(void){if(js){call_event_number("unload",0,false);duk_destroy_heap(js);js=NULL;}memset(voices,0,sizeof(voices));}

void retro_run(void){
    input_before=input_now;input_now=0;if(input_poll_cb)input_poll_cb();
    if(input_state_cb)for(int i=0;i<16;i++)if(input_state_cb(0,RETRO_DEVICE_JOYPAD,0,(unsigned)i))input_now|=(uint16_t)(1u<<i);
    uint16_t changed=input_now^input_before;
    for(int i=0;i<16;i++)if(changed&(1u<<i))call_event_string((input_now&(1u<<i))?"buttondown":"buttonup",button_names[i]);
    call_event_number("update",1.0/FPS,true);call_event_number("draw",0,false);
    if(runtime_error[0]){
        draw_rect(12,12,WIDTH-24,100,rgb565(0x3a0810),true);draw_rect(12,12,WIDTH-24,100,rgb565(0xff4964),false);
        draw_text(24,20,"JSDEV SCRIPT ERROR",rgb565(0xff7088),20);
        draw_text(24,48,runtime_error,rgb565(0xffffff),14);
        draw_text(24,82,"Fix the script in Text Editor, then relaunch it.",rgb565(0xffd0d8),13);
    }
    audio_run();if(video_cb)video_cb(framebuffer,WIDTH,HEIGHT,WIDTH*sizeof(uint16_t));frame_number++;
}

size_t retro_serialize_size(void){return 0;}bool retro_serialize(void*d,size_t s){(void)d;(void)s;return false;}
bool retro_unserialize(const void*d,size_t s){(void)d;(void)s;return false;}void retro_cheat_reset(void){}
void retro_cheat_set(unsigned i,bool e,const char*c){(void)i;(void)e;(void)c;}void*retro_get_memory_data(unsigned id){(void)id;return NULL;}
size_t retro_get_memory_size(unsigned id){(void)id;return 0;}void retro_set_controller_port_device(unsigned p,unsigned d){(void)p;(void)d;}
