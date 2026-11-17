/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Loads the built library the way the port loads it (GUIDE.md section 12), against a host table of
 * the test's own, once for each row of src/rac1/discs.h. It checks the query, the refusals, the
 * hook point and callbacks the mod takes, and what its pad and tick callbacks and its hook do to
 * game memory laid out at that row's addresses. Stubs stand in for PauseAllSounds and ShipTravelTo.
 *
 *     rac1_travel_anywhere_load_test <path to the library>
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define LIB_OPEN(p) ((void*)LoadLibraryA(p))
#define LIB_SYM(h, n) ((void*)GetProcAddress((HMODULE)(h), (n)))
#else
#include <dlfcn.h>
#define LIB_OPEN(p) dlopen((p), RTLD_NOW | RTLD_LOCAL)
#define LIB_SYM(h, n) dlsym((h), (n))
#endif

#include "check.h"
#include "openrac/games/rac1.h"
#include "openrac/keys.h"
#include "openrac/openrac_mod.h"
#include "rac1/discs.h"
#include "rac1/rac1.h"
#include "rac1/travel.h"
#include "version.h"

/* ---- Game memory and host state ---- */

#define MEMORY_SIZE 0x00400000u
static uint8_t* g_mem;

static const char* g_game = "rac1";
static const char* g_version = "ntsc";
static int g_level = 3;
static char g_fail[512];
static char g_last_log[1024];

/* The settings the next load reads. */
static const char* s_combo = "l1_r1_select";
static const char* s_key = "";
static int s_trace;

/* The disc row under test. */
static const ta_disc* g_disc;

/* Where the test places the objects that game pointers point at. */
#define SHIP_MOBY 0x00300000u
#define HERO_MOBY 0x00310000u
#define PAGE_PLANETS 0x00320000u
#define PAGE_CONFIRM 0x00320100u

static int32_t geti(uint32_t a) {
    int32_t v;
    memcpy(&v, g_mem + a, 4);
    return v;
}

static void puti(uint32_t a, int32_t v) {
    memcpy(g_mem + a, &v, 4);
}

static void putb(uint32_t a, uint8_t v) {
    g_mem[a] = v;
}

#define HERO(off) (g_disc->hero + (off))
#define MENU(off) (g_disc->menu + (off))

/* Resets game memory to Ratchet standing in play on Kerwan (level 3), with his ship parked, the
 * card idle and Kerwan and Novalis in the ship's list. */
static void reset_game(void) {
    memset(g_mem, 0, MEMORY_SIZE);
    g_level = 3;
    puti(g_disc->current_level, 3);
    puti(g_disc->galaxy_list, 3);
    puti(g_disc->galaxy_list + 4, 1);
    puti(g_disc->mode, 0);
    puti(g_disc->mode_ticks, 500);
    puti(HERO(TA_HERO_MOBY), (int32_t)HERO_MOBY);
    puti(HERO(TA_HERO_HP), 4);
    puti(g_disc->ship + TA_SHIP_MOBY, (int32_t)SHIP_MOBY);
    puti(g_disc->card + g_disc->card_pending, -1);
    puti(g_disc->debug + TA_DEBUG_FLAGS, 0xF);
    puti(PAGE_PLANETS + TA_PAGE_STATE, TA_MENU_PLANETS);
    puti(PAGE_CONFIRM + TA_PAGE_STATE, TA_MENU_CONFIRM);
}

/* ---- The hook point ---- */

static void* g_travel; /* the mod's replacement of ShipTravelTo */
static int g_travel_calls, g_travel_dest;
static int g_hooks_taken;

/* A stub of the game's ShipTravelTo. It always sets game mode 6. Another planet (or none) takes the
 * travel branch, which sets the level the loop flies to when it ends; the planet being played
 * takes the landing. */
static void orig_travel(int32_t dest) {
    g_travel_calls++;
    g_travel_dest = dest;
    puti(g_disc->mode, 6);

    // The travel branch.
    if (dest != geti(g_disc->current_level)) {
        puti(g_disc->load_level, dest);
    }
}

static int fake_hook(
    openrac_mod* mod, const char* name, const char* signature, void* replacement, void** original
) {
    (void)mod;

    // The one hook point the mod takes, by id or alias, as the port's loader accepts either.
    if (strcmp(name, "rac1.ship.travel_to") != 0
        && strcmp(name, OPENRAC_RAC1_HOOK_Ship_TravelTo) != 0) {
        return OPENRAC_HOOK_UNKNOWN;
    }

    // Refuse any signature but the SDK's, as the port does.
    if (strcmp(signature, OPENRAC_RAC1_SIG_Ship_TravelTo) != 0) {
        return OPENRAC_HOOK_SIGNATURE;
    }

    g_travel = replacement;
    *original = (void*)orig_travel;
    g_hooks_taken++;
    return OPENRAC_HOOK_OK;
}

