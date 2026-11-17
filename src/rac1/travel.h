/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The mod's decisions, apart from game memory: plain C the tests call with hand-made inputs
 * (tests/travel_test.c), and game.cpp with what it read from the game.
 *
 * 1. The trigger: two buttons held and a third pressed (the `combo` setting), the mod's only in a
 *    level's play; the third press is then the mod's alone.
 * 2. Whether the planet select may open now: the game's own rules for its map, and the mod's for
 *    the ship (DESIGN.md, section 4). The first rule that refuses names the reason.
 * 3. The menu once open: which press closed it, so a close that backs out plays on where Ratchet
 *    was instead of landing the ship, and one that chooses the planet he is on travels to it,
 *    starting the level anew (DESIGN.md, section 2).
 */
#ifndef RAC1_TRAVEL_ANYWHERE_TRAVEL_H
#define RAC1_TRAVEL_ANYWHERE_TRAVEL_H

#include <stdint.h>

#include "rac1/rac1.h"

/* The pad's buttons as its report has them (openrac_api.on_pad; here a set bit is held) and as the
 * port's OPENRAC_PRESS spells them. */
#define TA_PAD_SELECT 0x0001u
#define TA_PAD_L3 0x0002u
#define TA_PAD_R3 0x0004u
#define TA_PAD_START 0x0008u
#define TA_PAD_L2 0x0100u
#define TA_PAD_R2 0x0200u
#define TA_PAD_L1 0x0400u
#define TA_PAD_R1 0x0800u
#define TA_PAD_TRIANGLE 0x1000u
#define TA_PAD_CIRCLE 0x2000u
#define TA_PAD_CROSS 0x4000u
#define TA_PAD_SQUARE 0x8000u

/* The buttons a report holds: its first two bytes are a little-endian word, a clear bit held. */
static inline uint32_t ta_report_held(const uint8_t* report) {
    return ~((uint32_t)report[0] | (uint32_t)report[1] << 8) & 0xFFFFu;
}

/* A button let go in a report, as the game will read it. */
static inline void ta_report_release(uint8_t* report, uint32_t button) {
    report[0] |= (uint8_t)(button & 0xFFu);
    report[1] |= (uint8_t)(button >> 8);
}

/* ---- 1. The trigger --------------------------------------------------------------------------- */

/* The combo setting's choices. */
enum {
    TA_COMBO_OFF = 0,
    TA_COMBO_L1_R1_SELECT = 1,
    TA_COMBO_L2_R2_SELECT = 2
};

/* A combo: every `hold` button held, then `press` pressed. No buttons: off. */
typedef struct ta_combo {
    uint32_t hold;
    uint32_t press;
} ta_combo;

static inline ta_combo ta_combo_of(int choice) {
    ta_combo c = {0u, 0u};

    // L1 and R1, the default: Ratchet's look and crouch, which go on as ever while they are held.
    if (choice == TA_COMBO_L1_R1_SELECT) {
        c.hold = TA_PAD_L1 | TA_PAD_R1;
        c.press = TA_PAD_SELECT;
    } else if (choice == TA_COMBO_L2_R2_SELECT) {
        c.hold = TA_PAD_L2 | TA_PAD_R2;
        c.press = TA_PAD_SELECT;
    }

    return c;
}

/* Whether the combo fires on a report: its buttons held (`held`), the last one pressed in it
 * (`pressed`: held now and not in the report before). Pressing the three at once counts. */
static inline int ta_combo_fires(ta_combo c, uint32_t held, uint32_t pressed) {
    return c.press != 0u && (held & c.hold) == c.hold && (pressed & c.press) != 0u;
}

/* Whether a press of the combo is the mod's: in a level whose loop runs, in play (game mode 0).
 * Anywhere else (a scene, a menu, the ship's flight, a load) the game keeps every button. */
static inline int ta_combo_is_ours(int level, int leaving, int mode) {
    return level >= 0 && !leaving && mode == 0;
}

/* ---- 2. May the planet select open now? ------------------------------------------------------ */

