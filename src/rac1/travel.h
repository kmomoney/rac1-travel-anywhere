/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The mod's decisions, kept apart from game memory: plain C that the tests call with hand-made
 * inputs (tests/travel_test.c) and game.cpp calls with what it read from the game.
 *
 * 1. The trigger: two buttons held and a third pressed (the `combo` setting). The mod takes it only
 *    in a level's play, and the game then never sees the third button.
 * 2. Whether the planet select may open now: the game's rules for its map, plus the mod's for the
 *    ship (DESIGN.md section 4). The first rule that refuses gives the reason.
 * 3. The menu once open: which press closed it. A back-out resumes play where Ratchet was instead
 *    of landing the ship, and choosing the planet he is on travels to it, which restarts the level
 *    (DESIGN.md section 2).
 */
#ifndef RAC1_TRAVEL_ANYWHERE_TRAVEL_H
#define RAC1_TRAVEL_ANYWHERE_TRAVEL_H

#include <stdint.h>

#include "rac1/rac1.h"

/* The pad's button bits, as the report lays them out (openrac_api.on_pad) and as the port's
 * OPENRAC_PRESS spells them. Here a set bit means held. */
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

/* Returns the buttons held in a report. Its first two bytes are a little-endian word in which a
 * clear bit means held. */
static inline uint32_t ta_report_held(const uint8_t* report) {
    return ~((uint32_t)report[0] | (uint32_t)report[1] << 8) & 0xFFFFu;
}

/* Marks `button` as released in a report, so the game reads it as not held. */
static inline void ta_report_release(uint8_t* report, uint32_t button) {
    report[0] |= (uint8_t)(button & 0xFFu);
    report[1] |= (uint8_t)(button >> 8);
}

/* ---- 1. The trigger ---- */

/* The combo setting's choices. */
enum {
    TA_COMBO_OFF = 0,
    TA_COMBO_L1_R1_SELECT = 1,
    TA_COMBO_L2_R2_SELECT = 2
};

/* A combo: every `hold` button held, then `press` pressed. With no buttons it is off. */
typedef struct ta_combo {
    uint32_t hold;
    uint32_t press;
} ta_combo;

/* Returns the combo for a TA_COMBO_* choice; an unknown choice is off. */
static inline ta_combo ta_combo_of(int choice) {
    ta_combo c = {0u, 0u};

    // L1 and R1 are the default. Ratchet still looks and crouches with them while they are held.
    if (choice == TA_COMBO_L1_R1_SELECT) {
        c.hold = TA_PAD_L1 | TA_PAD_R1;
        c.press = TA_PAD_SELECT;
    } else if (choice == TA_COMBO_L2_R2_SELECT) {
        c.hold = TA_PAD_L2 | TA_PAD_R2;
        c.press = TA_PAD_SELECT;
    }

    return c;
}

/* Returns whether the combo fires on a report: all its buttons are in `held` and its last one is
 * in `pressed` (held now but not in the report before). Pressing all three at once counts. */
static inline int ta_combo_fires(ta_combo c, uint32_t held, uint32_t pressed) {
    return c.press != 0u && (held & c.hold) == c.hold && (pressed & c.press) != 0u;
}

/* Returns whether the mod takes the combo: only in play (game mode 0) in a level that is not
 * ending. Elsewhere (a scene, a menu, the ship's flight, a load) the game keeps every button. */
static inline int ta_combo_is_ours(int level, int leaving, int mode) {
    return level >= 0 && !leaving && mode == 0;
}

/* ---- 2. When it may open ---- */

/* The game state the decision reads, taken once a tick (game.cpp's read_play). */
typedef struct ta_play {
    int level;        /* the level loaded, 0 to 18; -1 for none */
    int leaving;      /* its loop is ending: a travel or Quit Game on its way */
    int mode;         /* the level loop's game mode: 0 play */
    int mode_ticks;   /* ticks in that mode */
    int hero_bound;   /* the hero has his moby (the level is built) */
    int hp;           /* his health */
    int fall_fade;    /* he fell off the world and the level loop restarts it */
    int state;        /* his state */
    int control_mode; /* his state's control mode */
    int vehicle;      /* 0 Ratchet, 1 Clank, 2 Giant Clank, 3 another vehicle */
    int hoverboard;   /* on the hoverboard */
    int switch_lock;  /* his item slot's switch lock: 2 during a weapon change */
    int ship;         /* the level's ship can be boarded: it has a moby and is not hidden */
    int ship_scenes;  /* the ship moby is playing its story scenes */
    int ship_request; /* Triangle was pressed at the ship: its take-off is next */
    int freeze;       /* a dialog the level loop opens next */
    int menu_pending; /* the menu reopens by itself (after a movie) */
    int camera_block; /* the camera keeps the game from pausing */
    int card_busy;    /* the memory card is saving or loading */
    int stepping;     /* the debug menu steps the game a tick at a time */
} ta_play;