static void travel(int32_t dest) {
    ((void (*)(int32_t))g_travel)(dest);
}

/* ---- The game function ---- */

static int g_open_calls, g_open_state;
static int g_camera_blocks; /* PauseAllSounds only notes the pause and returns */

/* A stub of the game's PauseAllSounds: sets the menu's state, clears its close request and sets
 * game mode 3. */
static void fn_open_menu(int32_t state) {
    g_open_calls++;
    g_open_state = state;

    // The camera holds the pause off.
    if (g_camera_blocks) {
        return;
    }

    puti(MENU(TA_MENU_STATE), state);
    puti(MENU(TA_MENU_CLOSE), 0);
    puti(g_disc->mode, 3);
}

static void* fake_function(const char* name) {
    return strcmp(name, g_disc->open_menu) == 0 ? (void*)fn_open_menu : NULL;
}

/* ---- The mod's callbacks ---- */

static openrac_frame_fn g_frame_fn;
static void* g_frame_user;
static openrac_pad_fn g_pad_fn;
static void* g_pad_user;
static int g_callbacks;

static int fake_on_frame(openrac_mod* mod, openrac_frame_fn fn, void* user) {
    (void)mod;
    g_frame_fn = fn;
    g_frame_user = user;
    g_callbacks++;
    return 0;
}

static int fake_on_pad(openrac_mod* mod, openrac_pad_fn fn, void* user) {
    (void)mod;
    g_pad_fn = fn;
    g_pad_user = user;
    g_callbacks++;
    return 0;
}

static void tick(void) {
    g_frame_fn(g_frame_user);
}

/* The last report passed to the pad callback, as the game then reads it. */
static uint8_t g_report[18];

/* Sends pad 0's report with these buttons held through the mod; returns what the game sees. */
static uint32_t poll(uint32_t held) {
    memset(g_report, 0xFF, 2);
    memset(g_report + 2, 0x80, 4);
    memset(g_report + 6, 0, 12);
    g_report[0] = (uint8_t)~(held & 0xFFu);
    g_report[1] = (uint8_t)~(held >> 8);
    g_pad_fn(g_pad_user, 0, g_report, 18);
    return ta_report_held(g_report);
}

/* ---- The rest of the host table ---- */

static openrac_input g_input;
static char g_notice[256];
static int g_notices;

static const openrac_input* fake_input(void) {
    return &g_input;
}

static int fake_play_status(openrac_play_status* out) {
    (void)out;
    return OPENRAC_HOOK_OK;
}

static int fake_play_notice(openrac_mod* mod, const char* text) {
    (void)mod;
    snprintf(g_notice, sizeof g_notice, "%s", text != NULL ? text : "");
    g_notices++;
    return OPENRAC_HOOK_OK;
}

static void fake_log(openrac_mod* mod, int level, const char* text) {
    (void)mod;
    (void)level;
    snprintf(g_last_log, sizeof g_last_log, "%s", text);
}

static void fake_fail(openrac_mod* mod, const char* reason) {
    (void)mod;
    snprintf(g_fail, sizeof g_fail, "%s", reason);
}

static int fake_bool(openrac_mod* mod, const char* key) {
    (void)mod;
    return strcmp(key, "trace") == 0 ? s_trace : 0;
}

static int64_t fake_int(openrac_mod* mod, const char* key) {
    (void)mod;
    (void)key;
    return 0;
}

static double fake_float(openrac_mod* mod, const char* key) {
    (void)mod;
    (void)key;
    return 0.0;
}

static const char* fake_string(openrac_mod* mod, const char* key) {
    (void)mod;

    // The combo's choice.
    if (strcmp(key, "combo") == 0) {
        return s_combo;
    }

    return strcmp(key, "key") == 0 ? s_key : "";
}

static openrac_gaddr identity(openrac_gaddr a) {
    return a;
}

static openrac_gaddr identity_level(int level, openrac_gaddr a) {
    (void)level;
    return a;
}

static int fake_loaded_level(void) {
    return g_level;
}

typedef void (*query_fn)(openrac_mod_info*);
typedef int (*load_fn)(const openrac_api*, openrac_mod*);
static load_fn g_load;

