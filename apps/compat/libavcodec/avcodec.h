#ifndef TREEFROGUI_MINIMAL_AVCODEC_H
#define TREEFROGUI_MINIMAL_AVCODEC_H
/* Minimal ABI declarations used by the vendor ffplayer interface. */
#include <stdint.h>
typedef struct AVDictionary AVDictionary;

enum AVSubtitleType {
    SUBTITLE_NONE,
    SUBTITLE_BITMAP,
    SUBTITLE_TEXT,
    SUBTITLE_ASS
};

typedef struct AVPicture {
    uint8_t *data[8];
    int linesize[8];
} AVPicture;

typedef struct AVSubtitleRect {
    int x, y, w, h, nb_colors;
    AVPicture pict;
    uint8_t *data[4];
    int linesize[4];
    enum AVSubtitleType type;
    char *text;
    char *ass;
    int flags;
} AVSubtitleRect;

typedef struct AVSubtitle {
    uint16_t format;
    uint32_t start_display_time;
    uint32_t end_display_time;
    unsigned num_rects;
    AVSubtitleRect **rects;
    int64_t pts;
} AVSubtitle;
#endif
