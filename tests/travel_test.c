/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Tests src/rac1/travel.h's decisions with hand-made inputs: the pad report's buttons, the combo,
 * when the mod takes the combo, every refusal of ta_may_open in its order, the page a poll's
 * presses reach, the presses the session records, and what the menu's close does.
 *
 * Each expected value follows the game's rule as DESIGN.md describes it (sections 1, 2 and 4).
 */
#include <stdint.h>
#include <string.h>

#include "check.h"
#include "rac1/travel.h"

/* Fills a report with these buttons held (a clear bit means held) and the sticks centred. */
static void report_of(uint8_t r[18], uint32_t held) {
    memset(r, 0xFF, 2);
    memset(r + 2, 0x80, 4);
    memset(r + 6, 0, 12);
    r[0] = (uint8_t)~(held & 0xFFu);
    r[1] = (uint8_t)~(held >> 8);
}

static void test_report(void) {
    uint8_t r[18];

    // None held, then L1, R1 and Select.
    report_of(r, 0);
    CHECK_EQ(ta_report_held(r), 0);
    report_of(r, TA_PAD_L1 | TA_PAD_R1 | TA_PAD_SELECT);
    CHECK_EQ(ta_report_held(r), TA_PAD_L1 | TA_PAD_R1 | TA_PAD_SELECT);

    // The byte layout: Select is bit 0 of byte 0, L1 bit 2 of byte 1 (scripts.md).
    CHECK_EQ(r[0], 0xFE);
    CHECK_EQ(r[1], 0xF3);

    // Releasing Select leaves the shoulders held and the sticks untouched.
    ta_report_release(r, TA_PAD_SELECT);
    CHECK_EQ(ta_report_held(r), TA_PAD_L1 | TA_PAD_R1);
    CHECK_EQ(r[2], 0x80);

    // Releasing a button that is not held changes nothing.
    ta_report_release(r, TA_PAD_SELECT);
    CHECK_EQ(ta_report_held(r), TA_PAD_L1 | TA_PAD_R1);
}

static void test_combo(void) {
    const ta_combo l1r1 = ta_combo_of(TA_COMBO_L1_R1_SELECT);
    const ta_combo l2r2 = ta_combo_of(TA_COMBO_L2_R2_SELECT);
    const ta_combo off = ta_combo_of(TA_COMBO_OFF);
    const uint32_t shoulders = TA_PAD_L1 | TA_PAD_R1;

    CHECK_EQ(l1r1.hold, TA_PAD_L1 | TA_PAD_R1);
    CHECK_EQ(l1r1.press, TA_PAD_SELECT);
    CHECK_EQ(l2r2.hold, TA_PAD_L2 | TA_PAD_R2);
    CHECK_EQ(off.press, 0);

    // L1 and R1 held, Select pressed: it fires.
    CHECK_EQ(ta_combo_fires(l1r1, shoulders | TA_PAD_SELECT, TA_PAD_SELECT), 1);

    // All three pressed in the same report.
    CHECK_EQ(ta_combo_fires(l1r1, shoulders | TA_PAD_SELECT, shoulders | TA_PAD_SELECT), 1);

    // Other buttons held too (running, jumping) still count as the combo.
    CHECK_EQ(ta_combo_fires(l1r1, shoulders | TA_PAD_SELECT | TA_PAD_CROSS, TA_PAD_SELECT), 1);

    // Select alone is the game's map.
    CHECK_EQ(ta_combo_fires(l1r1, TA_PAD_SELECT, TA_PAD_SELECT), 0);

    // One shoulder only.
    CHECK_EQ(ta_combo_fires(l1r1, TA_PAD_L1 | TA_PAD_SELECT, TA_PAD_SELECT), 0);
    CHECK_EQ(ta_combo_fires(l1r1, TA_PAD_R1 | TA_PAD_SELECT, TA_PAD_SELECT), 0);

    // Select held from before and a shoulder pressed last is not the combo's order.
    CHECK_EQ(ta_combo_fires(l1r1, shoulders | TA_PAD_SELECT, TA_PAD_R1), 0);

    // The shoulders held, no Select.
    CHECK_EQ(ta_combo_fires(l1r1, shoulders, TA_PAD_L1), 0);

    // The other combo's shoulders.
    CHECK_EQ(ta_combo_fires(l2r2, shoulders | TA_PAD_SELECT, TA_PAD_SELECT), 0);
    CHECK_EQ(ta_combo_fires(l2r2, TA_PAD_L2 | TA_PAD_R2 | TA_PAD_SELECT, TA_PAD_SELECT), 1);

    // Off never fires, even with every button.
    CHECK_EQ(ta_combo_fires(off, 0xFFFFu, 0xFFFFu), 0);

    // An unknown choice is off too.
    CHECK_EQ(ta_combo_of(7).press, 0);
}

