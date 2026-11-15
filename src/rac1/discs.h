/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * What the mod uses of the game, on each disc it knows: the globals it reads and writes and the
 * game function it calls, by the name each disc's build of the port gives it (openrac_api.function).
 * - PAL v2.00 (SCES-50916, "pal"): rac1-decomp's own addresses and names (include/names/,
 *   config/names.tsv); OpenRAC's PAL build names functions as rac1-decomp does.
 * - NTSC-U v1.00 (SCUS-97199, "ntsc"): the same globals and function at the US addresses, from the
 *   NTSC decomp (OpenRAC's games/rac1/ntsc-decomp) and the C OpenRAC's NTSC build generates from it
 *   (port/build/release/games/rac1-ntsc/gen), which spells each global as the game reads it.
 * DESIGN.md, section 5, has the evidence for each row.
 */
#ifndef RAC1_TRAVEL_ANYWHERE_DISCS_H
#define RAC1_TRAVEL_ANYWHERE_DISCS_H

#include <stdint.h>

typedef struct ta_disc {
    const char* version; /* openrac_api.version */

    /* The executable's, used as they are (below 0x15F000: no level program moves them). */
    uint32_t hero; /* g_hero */
    uint32_t ship; /* the ship's record (level_render_state): +0 its moby, +0x20 its state */
    uint32_t current_level; /* g_current_level_index */
    uint32_t freeze_flags;  /* mode_freeze_flags: a dialog the level loop opens next */
    uint32_t freeze_mask;   /* the bits of it handle_pause_input opens a dialog for */
    uint32_t card;          /* memory_card_state */
    uint32_t card_state;    /* its operation: 3 and over busy */
    uint32_t card_pending;  /* its queued operation: under 0 none */

    /* The executable's, through relocate_data (a level program has its own copy). */
    uint32_t menu;            /* menu_system: +0 state, +8 next page, +0xC close request, ... */
    uint32_t selected_level;  /* selected_level_index: the planet the select's cursor is on */
    uint32_t camera_suppress; /* camera_position_publication_suppressed: PauseAllSounds bails */

    /* Level 0's program's, through relocate_level_data(0, ...). */
    uint32_t mode;         /* the level loop's game mode (cinematic_mode): 0 play */
    uint32_t mode_ticks;   /* ticks in that mode (cinematic_mode_frames) */
    uint32_t level_exit;   /* nonzero: the level loop ends (a travel on its way) */
    uint32_t ship_request; /* ship_level_exit_requested: Triangle at the ship, its take-off next */
    uint32_t debug;        /* the debug menu's record: +8 its update flags, +0xEC its step count */

    /* The game function it calls. */
    const char* open_menu; /* void (int state): PauseAllSounds, the menu opened in that state */
} ta_disc;

static const ta_disc k_ta_discs[] = {
    {
        "pal",
        0x0013F450u, /* g_hero */
        0x0013E130u, /* resident_data_0013E130 (level_render_state) */
        0x0015EE84u, /* g_current_level_index */
        0x0015EFB4u, /* mode_freeze_flags */
        0x00000401u, /* handle_pause_input: mode_freeze_flags & 0x401 */
        0x0013D390u, /* memory_card_state */
        0xDCu,       /* its +0xDC, which the PAL pause and map want under 3 */
        0xE4u,       /* its +0xE4, which they want under 0 */
        0x001D5F70u, /* menu_system */
        0x001A0414u, /* selected_level_index */
        0x0018C42Cu, /* camera_position_publication_suppressed */
        0x0015F6A8u, /* D_L00_0015F6A8 cinematic_mode */
        0x0015F6ACu, /* D_L00_0015F6AC cinematic_mode_frames */
        0x0015F650u, /* D_L00_0015F650 level_loop_exit_requested */
        0x0015F710u, /* D_L00_0015F710 ship_level_exit_requested */
        0x0016C158u, /* D_L00_0016C158 gDebugFreeCamLvl */
        "func_00219C70",
    },
    {
        "ntsc",
        0x0013F350u,
        0x0013E030u,
        0x0015ED84u,
        0x0015EEB4u,
        0x00000001u, /* the NTSC handle_pause_input: mode_freeze_flags & 1 */
        0x0013D290u,
        0xD4u, /* state: the level loop's card icon shows from 3 */
        0xDCu, /* pending_state: and from 0 */
        0x001D5BF0u,
        0x001A0314u,
        0x0018C32Cu,
        0x0015F5C4u,
        0x0015F5C8u,
        0x0015F570u,
        0x0015F630u,
        0x0016C058u,
        "FUN_00218d78",
    },
};

#endif /* RAC1_TRAVEL_ANYWHERE_DISCS_H */