/* Loads the mod with the current settings. */
static int load_mod(void) {
    static openrac_api api;
    memset(&api, 0, sizeof api);
    api.size = sizeof api;
    api.api_major = OPENRAC_MOD_API_MAJOR;
    api.api_minor = OPENRAC_MOD_API_MINOR;
    api.game = g_game;
    api.version = g_version;
    api.openrac_version = "test";
    api.platform = "test";
    api.memory = g_mem;
    api.log = fake_log;
    api.fail = fake_fail;
    api.setting_bool = fake_bool;
    api.setting_int = fake_int;
    api.setting_float = fake_float;
    api.setting_string = fake_string;
    api.hook = fake_hook;
    api.on_frame = fake_on_frame;
    api.on_pad = fake_on_pad;
    api.function = fake_function;
    api.relocate_data = identity;
    api.relocate_level_data = identity_level;
    api.loaded_level = fake_loaded_level;
    api.input = fake_input;
    api.play_status = fake_play_status;
    api.play_notice = fake_play_notice;

    g_travel = NULL;
    g_hooks_taken = 0;
    g_callbacks = 0;
    g_frame_fn = NULL;
    g_pad_fn = NULL;
    g_fail[0] = 0;
    {
        const int r = g_load(&api, (openrac_mod*)&api);

        // Print why the load was refused, for the test's output.
        if (r != 0) {
            printf("load refused: %s\n", g_fail);
        }

        return r;
    }
}

static void reset_counters(void) {
    g_open_calls = g_travel_calls = g_notices = 0;
    g_open_state = g_travel_dest = -1;
    g_notice[0] = 0;
}

/* Presses the default combo (L1 and R1, then Select with them), with a tick after each poll. */
static void press_combo(void) {
    poll(TA_PAD_L1 | TA_PAD_R1);
    tick();
    poll(TA_PAD_L1 | TA_PAD_R1 | TA_PAD_SELECT);
    tick();
}

/* Shows `page` as the game's menu update leaves it after a page switch. */
static void menu_on(int page) {
    puti(MENU(TA_MENU_STATE), page);
    puti(MENU(TA_MENU_NEXT), 0);
    puti(MENU(TA_MENU_TIMER), 0);
}

/* Closes the menu for the ship on `selected`, as update_pause_menu's close_pause_menu does. */
static void menu_closing(int selected) {
    puti(MENU(TA_MENU_STATE), TA_MENU_CLOSING);
    puti(MENU(TA_MENU_CLOSE), TA_CLOSE_SHIP);
    puti(MENU(TA_MENU_TIMER), 2);
    puti(g_disc->menu_target, selected);
}

/* Runs the restore two ticks later, as update_pause_menu does for each close request: 2 calls the
 * ship's travel (here the mod's hook) and leaves the game mode to it; any other resumes play. */
static void menu_restore(void) {
    const int request = geti(MENU(TA_MENU_CLOSE));

    // The ship's travel.
    if (request == TA_CLOSE_SHIP) {
        travel(geti(g_disc->menu_target));
        return;
    }

    puti(g_disc->mode, 0);
}

/* The same restore as OpenRAC's PAL build runs it: the game's ShipTravelTo, without the hook. */
static void menu_restore_unhooked(void) {
    const int request = geti(MENU(TA_MENU_CLOSE));

    // The ship's travel, skipping the hook.
    if (request == TA_CLOSE_SHIP) {
        orig_travel(geti(g_disc->menu_target));
        return;
    }

    puti(g_disc->mode, 0);
}

/* ---- The tests ---- */

static void test_load_refusals(void) {
    // Another game.
    g_game = "rac2";
    CHECK_EQ(load_mod(), 1);
    CHECK_EQ(strstr(g_fail, "Ratchet & Clank") != NULL, 1);
    CHECK_EQ(g_hooks_taken, 0);
    CHECK_EQ(g_callbacks, 0);
    g_game = "rac1";

    // A disc with no row.
    {
        const char* kept = g_version;
        g_version = "jp";
        CHECK_EQ(load_mod(), 1);
        CHECK_EQ(strstr(g_fail, "jp") != NULL, 1);
        CHECK_EQ(g_hooks_taken, 0);
        CHECK_EQ(g_callbacks, 0);
        g_version = kept;
    }
}

static void test_takes(void) {
    CHECK_EQ(load_mod(), 0);
    CHECK_EQ(g_hooks_taken, 1);
    CHECK_EQ(g_travel != NULL, 1);
    CHECK_EQ(g_callbacks, 2);
    CHECK_EQ(strstr(g_last_log, "L1 + R1 + Select") != NULL, 1);
}