/* The game as the decision reads it, once a tick (game.cpp's read_play). */
typedef struct ta_play {
    int level;        /* the level loaded, 0 to 18; -1 none */
    int leaving;      /* its loop ends: a travel or Quit Game on its way */
    int mode;         /* the level loop's game mode: 0 play */
    int mode_ticks;   /* ticks in that mode */
    int hero_bound;   /* the hero has his moby (the level is built) */
    int hp;           /* his health */
    int fall_fade;    /* he fell off the world: the level loop restarts it */
    int state;        /* his state */
    int control_mode; /* his state's control mode */
    int vehicle;      /* 0 Ratchet, 1 Clank, 2 Giant Clank, 3 another vehicle */
    int hoverboard;   /* on the hoverboard */
    int switch_lock;  /* his item slot's switch lock: 2 a change under way */
    int ship;         /* the level's ship is there to board: its moby, not hidden */
    int ship_scenes;  /* the ship moby plays its story scenes */
    int ship_request; /* Triangle at the ship: its take-off next */
    int freeze;       /* a dialog the level loop opens next */
    int menu_pending; /* the menu opens again by itself (after its movie) */
    int camera_block; /* the camera keeps the game from pausing */
    int card_busy;    /* the memory card is at work */
    int stepping;     /* the debug menu steps the game a tick at a time */
} ta_play;

/* Why the planet select does not open, or that it does. */
enum ta_why {
    TA_OPEN = 0,
    TA_NO_LEVEL,    /* no level, one loading, or one ending */
    TA_SCENE,       /* a movie, a cutscene or a slideshow */
    TA_SHIP_FLIGHT, /* the ship's own landing, take-off or flight */
    TA_MENU,        /* a menu, a dialog, a vendor or the debug menu */
    TA_TOO_SOON,    /* the first ticks of play after any of those */
    TA_DOWN,        /* dead, dying, off the world or respawning */
    TA_CLANK,       /* Clank on his own */
    TA_GIANT_CLANK, /* Giant Clank */
    TA_RIDING,      /* a vehicle, a turret, the hoverboard */
    TA_BUSY,        /* a moment the game's own pause or map waits out */
    TA_NO_SHIP,     /* no ship to board: none in the level, or the story keeps it away */
    TA_SHIP_BUSY,   /* the ship's own story scenes, or its take-off asked for */
    TA_DIALOG,      /* a dialog, or the game's own menu, about to open */
    TA_CARD,        /* the memory card at work */
    TA_CAMERA,      /* the camera keeps the game from pausing */
    TA_STEPPING,    /* the debug menu's single step */
    TA_WHY_COUNT
};

/* The game's own map (Select) and pause (Start) open only after 8 ticks of play. */
#define TA_PLAY_TICKS 8

/* The hero's states the game shows no menu in (the map's own refusals) and the ship's own. */
static inline int ta_state_busy(int state) {
    return state == 0x72    /* an item's cancellation: pause and map wait */
           || state == 0x1D /* the blend back to idle: pause and map wait */
           || state == 0x64 /* in the ship: its landing and take-off put him there */
        ;
}

/* The hero's states of death, dying, falling and respawning (rnc/hero/hero_state.h). */
static inline int ta_state_down(int state) {
    // Each such state, by its id.
    switch (state) {
        case 0x3D: /* dying */
        case 0x47: /* Clank dying */
        case 0x57: /* a vehicle's dying */
        case 0x62: /* Giant Clank dying */
        case 0x68: /* waiting to respawn */
        case 0x69: /* the jump out of that wait */
        case 0x6A: /* drowning */
        case 0x6D: /* the hoverboard's respawns */
        case 0x6E:
        case 0x6F:
        case 0x77: /* a fatal fall */
        case 0x7B: /* waiting to respawn */
        case 0x7C: /* the death descents */
        case 0x7D:
        case 0x7F: /* sinking */
        case 0x80: /* dying */
        case 0x82:
            return 1;
        default:
            return 0;
    }
}

/* The hero's states on a ride: the vehicles' and turrets' (0x32, which the map refuses) and the
 * hoverboard's. */
static inline int ta_state_riding(int state) {
    return state == 0x32 || state == 0x3E || state == 0x6B || state == 0x6C;
}