/* Why the planet select does not open, or TA_OPEN when it does. */
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
    TA_BUSY,        /* a moment the game's pause and map wait out */
    TA_NO_SHIP,     /* no ship to board: none in the level, or the story keeps it away */
    TA_SHIP_BUSY,   /* the ship's story scenes, or a take-off already asked for */
    TA_DIALOG,      /* a dialog or the game's menu about to open */
    TA_CARD,        /* the memory card saving or loading */
    TA_CAMERA,      /* the camera keeps the game from pausing */
    TA_STEPPING,    /* the debug menu's single step */
    TA_WHY_COUNT
};

/* The game's map (Select) and pause menu (Start) open only after 8 ticks of play. */
#define TA_PLAY_TICKS 8

/* Returns whether the hero is in a state the game's pause and map wait out, or in the ship. */
static inline int ta_state_busy(int state) {
    return state == 0x72    /* an item's cancellation: pause and map wait */
           || state == 0x1D /* the blend back to idle: pause and map wait */
           || state == 0x64 /* in the ship: its landing and take-off put him there */
        ;
}

/* Returns whether the hero's state is a death, a fatal fall or a respawn
 * (rnc/hero/hero_state.h). */
static inline int ta_state_down(int state) {
    // A state with no label of its own belongs with the one above it.
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

/* Returns whether the hero is riding: a vehicle or turret (0x32, which the map refuses) or the
 * hoverboard (0x3E, 0x6B, 0x6C). */
static inline int ta_state_riding(int state) {
    return state == 0x32 || state == 0x3E || state == 0x6B || state == 0x6C;
}

/* Returns TA_OPEN when the planet select may open now, else the first reason it may not (enum
 * ta_why). The rules run in the order of DESIGN.md section 4. */
static inline int ta_may_open(const ta_play* p) {
    // The level must be loaded and not ending, with the hero bound to his moby.
    if (p->level < 0 || p->leaving || !p->hero_bound) {
        return TA_NO_LEVEL;
    }

    // Game modes 1, 2 and 7 are a movie, a cutscene and a slideshow.
    if (p->mode == 1 || p->mode == 2 || p->mode == 7) {
        return TA_SCENE;
    }

    // Game mode 6 is the ship's landing, take-off or flight.
    if (p->mode == 6) {
        return TA_SHIP_FLIGHT;
    }

    // Modes 3, 4, 5 and -1 are a menu, a dialog, a vendor and the debug menu.
    if (p->mode != 0) {
        return TA_MENU;
    }

    // The game's pause and map also wait this long after any of those modes.
    if (p->mode_ticks < TA_PLAY_TICKS) {
        return TA_TOO_SOON;
    }

    // Dead, dying or off the world: the level respawns him next.
    if (p->hp <= 0 || p->fall_fade || ta_state_down(p->state)) {
        return TA_DOWN;
    }

    // The game never flies Clank alone; at the ship it plays a scene instead.
    if (p->vehicle == 1) {
        return TA_CLANK;
    }

    // As Giant Clank, Start opens his own screen rather than the pause menu.
    if (p->vehicle == 2) {
        return TA_GIANT_CLANK;
    }

    // Any other vehicle, a ride or turret, or the hoverboard.
    if (p->vehicle != 0 || p->hoverboard || ta_state_riding(p->state)) {
        return TA_RIDING;
    }

    // The game's pause and map wait these moments out too.
    if (ta_state_busy(p->state) || p->control_mode == 0x16 || p->switch_lock == 2) {
        return TA_BUSY;
    }

    // The menu flies the level's ship, so it must be there to board.
    if (!p->ship) {
        return TA_NO_SHIP;
    }

    // The ship's story scenes or a pending take-off. Its landing and take-off are mode 6, above.
    if (p->ship_scenes || p->ship_request) {
        return TA_SHIP_BUSY;
    }

    // A dialog or the game's menu opens next tick, and the pause menu would take its place.
    if (p->freeze || p->menu_pending) {
        return TA_DIALOG;
    }

    // The memory card is saving or loading.
    if (p->card_busy) {
        return TA_CARD;
    }

    // While the camera holds the pause off, PauseAllSounds only notes the pause and opens nothing.
    if (p->camera_block) {
        return TA_CAMERA;
    }

    // The debug menu is stepping the game one tick at a time.
    if (p->stepping) {
        return TA_STEPPING;
    }

    return TA_OPEN;
}

/* Returns a short name for a reason, for the trace. */
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

/* Returns the notice the player sees when the planet select does not open (shown by the port at
 * the top of the screen). */
static inline const char* ta_why_text(int why) {
    static const char* const texts[TA_WHY_COUNT] = {
        "",
        "Can't travel while a planet loads",
        "Can't travel during a cutscene",
        "Can't travel while the ship is flying",
        "Can't travel from a menu",
        "Can't travel yet, try again",
        "Can't travel until Ratchet respawns",
        "Can't travel as Clank",
        "Can't travel as Giant Clank",
        "Can't travel while riding",
        "Can't travel right now, try again",
        "Can't travel: no ship to board here",
        "Can't travel: the ship is about to take off",
        "Can't travel right now, try again",
        "Can't travel while the memory card is busy",
        "Can't travel right now, try again",
        "Can't travel while frame-stepping",
    };
    return why >= 0 && why < TA_WHY_COUNT ? texts[why] : "";
}

/* ---- 3. The menu once open ---- */

/* The planet select the mod opened, tracked from its opening to its close. */
typedef struct ta_session {
    int open;      /* the mod's planet select is up, or will reopen after a movie */
    int level;     /* the level it opened in */
    int confirmed; /* the last press that decides was Cross on the confirm page */
} ta_session;

/* Returns the state of the page that this tick's menu update hands the pad's presses to, or 0
 * when no page updates this tick. The pad is polled before the menu update, so the inputs are the
 * menu as the poll finds it. A switch asked for (`has_next`) starts this tick and updates no page.
 * A switch under way (state 1) ends this tick when its `timer` is 1 or less, and the new page
 * (`next_state`) then takes the presses. */
static inline int ta_menu_page(int state, int timer, int has_next, int next_state) {
    // During a switch, only its last tick hands the presses to a page.
    if (state == TA_MENU_SWITCHING) {
        return timer <= 1 && has_next ? next_state : 0;
    }

    return has_next ? 0 : state;
}

/* Records one pad poll while the mod's menu is open. `page` is the page the presses reach
 * (ta_menu_page) and `pressed` the newly pressed buttons. Keeps whether the last press that can
 * close the menu was Cross on the confirm page, weighing buttons as the game does. On the planet
 * list, Select, R3, Start and Triangle close it on the planet being played
 * (update_menu_entry_actions). On the confirm page, Triangle goes back to the list, Start closes it
 * on the planet being played, and Cross, checked after those two, travels to the planet chosen
 * (handle_menu_action_buttons). */
static inline void ta_session_press(ta_session* s, int page, uint32_t pressed) {
    // On the planet list, every close is a back-out.
    if (page == TA_MENU_PLANETS) {
        // The closing buttons clear a choice, and so does Cross, which only opens the confirm page.
        if (pressed & (TA_PAD_SELECT | TA_PAD_R3 | TA_PAD_START | TA_PAD_TRIANGLE | TA_PAD_CROSS)) {
            s->confirmed = 0;
        }

        return;
    }

    // No other page can decide the close.
    if (page != TA_MENU_CONFIRM) {
        return;
    }

    // Triangle (back to the list) and Start (close) win over Cross, as in the game.
    if (pressed & (TA_PAD_TRIANGLE | TA_PAD_START)) {
        s->confirmed = 0;
    } else if (pressed & TA_PAD_CROSS) {
        s->confirmed = 1;
    }
}

/* Returns whether the ship's list holds `level`. `list` is gGalaxyList's `count` entries, ended
 * early by a 0, so level 0 (Veldin's first visit) is never listed. */
static inline int ta_listed(const int32_t* list, int count, int level) {
    // The entries run up to `count` or the first 0.
    for (int i = 0; i < count && list[i] != 0; i++) {
        // The level is listed.
        if (list[i] == level) {
            return 1;
        }
    }

    return 0;
}

/* What the mod does with the planet select's close (ta_close). */
enum {
    TA_CLOSE_GAME = 0,   /* left to the game: a travel to another planet, or the ship's landing */
    TA_CLOSE_BACK = 1,   /* backed out: play goes on where Ratchet was, as after the map */
    TA_CLOSE_RESTART = 2 /* the current planet chosen: the game's travel to it restarts the level */
};

/* Returns what to do with the close of a menu (TA_CLOSE_*). `dest` is the planet it closed on,
 * `current` the planet being played and `listed` whether the ship's list holds it. A back-out
 * closes on the current planet with nothing chosen. Choosing another planet is the game's travel.
 * Choosing the current planet travels to it too, restarting the level, when the ship can fly
 * there; otherwise (Veldin, never listed) the game lands the ship again. */
static inline int ta_close(const ta_session* s, int dest, int current, int listed) {
    // The game keeps its own menu's close and any travel to another planet.
    if (!s->open || dest != current) {
        return TA_CLOSE_GAME;
    }

    // The current planet with nothing chosen is a back-out.
    if (!s->confirmed) {
        return TA_CLOSE_BACK;
    }

    return listed ? TA_CLOSE_RESTART : TA_CLOSE_GAME;
}

/* The menu target the mod sets for a restart: no planet, so ShipTravelTo takes its travel branch
 * and its save (memcard_Save with a level under 0) resumes on the level being played. -1 in the
 * load level also means the title (Quit Game), so the mod puts the planet back right away. */
#define TA_NO_PLANET (-1)

#endif /* RAC1_TRAVEL_ANYWHERE_TRAVEL_H */