static void test_combo_is_ours(void) {
    // In a level's play.
    CHECK_EQ(ta_combo_is_ours(3, 0, 0), 1);

    // Not with no level, a level ending, a cutscene, a menu, the ship's flight or the debug menu.
    CHECK_EQ(ta_combo_is_ours(-1, 0, 0), 0);
    CHECK_EQ(ta_combo_is_ours(3, 1, 0), 0);
    CHECK_EQ(ta_combo_is_ours(3, 0, 2), 0);
    CHECK_EQ(ta_combo_is_ours(3, 0, 3), 0);
    CHECK_EQ(ta_combo_is_ours(3, 0, 6), 0);
    CHECK_EQ(ta_combo_is_ours(3, 0, -1), 0);
}

/* Ratchet standing in play on Kerwan, his ship parked, nothing else going on. */
static ta_play playing(void) {
    ta_play p;
    memset(&p, 0, sizeof p);
    p.level = 3;
    p.mode_ticks = 100;
    p.hero_bound = 1;
    p.hp = 4;
    p.ship = 1;
    return p;
}

static void test_may_open(void) {
    ta_play p = playing();
    CHECK_EQ(ta_may_open(&p), TA_OPEN);

    // Mid-air, swimming, grinding, hanging, looking, crouching, hurt: the game's pause and map open
    // in all of them, and so does the planet select.
    {
        const int states[] = {0x01, 0x04, 0x07, 0x0E, 0x16, 0x19, 0x28, 0x33, 0x74};

        // Each state.
        for (unsigned i = 0; i < sizeof states / sizeof states[0]; i++) {
            p = playing();
            p.state = states[i];
            CHECK_EQ(ta_may_open(&p), TA_OPEN);
        }
    }

    // No level, the level ending, the hero not yet bound.
    p = playing();
    p.level = -1;
    CHECK_EQ(ta_may_open(&p), TA_NO_LEVEL);
    p = playing();
    p.leaving = 1;
    CHECK_EQ(ta_may_open(&p), TA_NO_LEVEL);
    p = playing();
    p.hero_bound = 0;
    CHECK_EQ(ta_may_open(&p), TA_NO_LEVEL);

    // Each game mode but play.
    {
        const struct {
            int mode, why;
        } modes[] = {
            {1, TA_SCENE},
            {2, TA_SCENE},
            {7, TA_SCENE},
            {6, TA_SHIP_FLIGHT},
            {3, TA_MENU},
            {4, TA_MENU},
            {5, TA_MENU},
            {-1, TA_MENU},
        };

        // Each mode.
        for (unsigned i = 0; i < sizeof modes / sizeof modes[0]; i++) {
            p = playing();
            p.mode = modes[i].mode;
            CHECK_EQ(ta_may_open(&p), modes[i].why);
        }
    }

    // The game waits 8 ticks of play first.
    p = playing();
    p.mode_ticks = 7;
    CHECK_EQ(ta_may_open(&p), TA_TOO_SOON);
    p.mode_ticks = 8;
    CHECK_EQ(ta_may_open(&p), TA_OPEN);

    // Dead, off the world, and each dying or respawning state.
    p = playing();
    p.hp = 0;
    CHECK_EQ(ta_may_open(&p), TA_DOWN);
    p = playing();
    p.fall_fade = 1;
    CHECK_EQ(ta_may_open(&p), TA_DOWN);
    {
        const int states[] =
            {0x3D,
             0x47,
             0x57,
             0x62,
             0x68,
             0x69,
             0x6A,
             0x6D,
             0x6E,
             0x6F,
             0x77,
             0x7B,
             0x7C,
             0x7D,
             0x7F,
             0x80,
             0x82};

        // Each state.
        for (unsigned i = 0; i < sizeof states / sizeof states[0]; i++) {
            p = playing();
            p.state = states[i];
            CHECK_EQ(ta_may_open(&p), TA_DOWN);
        }
    }

    // Clank, Giant Clank, another vehicle.
    p = playing();
    p.vehicle = 1;
    CHECK_EQ(ta_may_open(&p), TA_CLANK);
    p.vehicle = 2;
    CHECK_EQ(ta_may_open(&p), TA_GIANT_CLANK);
    p.vehicle = 3;
    CHECK_EQ(ta_may_open(&p), TA_RIDING);

    // A ride, a turret, the hoverboard (its flag, its states).
    p = playing();
    p.hoverboard = 1;
    CHECK_EQ(ta_may_open(&p), TA_RIDING);
    {
        const int states[] = {0x32, 0x3E, 0x6B, 0x6C};

        // Each state.
        for (unsigned i = 0; i < sizeof states / sizeof states[0]; i++) {
            p = playing();
            p.state = states[i];
            CHECK_EQ(ta_may_open(&p), TA_RIDING);
        }
    }

    // The game's pause and map wait out these states, control mode 0x16 and a weapon change.
    {
        const int states[] = {0x72, 0x1D, 0x64};

        // Each state.
        for (unsigned i = 0; i < sizeof states / sizeof states[0]; i++) {
            p = playing();
            p.state = states[i];
            CHECK_EQ(ta_may_open(&p), TA_BUSY);
        }
    }

    p = playing();
    p.control_mode = 0x16;
    CHECK_EQ(ta_may_open(&p), TA_BUSY);
    p = playing();
    p.switch_lock = 2;
    CHECK_EQ(ta_may_open(&p), TA_BUSY);
    p.switch_lock = 1;
    CHECK_EQ(ta_may_open(&p), TA_OPEN);

    // No ship, its story scenes, a take-off already asked for.
    p = playing();
    p.ship = 0;
    CHECK_EQ(ta_may_open(&p), TA_NO_SHIP);
    p = playing();
    p.ship_scenes = 1;
    CHECK_EQ(ta_may_open(&p), TA_SHIP_BUSY);
    p = playing();
    p.ship_request = 1;
    CHECK_EQ(ta_may_open(&p), TA_SHIP_BUSY);

    // A dialog next, the menu reopening, the card, the camera, the debug menu stepping.
    p = playing();
    p.freeze = 1;
    CHECK_EQ(ta_may_open(&p), TA_DIALOG);
    p = playing();
    p.menu_pending = 1;
    CHECK_EQ(ta_may_open(&p), TA_DIALOG);
    p = playing();
    p.card_busy = 1;
    CHECK_EQ(ta_may_open(&p), TA_CARD);
    p = playing();
    p.camera_block = 1;
    CHECK_EQ(ta_may_open(&p), TA_CAMERA);
    p = playing();
    p.stepping = 1;
    CHECK_EQ(ta_may_open(&p), TA_STEPPING);

    // The order: a cutscene beats a dead hero, a dying Clank is down, no ship beats the card.
    p = playing();
    p.mode = 2;
    p.hp = 0;
    CHECK_EQ(ta_may_open(&p), TA_SCENE);
    p = playing();
    p.vehicle = 1;
    p.state = 0x47;
    CHECK_EQ(ta_may_open(&p), TA_DOWN);
    p = playing();
    p.ship = 0;
    p.card_busy = 1;
    CHECK_EQ(ta_may_open(&p), TA_NO_SHIP);

    // Every reason has a trace name, and every one but TA_OPEN a notice.
    for (int why = 0; why < TA_WHY_COUNT; why++) {
        CHECK_EQ(strlen(ta_why_name(why)) > 0, 1);
        CHECK_EQ(strlen(ta_why_text(why)) > 0, why != TA_OPEN);
    }

    CHECK_EQ(strcmp(ta_why_name(TA_WHY_COUNT), "?"), 0);
    CHECK_EQ(strlen(ta_why_text(-1)), 0);
}