static void test_open_and_back_out(void) {
    reset_game();
    reset_counters();

    // The shoulders alone are the game's look and crouch; the mod does nothing.
    CHECK_EQ(poll(TA_PAD_L1 | TA_PAD_R1), TA_PAD_L1 | TA_PAD_R1);
    tick();
    CHECK_EQ(g_open_calls, 0);

    // Select with them is hidden from the game, and the menu opens at the end of the tick.
    CHECK_EQ(poll(TA_PAD_L1 | TA_PAD_R1 | TA_PAD_SELECT), TA_PAD_L1 | TA_PAD_R1);
    CHECK_EQ(g_open_calls, 0);
    tick();
    CHECK_EQ(g_open_calls, 1);
    CHECK_EQ(g_open_state, TA_MENU_SHIP_ENTRY);
    CHECK_EQ(geti(g_disc->mode), 3);
    CHECK_EQ(geti(g_disc->mode_ticks), 0);
    CHECK_EQ(g_notices, 0);

    // Select stays hidden while held; once released, the pad is the game's again.
    CHECK_EQ(poll(TA_PAD_SELECT), 0);
    CHECK_EQ(poll(0), 0);
    CHECK_EQ(poll(TA_PAD_CROSS), TA_PAD_CROSS);

    // On the list, Triangle closes it on the planet being played.
    menu_on(TA_MENU_PLANETS);
    poll(0);
    poll(TA_PAD_TRIANGLE);
    menu_closing(3);

    // The next poll turns the close into the map's, so play resumes in place.
    poll(0);
    CHECK_EQ(geti(MENU(TA_MENU_CLOSE)), TA_CLOSE_TO_PLAY);
    tick();
    menu_restore();
    CHECK_EQ(g_travel_calls, 0);
    CHECK_EQ(geti(g_disc->mode), 0);

    // The menu is over, so a later travel from the ship's own menu is the game's.
    tick();
    travel(3);
    CHECK_EQ(g_travel_calls, 1);
    CHECK_EQ(g_travel_dest, 3);
}

static void test_back_out_at_tick(void) {
    reset_game();
    reset_counters();
    press_combo();

    // Start on the confirm page closes on the planet being played.
    menu_on(TA_MENU_CONFIRM);
    poll(0);
    poll(TA_PAD_START);
    menu_closing(3);

    // With no poll before the restore, the end of the tick still steers the close.
    tick();
    CHECK_EQ(geti(MENU(TA_MENU_CLOSE)), TA_CLOSE_TO_PLAY);
    menu_restore();
    CHECK_EQ(g_travel_calls, 0);
}

static void test_travel(void) {
    reset_game();
    reset_counters();
    press_combo();

    // Cross on Novalis (level 1) in the list, then Cross on its confirm page.
    menu_on(TA_MENU_PLANETS);
    poll(0);
    poll(TA_PAD_CROSS);
    puti(MENU(TA_MENU_STATE), TA_MENU_SWITCHING);
    puti(MENU(TA_MENU_NEXT), (int32_t)PAGE_CONFIRM);
    puti(MENU(TA_MENU_TIMER), 6);
    poll(0);
    menu_on(TA_MENU_CONFIRM);
    poll(TA_PAD_CROSS);
    menu_closing(1);
    poll(0);
    tick();

    // The close stays the ship's, and the game's ShipTravelTo flies there.
    CHECK_EQ(geti(MENU(TA_MENU_CLOSE)), TA_CLOSE_SHIP);
    menu_restore();
    CHECK_EQ(g_travel_calls, 1);
    CHECK_EQ(g_travel_dest, 1);
}

static void test_cross_on_switch(void) {
    reset_game();
    reset_counters();
    press_combo();

    // Cross in the tick the switch to the confirm page ends goes to that page.
    puti(MENU(TA_MENU_STATE), TA_MENU_SWITCHING);
    puti(MENU(TA_MENU_NEXT), (int32_t)PAGE_CONFIRM);
    puti(MENU(TA_MENU_TIMER), 1);
    poll(TA_PAD_CROSS);
    menu_closing(3);
    tick();

    // The planet being played, chosen, is steered to the game's travel branch.
    CHECK_EQ(geti(MENU(TA_MENU_CLOSE)), TA_CLOSE_SHIP);
    CHECK_EQ(geti(g_disc->menu_target), TA_NO_PLANET);
    menu_restore();
    CHECK_EQ(g_travel_calls, 1);
    CHECK_EQ(g_travel_dest, TA_NO_PLANET);
    CHECK_EQ(geti(g_disc->load_level), 3);
}

