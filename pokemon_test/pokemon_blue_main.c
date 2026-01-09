/* Main entry point */
#include "pokemon_blue.h"
#include "gbrt.h"
#ifdef GB_HAS_SDL2
#include "platform_sdl.h"
#endif
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[]) {
    // Parse args
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--trace") == 0) {
            gbrt_trace_enabled = true;
            printf("Trace enabled\n");
        } else if (strcmp(argv[i], "--limit") == 0 && i + 1 < argc) {
            gbrt_instruction_limit = strtoull(argv[++i], NULL, 10);
            printf("Instruction limit: %llu\n", (unsigned long long)gbrt_instruction_limit);
        }
    }

    GBContext* ctx = gb_context_create(NULL);
    if (!ctx) {
        fprintf(stderr, "Failed to create context\n");
        return 1;
    }
    pokemon_blue_init(ctx);

#ifdef GB_HAS_SDL2
    // Initialize SDL2 platform with 3x scaling
    if (!gb_platform_init(3)) {
        fprintf(stderr, "Failed to initialize platform\n");
        gb_context_destroy(ctx);
        return 1;
    }

    // Run the game loop
    while (1) {
        gb_run_frame(ctx);
        if (!gb_platform_poll_events(ctx)) break;
        if (ctx->frame_done) {
            const uint32_t* fb = gb_get_framebuffer(ctx);
            if (fb) gb_platform_render_frame(fb);
            gb_reset_frame(ctx);
            ctx->stopped = 0;
            gb_platform_vsync();
        }
    }
    gb_platform_shutdown();
#else
    // No SDL2 - just run for testing
    pokemon_blue_run(ctx);
    printf("Recompiled code executed successfully!\n");
    printf("Registers: A=%02X B=%02X C=%02X\n", ctx->a, ctx->b, ctx->c);
#endif

    gb_context_destroy(ctx);
    return 0;
}