static void test_menu_page(void) {
    // A page with no switch asked for gets the presses itself.
    CHECK_EQ(ta_menu_page(TA_MENU_PLANETS, 0, 0, 0), TA_MENU_PLANETS);
    CHECK_EQ(ta_menu_page(TA_MENU_CONFIRM, 0, 0, 0), TA_MENU_CONFIRM);

    // A switch asked for starts this tick, and no page updates.
    CHECK_EQ(ta_menu_page(TA_MENU_PLANETS, 0, 1, TA_MENU_CONFIRM), 0);

    // During a switch no page updates until its last tick, when the new page takes the presses.
    CHECK_EQ(ta_menu_page(TA_MENU_SWITCHING, 12, 1, TA_MENU_CONFIRM), 0);
    CHECK_EQ(ta_menu_page(TA_MENU_SWITCHING, 2, 1, TA_MENU_CONFIRM), 0);
    CHECK_EQ(ta_menu_page(TA_MENU_SWITCHING, 1, 1, TA_MENU_CONFIRM), TA_MENU_CONFIRM);
    CHECK_EQ(ta_menu_page(TA_MENU_SWITCHING, 0, 1, TA_MENU_PLANETS), TA_MENU_PLANETS);

    // The closing menu has a state of its own.
    CHECK_EQ(ta_menu_page(TA_MENU_CLOSING, 2, 0, 0), TA_MENU_CLOSING);
}