static inline int ta_may_open(const ta_play* p) {
    // A level built, its loop running, the hero in it.
    if (p->level < 0 || p->leaving || !p->hero_bound) {
        return TA_NO_LEVEL;
    }

    // The game mode: play only. 1 a movie, 2 a cutscene, 7 a slideshow; 6 the ship's own.
    if (p->mode == 1 || p->mode == 2 || p->mode == 7) {
        return TA_SCENE;
    }

    // The ship's landing, take-off or flight.
    if (p->mode == 6) {
        return TA_SHIP_FLIGHT;
    }

    // 3 a menu, 4 a dialog, 5 a vendor, -1 the debug menu.
    if (p->mode != 0) {
        return TA_MENU;
    }

    // The game's own wait after any of them.
    if (p->mode_ticks < TA_PLAY_TICKS) {
        return TA_TOO_SOON;
    }

    // Death and its kin: the level's respawn comes next.
    if (p->hp <= 0 || p->fall_fade || ta_state_down(p->state)) {
        return TA_DOWN;
    }

    // Clank alone: the game itself plays a scene when he reaches the ship.
    if (p->vehicle == 1) {
        return TA_CLANK;
    }

    // Giant Clank: Start opens his own screen.
    if (p->vehicle == 2) {
        return TA_GIANT_CLANK;
    }

    // Any other vehicle the hero became, a ride, the hoverboard.
    if (p->vehicle != 0 || p->hoverboard || ta_state_riding(p->state)) {
        return TA_RIDING;
    }

    // The moments the game's own pause and map wait out.
    if (ta_state_busy(p->state) || p->control_mode == 0x16 || p->switch_lock == 2) {
        return TA_BUSY;
    }

    // The ship the menu flies.
    if (!p->ship) {
        return TA_NO_SHIP;
    }

    // Its story scenes, its take-off asked for. (Its own sequences play in game mode 6, above.)
    if (p->ship_scenes || p->ship_request) {
        return TA_SHIP_BUSY;
    }

    // A dialog or the game's menu opening next tick.
    if (p->freeze || p->menu_pending) {
        return TA_DIALOG;
    }

    // A save or a load under way.
    if (p->card_busy) {
        return TA_CARD;
    }

    // PauseAllSounds would only note a pause for later.
    if (p->camera_block) {
        return TA_CAMERA;
    }

    // The debug menu's single step.
    if (p->stepping) {
        return TA_STEPPING;
    }

    return TA_OPEN;
}

/* A word for the trace. */
static inline const char* ta_why_name(int why) {
    static const char* const names[TA_WHY_COUNT] = {
        "open",
        "no-level",
        "scene",
        "ship-flight",
        "menu",
        "too-soon",
        "down",
        "clank",
        "giant-clank",
        "riding",
        "busy",
        "no-ship",
        "ship-busy",
        "dialog",
        "card",
        "camera",
        "stepping",
    };
    return why >= 0 && why < TA_WHY_COUNT ? names[why] : "?";
}

/* The line the player sees when it does not open (the port's notice, at the top of the picture). */
static inline const char* ta_why_text(int why) {
    static const char* const texts[TA_WHY_COUNT] = {
        "",
        "Ship travel: not while a planet loads",
        "Ship travel: not during a cutscene",
        "Ship travel: not while the ship flies",
        "Ship travel: not in a menu",
        "Ship travel: not yet",
        "Ship travel: not while Ratchet is down",
        "Ship travel: Clank cannot fly the ship",
        "Ship travel: not as Giant Clank",
        "Ship travel: not while riding",
        "Ship travel: not now",
        "Ship travel: the ship is not here",
        "Ship travel: the ship is busy",
        "Ship travel: not now",
        "Ship travel: not while the memory card is busy",
        "Ship travel: not now",
        "Ship travel: not while the debug menu steps",
    };
    return why >= 0 && why < TA_WHY_COUNT ? texts[why] : "";
}

/* ---- 3. The menu once open ------------------------------------------------------------------ */