static void test_restart(void) {
    reset_game();
    reset_counters();
    press_combo();

    // Cross on the confirm page of the planet being played.
    menu_on(TA_MENU_CONFIRM);
    poll(0);
    poll(TA_PAD_CROSS);
    menu_closing(3);
    poll(0);

    // While it closes, the close stays the ship's and its target becomes no planet.
    CHECK_EQ(geti(MENU(TA_MENU_CLOSE)), TA_CLOSE_SHIP);
    CHECK_EQ(geti(g_disc->menu_target), TA_NO_PLANET);
    tick();
    CHECK_EQ(geti(g_disc->menu_target), TA_NO_PLANET);

    // The restore calls the game's ShipTravelTo for no planet (its travel branch; the save resumes
    // on the planet being played), then the hook puts the load level back to that planet.
    menu_restore();
    CHECK_EQ(g_travel_calls, 1);
    CHECK_EQ(g_travel_dest, TA_NO_PLANET);
    CHECK_EQ(geti(g_disc->mode), 6);
    CHECK_EQ(geti(g_disc->load_level), 3);

    // The menu is over, and the next poll and tick change nothing.
    poll(0);
    tick();
    CHECK_EQ(geti(g_disc->load_level), 3);
    travel(1);
    CHECK_EQ(g_travel_dest, 1);
}

static void test_restart_unhooked(void) {
    reset_game();
    reset_counters();
    press_combo();

    // As on OpenRAC's PAL build, the game's ShipTravelTo runs without the hook.
    menu_on(TA_MENU_CONFIRM);
    poll(0);
    poll(TA_PAD_CROSS);
    menu_closing(3);
    tick();
    menu_restore_unhooked();
    CHECK_EQ(g_travel_dest, TA_NO_PLANET);
    CHECK_EQ(geti(g_disc->load_level), TA_NO_PLANET);

    // The next poll, during the ship's flight out (game mode 6), puts the planet back.
    poll(0);
    CHECK_EQ(geti(g_disc->load_level), 3);

    // Or the end of the tick, when no poll came first.
    reset_game();
    reset_counters();
    press_combo();
    menu_on(TA_MENU_CONFIRM);
    poll(0);
    poll(TA_PAD_CROSS);
    menu_closing(3);
    tick();
    menu_restore_unhooked();
    tick();
    CHECK_EQ(geti(g_disc->load_level), 3);

    // Loading the new level ends the menu.
    g_level = 4;
    tick();
    g_level = 3;
    poll(0);
}

static void test_restart_missed(void) {
    reset_game();
    reset_counters();
    press_combo();

    // The restore runs before any poll or tick end saw the close. The planet being played reaches
    // the hook, which leaves it to the game (the landing) rather than force a trip.
    menu_on(TA_MENU_CONFIRM);
    poll(0);
    poll(TA_PAD_CROSS);
    menu_closing(3);
    menu_restore();
    CHECK_EQ(g_travel_calls, 1);
    CHECK_EQ(g_travel_dest, 3);
    CHECK_EQ(geti(g_disc->load_level), 0);
    poll(0);
    tick();

    // An earlier mod turns the steered -1 back into the planet being played: the same, and no load
    // level is put back later (a Quit Game's -1 afterwards stays the title).
    reset_game();
    reset_counters();
    press_combo();
    menu_on(TA_MENU_CONFIRM);
    poll(0);
    poll(TA_PAD_CROSS);
    menu_closing(3);
    tick();
    CHECK_EQ(geti(g_disc->menu_target), TA_NO_PLANET);
    travel(3);
    CHECK_EQ(g_travel_dest, 3);
    puti(g_disc->load_level, TA_NO_PLANET);
    poll(0);
    tick();
    CHECK_EQ(geti(g_disc->load_level), TA_NO_PLANET);
}

static void test_landing_unlisted(void) {
    reset_game();
    reset_counters();

    // Veldin's first visit (level 0) is never in the ship's list: Blast Off there is the landing.
    g_level = 0;
    puti(g_disc->current_level, 0);
    puti(g_disc->galaxy_list, 0);
    press_combo();
    menu_on(TA_MENU_CONFIRM);
    poll(0);
    poll(TA_PAD_CROSS);
    menu_closing(0);
    poll(0);
    tick();
    CHECK_EQ(geti(g_disc->menu_target), 0);
    menu_restore();
    CHECK_EQ(g_travel_calls, 1);
    CHECK_EQ(g_travel_dest, 0);
    CHECK_EQ(geti(g_disc->load_level), 0);
    poll(0);
    tick();
}

