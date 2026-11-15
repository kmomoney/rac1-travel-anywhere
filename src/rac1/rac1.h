/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Ratchet & Clank's structures as the mod reads them: offsets and values the same on both discs.
 * Each says where it was seen: rac1-decomp's include/structs/ (PAL) and the NTSC decomp's
 * include/rnc/ agree on every one, and the game code named beside it reads it so (DESIGN.md).
 */
#ifndef RAC1_TRAVEL_ANYWHERE_RAC1_H
#define RAC1_TRAVEL_ANYWHERE_RAC1_H

/* The hero (struct Hero, structs/hero.h; rnc/hero/hero.h). */
enum {
    TA_HERO_SWITCH_LOCK = 0x10AC,  /* u8 items[0].switch_lock: 2 a weapon change under way */
    TA_HERO_MOBY = 0x2080,         /* his moby; 0 until the level binds him (initialize_hero) */
    TA_HERO_STATE = 0x2084,        /* s32 state.current */
    TA_HERO_CONTROL_MODE = 0x208C, /* s32 state.control_mode */
    TA_HERO_VEHICLE = 0x20A4,      /* u8 vehicle_mode: 0 Ratchet, 1 Clank, 2 Giant Clank, 3 other */
    TA_HERO_FALL_FADE = 0x20B1,    /* u8 fall_fade_pending: off the world, the level reloads */
    TA_HERO_HOVERBOARD = 0x20B2,   /* u8 on_hoverboard: hero_set_state's hoverboard ride sets it */
    TA_HERO_HP = 0x22A8            /* s32 health.hp: 0 dead */
};

/* A moby (struct Moby). */
enum {
    TA_MOBY_STATE = 0x20, /* u8 */
    TA_MOBY_FLAGS = 0x34, /* u16 */
    /* Its flag bit 0: hidden and out of use. A ship with it is not boarded (the proximity check of
     * update_moby_proximity_interaction drops the hero); deactivate_recorded_moby sets it, as do
     * the ship's own sequences while they play. */
    TA_MOBY_HIDDEN = 0x01
};

/* The ship's record (level_render_state, rnc/rendering/level_render_state.h). */
enum {
    TA_SHIP_MOBY = 0x00, /* the parked ship; 0 in a level with none (load_level_gameplay) */
    /* The ship moby's state while its update plays Orxon's story scenes (scene 9's follow-ups,
     * update_moby_proximity_interaction), the take-off coming after them. */
    TA_SHIP_MOBY_SCENES = 0x2A
};

/* The menu system (struct MenuSystem, rnc/ui/menus/menu_system.h; the level's copy struct
 * GameState, rnc/game/overlay_game_state.h). */
enum {
    TA_MENU_STATE = 0x00,        /* s32: the page's state, or one of the steps below */
    TA_MENU_NEXT = 0x08,         /* the page asked for */
    TA_MENU_CLOSE = 0x0C,        /* s32 close_request: 1 back to play, 2 the ship's travel, ... */
    TA_MENU_TIMER = 0x14,        /* s32: a page switch's ticks */
    TA_MENU_PENDING_PAGE = 0xF0, /* the page the menu opens on again after its movie */
    TA_PAGE_STATE = 0x3C,        /* a page's (struct MenuPage) state */
    TA_MENU_SWITCHING = 1,       /* the step between two pages */
    TA_MENU_SHIP_ENTRY = 14,     /* the state the ship's take-off opens the menu in */
    TA_MENU_PLANETS = 15,        /* the planet select (planet_select_page) */
    TA_MENU_CONFIRM = 16,        /* a planet chosen (planet_confirm_page) */
    TA_MENU_CLOSING = 20,        /* closed, play coming back in two ticks */
    TA_CLOSE_TO_PLAY = 1,        /* any close but the next ones: the game plays on */
    TA_CLOSE_SHIP = 2            /* the planet select's: ShipTravelTo */
};

/* The debug menu's record (gDebugFreeCamLvl). */
enum {
    TA_DEBUG_FLAGS = 0x08, /* 0x10: single step */
    TA_DEBUG_STEPS = 0xEC, /* single steps left */
    TA_DEBUG_SINGLE_STEP = 0x10
};

#endif /* RAC1_TRAVEL_ANYWHERE_RAC1_H */
