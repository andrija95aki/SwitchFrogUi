#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../frogui/libretro.h"

static unsigned video_frames;
static unsigned audio_frames;

static bool environment(unsigned command, void *data) {
    (void)data;
    return command == RETRO_ENVIRONMENT_SET_PIXEL_FORMAT;
}
static void video(const void *data, unsigned width, unsigned height, size_t pitch) {
    if (data && width == 640 && height == 480 && pitch == 1280) video_frames++;
}
static size_t audio_batch(const int16_t *data, size_t frames) {
    if (data) audio_frames += (unsigned)frames;
    return frames;
}
static void input_poll(void) {}
static int16_t input_state(unsigned port, unsigned device, unsigned index, unsigned id) {
    (void)port; (void)device; (void)index; (void)id; return 0;
}

int main(int argc, char **argv) {
    struct retro_game_info game;
    if (argc != 2) { fprintf(stderr, "usage: jsdev_smoke <project.js>\n"); return 2; }
    memset(&game, 0, sizeof(game)); game.path = argv[1];
    retro_set_environment(environment); retro_set_video_refresh(video);
    retro_set_audio_sample_batch(audio_batch); retro_set_input_poll(input_poll);
    retro_set_input_state(input_state); retro_init();
    if (!retro_load_game(&game)) { retro_deinit(); return 3; }
    for (int frame = 0; frame < 180; frame++) retro_run();
    retro_unload_game(); retro_deinit();
    if (video_frames != 180 || audio_frames != 180 * 735) {
        fprintf(stderr, "unexpected callbacks: video=%u audio=%u\n", video_frames, audio_frames);
        return 4;
    }
    printf("JSDev smoke passed: %u video frames, %u audio frames\n", video_frames, audio_frames);
    return 0;
}