static void test_hook_backstop(void) {
    reset_game();
    reset_counters();
    press_combo();

    // A back-out the poll and tick did not see (the restore came first): the hook resumes play in
    // place, as the map's close does, and calls no travel.
    menu_on(TA_MENU_PLANETS);
    poll(0);
    poll(TA_PAD_TRIANGLE);
    puti(MENU(TA_MENU_CLOSE), TA_CLOSE_SHIP);
    travel(3);
    CHECK_EQ(g_travel_calls, 0);
    CHECK_EQ(geti(g_disc->mode), 0);
    CHECK_EQ(geti(MENU(TA_MENU_CLOSE)), TA_CLOSE_TO_PLAY);

    // The menu is over, so the next travel is the game's.
    travel(3);
    CHECK_EQ(g_travel_calls, 1);
}

static void test_movie_round_trip(void) {
    reset_game();
    reset_counters();
    press_combo();

    // Circle on the confirm page plays the planet's infobot movie, then the menu comes back.
    menu_on(TA_MENU_CONFIRM);
    poll(0);
    poll(TA_PAD_CIRCLE);
    puti(MENU(TA_MENU_PENDING_PAGE), (int32_t)PAGE_CONFIRM);
    puti(g_disc->mode, 0);
    tick();
    puti(g_disc->mode, 1);
    tick();
    puti(g_disc->mode, 0);
    tick();

    // The game reopens the menu (PauseAllSounds). It is still the mod's: a back-out resumes play.
    puti(MENU(TA_MENU_PENDING_PAGE), 0);
    puti(g_disc->mode, 3);
    menu_on(TA_MENU_PLANETS);
    poll(0);
    poll(TA_PAD_TRIANGLE);
    menu_closing(3);
    tick();
    CHECK_EQ(geti(MENU(TA_MENU_CLOSE)), TA_CLOSE_TO_PLAY);
}

static void test_level_change(void) {
    reset_game();
    reset_counters();
    press_combo();

    // Another level loaded (a travel the hook did not see) ends the menu.
    g_level = 1;
    puti(g_disc->current_level, 1);
    tick();
    g_level = 3;
    puti(g_disc->current_level, 3);
    menu_on(TA_MENU_PLANETS);
    poll(TA_PAD_TRIANGLE);
    menu_closing(3);
    tick();
    CHECK_EQ(geti(MENU(TA_MENU_CLOSE)), TA_CLOSE_SHIP);
}

static void test_open_refusals(void) {
    // Down: Select is still hidden, nothing opens, and the notice says why, then clears.
    reset_game();
    reset_counters();
    puti(HERO(TA_HERO_HP), 0);
    poll(TA_PAD_L1 | TA_PAD_R1);
    CHECK_EQ(poll(TA_PAD_L1 | TA_PAD_R1 | TA_PAD_SELECT), TA_PAD_L1 | TA_PAD_R1);
    tick();
    CHECK_EQ(g_open_calls, 0);
    CHECK_EQ(geti(g_disc->mode), 0);
    CHECK_EQ(strcmp(g_notice, ta_why_text(TA_DOWN)), 0);
    poll(0);

    // The notice stays for its full time.
    for (int i = 0; i < 149; i++) {
        tick();
    }

    CHECK_EQ(strcmp(g_notice, ta_why_text(TA_DOWN)), 0);
    tick();
    CHECK_EQ(g_notice[0], 0);

    // Clank.
    reset_game();
    reset_counters();
    putb(HERO(TA_HERO_VEHICLE), 1);
    press_combo();
    CHECK_EQ(g_open_calls, 0);
    CHECK_EQ(strcmp(g_notice, ta_why_text(TA_CLANK)), 0);
    poll(0);

    // No ship.
    reset_game();
    reset_counters();
    puti(g_disc->ship + TA_SHIP_MOBY, 0);
    press_combo();
    CHECK_EQ(g_open_calls, 0);
    CHECK_EQ(strcmp(g_notice, ta_why_text(TA_NO_SHIP)), 0);
    poll(0);

    // The ship hidden by the story (on Novalis's first visit the dropship's update hides it).
    reset_game();
    reset_counters();
    g_mem[SHIP_MOBY + TA_MOBY_FLAGS] = 0x03;
    press_combo();
    CHECK_EQ(g_open_calls, 0);
    CHECK_EQ(strcmp(g_notice, ta_why_text(TA_NO_SHIP)), 0);
    poll(0);

    // Orxon's story scenes at the ship.
    reset_game();
    reset_counters();
    putb(SHIP_MOBY + TA_MOBY_STATE, TA_SHIP_MOBY_SCENES);
    press_combo();
    CHECK_EQ(g_open_calls, 0);
    CHECK_EQ(strcmp(g_notice, ta_why_text(TA_SHIP_BUSY)), 0);
    poll(0);

    // A dialog next (the disc's bits of the freeze flags).
    reset_game();
    reset_counters();
    puti(g_disc->freeze_flags, (int32_t)g_disc->freeze_mask);
    press_combo();
    CHECK_EQ(g_open_calls, 0);
    CHECK_EQ(strcmp(g_notice, ta_why_text(TA_DIALOG)), 0);
    poll(0);

    // The card busy (the disc's fields).
    reset_game();
    reset_counters();
    puti(g_disc->card + g_disc->card_pending, 0);
    press_combo();
    CHECK_EQ(g_open_calls, 0);
    CHECK_EQ(strcmp(g_notice, ta_why_text(TA_CARD)), 0);
    poll(0);

    // The debug menu stepping.
    reset_game();
    reset_counters();
    puti(g_disc->debug + TA_DEBUG_STEPS, 3);
    press_combo();
    CHECK_EQ(g_open_calls, 0);
    CHECK_EQ(strcmp(g_notice, ta_why_text(TA_STEPPING)), 0);
    poll(0);

    // While the camera holds the pause off, PauseAllSounds is never called.
    reset_game();
    reset_counters();
    puti(g_disc->camera_suppress, 1);
    press_combo();
    CHECK_EQ(g_open_calls, 0);
    CHECK_EQ(strcmp(g_notice, ta_why_text(TA_CAMERA)), 0);
    poll(0);

    // If PauseAllSounds opens nothing after all, the mod has no menu.
    reset_game();
    reset_counters();
    g_camera_blocks = 1;
    press_combo();
    CHECK_EQ(g_open_calls, 1);
    g_camera_blocks = 0;
    puti(g_disc->mode, 3);
    menu_on(TA_MENU_PLANETS);
    poll(0);
    poll(TA_PAD_TRIANGLE);
    menu_closing(3);
    tick();
    CHECK_EQ(geti(MENU(TA_MENU_CLOSE)), TA_CLOSE_SHIP);
    poll(0);
}