static void test_session(void) {
    ta_session s = {1, 3, 0};

    // On the list: Cross only opens the confirm page.
    ta_session_press(&s, TA_MENU_PLANETS, TA_PAD_CROSS);
    CHECK_EQ(s.confirmed, 0);

    // On the confirm page: Cross travels.
    ta_session_press(&s, TA_MENU_CONFIRM, TA_PAD_CROSS);
    CHECK_EQ(s.confirmed, 1);

    // Up, Down, Circle (the planet's infobot) and presses no page reads leave the choice alone.
    ta_session_press(&s, TA_MENU_CONFIRM, TA_PAD_CIRCLE);
    ta_session_press(&s, 0, TA_PAD_TRIANGLE);
    ta_session_press(&s, TA_MENU_CLOSING, TA_PAD_START);
    CHECK_EQ(s.confirmed, 1);

    // Triangle goes back to the list.
    ta_session_press(&s, TA_MENU_CONFIRM, TA_PAD_TRIANGLE);
    CHECK_EQ(s.confirmed, 0);

    // Start on the confirm page closes on the planet being played: a back-out.
    s.confirmed = 1;
    ta_session_press(&s, TA_MENU_CONFIRM, TA_PAD_START);
    CHECK_EQ(s.confirmed, 0);

    // Cross with Start or Triangle: those win, as handle_menu_action_buttons weighs them.
    ta_session_press(&s, TA_MENU_CONFIRM, TA_PAD_CROSS | TA_PAD_START);
    CHECK_EQ(s.confirmed, 0);
    ta_session_press(&s, TA_MENU_CONFIRM, TA_PAD_CROSS | TA_PAD_TRIANGLE);
    CHECK_EQ(s.confirmed, 0);

    // Each way out of the list.
    {
        const uint32_t outs[] = {TA_PAD_SELECT, TA_PAD_R3, TA_PAD_START, TA_PAD_TRIANGLE};

        // Each button.
        for (unsigned i = 0; i < sizeof outs / sizeof outs[0]; i++) {
            s.confirmed = 1;
            ta_session_press(&s, TA_MENU_PLANETS, outs[i]);
            CHECK_EQ(s.confirmed, 0);
        }
    }
}

static void test_listed(void) {
    // A new game on Kerwan lists Kerwan alone; later Novalis and Aridia are added.
    const int32_t kerwan[TA_GALAXY_LIST_LEN] = {3};
    const int32_t three[TA_GALAXY_LIST_LEN] = {3, 1, 2};
    const int32_t full[4] = {1, 2, 3, 4};
    const int32_t none[TA_GALAXY_LIST_LEN] = {0};

    CHECK_EQ(ta_listed(kerwan, TA_GALAXY_LIST_LEN, 3), 1);
    CHECK_EQ(ta_listed(kerwan, TA_GALAXY_LIST_LEN, 1), 0);
    CHECK_EQ(ta_listed(three, TA_GALAXY_LIST_LEN, 2), 1);
    CHECK_EQ(ta_listed(three, TA_GALAXY_LIST_LEN, 4), 0);

    // The list ends at a 0 or at its length.
    CHECK_EQ(ta_listed(full, 4, 4), 1);
    CHECK_EQ(ta_listed(full, 3, 4), 0);

    // Veldin's first visit (level 0) is never listed: 0 ends the list.
    CHECK_EQ(ta_listed(none, TA_GALAXY_LIST_LEN, 0), 0);
    CHECK_EQ(ta_listed(three, TA_GALAXY_LIST_LEN, 0), 0);
}

static void test_close(void) {
    ta_session s = {1, 3, 0};

    // A back-out: the planet being played with nothing chosen, listed or not.
    CHECK_EQ(ta_close(&s, 3, 3, 1), TA_CLOSE_BACK);
    CHECK_EQ(ta_close(&s, 3, 3, 0), TA_CLOSE_BACK);

    // The planet being played, chosen: the game's travel to it restarts the level.
    s.confirmed = 1;
    CHECK_EQ(ta_close(&s, 3, 3, 1), TA_CLOSE_RESTART);

    // A planet the ship cannot fly to (Veldin's first visit) gets the game's landing.
    CHECK_EQ(ta_close(&s, 0, 0, 0), TA_CLOSE_GAME);

    // Another planet is the game's travel, chosen or not: the game's selection decides.
    CHECK_EQ(ta_close(&s, 1, 3, 1), TA_CLOSE_GAME);
    s.confirmed = 0;
    CHECK_EQ(ta_close(&s, 1, 3, 1), TA_CLOSE_GAME);

    // A menu the mod did not open is always the game's.
    s.open = 0;
    CHECK_EQ(ta_close(&s, 3, 3, 1), TA_CLOSE_GAME);
    s.confirmed = 1;
    CHECK_EQ(ta_close(&s, 3, 3, 1), TA_CLOSE_GAME);

    // TA_NO_PLANET is never a real planet.
    CHECK_EQ(TA_NO_PLANET < 0, 1);
}

int main(void) {
    test_report();
    test_combo();
    test_combo_is_ours();
    test_may_open();
    test_menu_page();
    test_session();
    test_listed();
    test_close();
    CHECK_DONE("travel_test");
}