/* What the mod keeps of the planet select it opened, from the open to its close. */
typedef struct ta_session {
    int open;      /* the mod's planet select is up, or comes back after its movie */
    int level;     /* the level it opened in */
    int confirmed; /* the last press that decides: Cross on a chosen planet */
} ta_session;

/* The page a poll's presses reach, from the menu as the poll finds it (before the tick's menu
 * update): the page's state, or 0 when the update leaves the pages alone this tick. A page switch
 * asked for (`next`) starts this tick and updates no page; one under way (state 1) ends this tick
 * when its `timer` is at 1 or under, and its page then takes the presses. */
static inline int ta_menu_page(int state, int timer, int has_next, int next_state) {
    // A switch under way.
    if (state == TA_MENU_SWITCHING) {
        return timer <= 1 && has_next ? next_state : 0;
    }

    return has_next ? 0 : state;
}

/* A poll while the mod's menu is open: `page` the page its presses reach (ta_menu_page),
 * `pressed` the buttons newly pressed. Keeps whether the last press that can close the menu is
 * Cross on the confirm page, as the game weighs them: on the planet list Select, R3, Start and
 * Triangle close it with the planet the ship is on (update_menu_entry_actions); on the confirm
 * page Triangle goes back to the list, Start closes it with that planet, and Cross only after
 * those two travels to the planet chosen (handle_menu_action_buttons). */
static inline void ta_session_press(ta_session* s, int page, uint32_t pressed) {
    // The planet list: any close is a way out.
    if (page == TA_MENU_PLANETS) {
        // Its closing buttons, and Cross, which only opens the confirm page.
        if (pressed & (TA_PAD_SELECT | TA_PAD_R3 | TA_PAD_START | TA_PAD_TRIANGLE | TA_PAD_CROSS)) {
            s->confirmed = 0;
        }

        return;
    }

    // Any other page but the confirm page: nothing of the mod's.
    if (page != TA_MENU_CONFIRM) {
        return;
    }

    // Back to the list, or out.
    if (pressed & (TA_PAD_TRIANGLE | TA_PAD_START)) {
        s->confirmed = 0;
    } else if (pressed & TA_PAD_CROSS) {
        s->confirmed = 1;
    }
}

/* Whether the ship's list holds a planet: `list` is gGalaxyList's `count` entries, a 0 ending them.
 * Level 0 (Veldin's first visit) is never in it: 0 is the end. */
static inline int ta_listed(const int32_t* list, int count, int level) {
    // Each entry up to the end.
    for (int i = 0; i < count && list[i] != 0; i++) {
        // This one.
        if (list[i] == level) {
            return 1;
        }
    }

    return 0;
}

/* What the planet select's close does. */
enum {
    TA_CLOSE_GAME = 0,   /* the game's own: the travel to another planet, or the ship's landing */
    TA_CLOSE_BACK = 1,   /* backed out: play on where Ratchet was, as the map's close does */
    TA_CLOSE_RESTART = 2 /* the planet he is on, chosen: the game's travel to it, the level anew */
};

/* The close of the mod's menu: `dest` the planet it closed with, `current` the one the hero is on,
 * `listed` whether the ship's list holds it. Backing out leaves the ship's planet selected. A planet
 * chosen is the game's travel; the one he is on too (the level loads anew, as on arrival) when the
 * ship could fly to it, else (Veldin, never listed) the game's own landing again. */
static inline int ta_close(const ta_session* s, int dest, int current, int listed) {
    // Not the mod's menu, or another planet: the game's.
    if (!s->open || dest != current) {
        return TA_CLOSE_GAME;
    }

    // The planet he is on.
    if (!s->confirmed) {
        return TA_CLOSE_BACK;
    }

    return listed ? TA_CLOSE_RESTART : TA_CLOSE_GAME;
}

/* The restore's target while a restart's travel branch runs: no planet, so ShipTravelTo takes its
 * travel branch, and its save (memcard_Save with a level under 0) resumes on the level played. Also
 * the load level's value for the title (Quit Game), so the mod sets the planet back at once. */
#define TA_NO_PLANET (-1)

#endif /* RAC1_TRAVEL_ANYWHERE_TRAVEL_H */