static void test_not_ours(void) {
    // In a cutscene the game keeps every button and the mod does nothing.
    reset_game();
    reset_counters();
    puti(g_disc->mode, 2);
    poll(TA_PAD_L1 | TA_PAD_R1);
    CHECK_EQ(poll(TA_PAD_L1 | TA_PAD_R1 | TA_PAD_SELECT), TA_PAD_L1 | TA_PAD_R1 | TA_PAD_SELECT);
    tick();
    CHECK_EQ(g_open_calls, 0);
    CHECK_EQ(g_notices, 0);
    poll(0);

    // The pause menu.
    puti(g_disc->mode, 3);
    poll(TA_PAD_L1 | TA_PAD_R1);
    CHECK_EQ(poll(TA_PAD_L1 | TA_PAD_R1 | TA_PAD_SELECT), TA_PAD_L1 | TA_PAD_R1 | TA_PAD_SELECT);
    tick();
    CHECK_EQ(g_open_calls, 0);
    poll(0);

    // A level loading.
    reset_game();
    g_level = -1;
    poll(TA_PAD_L1 | TA_PAD_R1);
    poll(TA_PAD_L1 | TA_PAD_R1 | TA_PAD_SELECT);
    tick();
    CHECK_EQ(g_open_calls, 0);
    poll(0);

    // In play, Select alone is the map's, and so is one shoulder with Select; pad 1 is ignored.
    reset_game();
    CHECK_EQ(poll(TA_PAD_SELECT), TA_PAD_SELECT);
    poll(0);
    CHECK_EQ(poll(TA_PAD_L1 | TA_PAD_SELECT), TA_PAD_L1 | TA_PAD_SELECT);
    poll(0);
    memset(g_report, 0xFF, 2);
    g_report[0] = (uint8_t)~TA_PAD_SELECT;
    g_report[1] = (uint8_t)~((TA_PAD_L1 | TA_PAD_R1) >> 8);
    g_pad_fn(g_pad_user, 1, g_report, 18);
    CHECK_EQ(ta_report_held(g_report), TA_PAD_L1 | TA_PAD_R1 | TA_PAD_SELECT);
    tick();
    CHECK_EQ(g_open_calls, 0);
}

static void test_other_combo(void) {
    s_combo = "l2_r2_select";
    CHECK_EQ(load_mod(), 0);
    CHECK_EQ(strstr(g_last_log, "L2 + R2 + Select") != NULL, 1);
    reset_game();
    reset_counters();

    // L1 and R1 do nothing now.
    press_combo();
    CHECK_EQ(g_open_calls, 0);
    poll(0);

    // L2 and R2 do.
    poll(TA_PAD_L2 | TA_PAD_R2);
    CHECK_EQ(poll(TA_PAD_L2 | TA_PAD_R2 | TA_PAD_SELECT), TA_PAD_L2 | TA_PAD_R2);
    tick();
    CHECK_EQ(g_open_calls, 1);
    poll(0);
    s_combo = "l1_r1_select";
}

static void test_key(void) {
    const int t = openrac_bind_from_name("KeyT");

    s_key = "KeyT";
    CHECK_EQ(load_mod(), 0);
    CHECK_EQ(strstr(g_last_log, "or the key KeyT") != NULL, 1);
    reset_game();
    reset_counters();
    memset(&g_input, 0, sizeof g_input);
    g_input.size = sizeof g_input;

    // The port's overlay has the keys: not the game's.
    g_input.keys[t] = OPENRAC_KEY_HELD | OPENRAC_KEY_PRESSED;
    g_input.overlay = 1;
    poll(0);
    tick();
    CHECK_EQ(g_open_calls, 0);

    // T pressed in play opens the menu, and no pad button is hidden.
    g_input.overlay = 0;
    CHECK_EQ(poll(TA_PAD_SELECT), TA_PAD_SELECT);
    tick();
    CHECK_EQ(g_open_calls, 1);
    memset(&g_input, 0, sizeof g_input);

    // The key alone, the combo off.
    s_combo = "off";
    CHECK_EQ(load_mod(), 0);
    CHECK_EQ(g_hooks_taken, 1);
    reset_game();
    reset_counters();
    press_combo();
    CHECK_EQ(g_open_calls, 0);
    poll(0);
    s_key = "";
}

static void test_nothing_set(void) {
    s_combo = "off";
    s_key = "";
    CHECK_EQ(load_mod(), 0);
    CHECK_EQ(g_hooks_taken, 0);
    CHECK_EQ(g_callbacks, 0);
    CHECK_EQ(strstr(g_last_log, "inactive") != NULL, 1);

    // An unknown choice falls back to the default.
    s_combo = "zz";
    CHECK_EQ(load_mod(), 0);
    CHECK_EQ(g_hooks_taken, 1);
    s_combo = "l1_r1_select";

    // With the trace on, the mod loads and works the same.
    s_trace = 1;
    CHECK_EQ(load_mod(), 0);
    CHECK_EQ(strstr(g_last_log, "trace on") != NULL, 1);
    reset_game();
    reset_counters();
    press_combo();
    CHECK_EQ(g_open_calls, 1);
    poll(0);
    s_trace = 0;
}

int main(int argc, char** argv) {
    void* lib;
    query_fn query;
    openrac_mod_info info;

    // The library's path is required.
    if (argc < 2) {
        printf("usage: %s <library>\n", argv[0]);
        return 2;
    }

    lib = LIB_OPEN(argv[1]);

    // The library must open.
    if (lib == NULL) {
        printf("cannot open %s\n", argv[1]);
        return 2;
    }

    query = (query_fn)LIB_SYM(lib, "openrac_mod_query");
    g_load = (load_fn)LIB_SYM(lib, "openrac_mod_load");

    // It must export both entry points.
    if (query == NULL || g_load == NULL) {
        printf("no openrac_mod_query or openrac_mod_load\n");
        return 2;
    }

    // The query: the manifest's id, version and interface ("api": "1.2").
    memset(&info, 0, sizeof info);
    query(&info);
    CHECK_EQ(strcmp(info.id, RAC1_TRAVEL_ANYWHERE_ID), 0);
    CHECK_EQ(strcmp(info.version, RAC1_TRAVEL_ANYWHERE_VERSION), 0);
    CHECK_EQ(info.api_major, 1);
    CHECK_EQ(info.api_minor, 2);

    g_mem = (uint8_t*)calloc(MEMORY_SIZE, 1);

    // Each disc's row, with its addresses inside the test's memory.
    for (unsigned i = 0; i < sizeof k_ta_discs / sizeof k_ta_discs[0]; i++) {
        g_disc = &k_ta_discs[i];
        g_version = g_disc->version;
        printf("disc %s\n", g_disc->version);
        CHECK_EQ(g_disc->menu + 0x148 < MEMORY_SIZE, 1);
        reset_game();
        test_load_refusals();
        test_takes();
        test_open_and_back_out();
        test_back_out_at_tick();
        test_travel();
        test_cross_on_switch();
        test_restart();
        test_restart_unhooked();
        test_restart_missed();
        test_landing_unlisted();
        test_hook_backstop();
        test_movie_round_trip();
        test_level_change();
        test_open_refusals();
        test_not_ours();
        test_other_combo();
        test_key();
        test_nothing_set();
        CHECK_EQ(load_mod(), 0);
    }

    free(g_mem);
    CHECK_DONE("load_test");
}
