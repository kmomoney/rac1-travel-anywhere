// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ratchet & Clank's part of the mod (DESIGN.md). It opens the ship's planet select from anywhere
// in play, resumes play where Ratchet was when the player backs out, and turns choosing the planet
// he is on into an ordinary trip to it, which restarts the level. The decisions are in
// src/rac1/travel.h, with no game memory; this file reads the game for them and applies what they
// return. Addresses are in discs.h, offsets in rac1.h.
//
// Where each part runs:
//   pad report  on_pad: in play, the combo (or the key) asks for the menu, and the combo's last
//               button is hidden from the game until released. While the mod's menu is up, it
//               records the presses that can close it and steers the close (below). After a
//               restart's travel branch, it sets the load level.
//   tick end    on_frame: opens the menu the way the ship's take-off does (PauseAllSounds with the
//               menu in state 14), or shows a notice saying why not. Steers the close: a back-out
//               becomes the map's close, which resumes play in place, and the current planet
//               becomes the game's travel branch (the restore's target set to -1). Ends the menu.
//   the close   rac1.ship.travel_to: the game's travel or landing, as for the ship. Puts a
//               restart's load level back at once, and does whatever the pad and tick missed. On
//               PAL the port never calls this hook (DESIGN.md section 2).
#include "rac1/game.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "openrac/games/rac1.hpp"
#include "rac1/discs.h"
#include "rac1/rac1.h"
#include "rac1/travel.h"

namespace travel_anywhere::rac1 {

namespace {

using openrac::gaddr;
namespace hooks = openrac::rac1;

// The number of levels: 0 (Veldin) to 18 (Veldin's second visit).
constexpr int kLevels = 19;
// The ticks a refusal's notice stays on screen.
constexpr int kNoticeTicks = 150;
// The end of the console's 32 MB, past which no game pointer goes.
constexpr gaddr kMemoryEnd = 0x02000000u;

// The settings, read once at load.
struct Settings {
    ta_combo combo = ta_combo_of(TA_COMBO_L1_R1_SELECT);
    int key = 0;  // a keyboard key (an openrac/keys.h bind), 0 for none
    bool trace = false;
};

// What the callbacks keep between calls. Reset in load (the load test loads the mod several times).
struct State {
    std::uint32_t pad_held = 0;  // pad 0's buttons held in its last report
    bool swallow = false;        // the combo's last button is hidden from the game until released
    bool request = false;        // the trigger fired: the menu opens at the end of the tick
    ta_session session{};        // the mod's planet select, from its opening to its close
    bool steered = false;        // steer_close has handled the menu's close
    int steer = TA_CLOSE_GAME;   // what steer_close made of it (TA_CLOSE_*)
    int restart = -1;            // the level a restart travels to, until its load level is set
    int notice_ticks = 0;        // the ticks the mod's notice has left on screen
};

openrac::Mod* s_mod = nullptr;
const ta_disc* s_disc = nullptr;
Settings s_settings;
State s_state;

// PauseAllSounds: opens the menu in the given state.
using OpenMenuFn = void (*)(int32_t);
OpenMenuFn s_open_menu = nullptr;

// Reads a T from game memory.
template <class T>
T get(gaddr address) {
    return s_mod->memory().read<T>(address);
}

// Writes a T to game memory.
template <class T>
void put(gaddr address, T value) {
    s_mod->memory().write<T>(address, value);
}

// Returns an executable global's address in the copy the loaded code uses.
gaddr exe(gaddr address) {
    return s_mod->relocate(address);
}

// Returns a global of level 0's program at its address in the loaded level's copy.
gaddr level0(gaddr address) {
    return s_mod->relocate_level(0, address);
}

// Whether a pointer read from game memory can be followed.
bool valid(gaddr address) {
    return address != 0 && address < kMemoryEnd;
}

// Returns the level whose program runs, or -1 for none.
int level_loaded() {
    const int level = s_mod->loaded_level();
    return level >= 0 && level < kLevels ? level : -1;
}

// Returns the level loop's game mode (0 in play).
int32_t game_mode() {
    return get<int32_t>(level0(s_disc->mode));
}

// Logs a line when the trace is on.
void trace(const std::string& line) {
    // The trace is for bug reports, so it is off unless asked for.
    if (s_settings.trace) {
        s_mod->info(line);
    }
}

// Reads the game state that ta_may_open decides on.
ta_play read_play() {
    ta_play p{};
    p.level = level_loaded();

    // With no level loaded, the rest of the game state is not read.
    if (p.level < 0) {
        return p;
    }

    const ta_disc& d = *s_disc;
    p.leaving = get<int32_t>(level0(d.level_exit)) != 0;
    p.mode = game_mode();
    p.mode_ticks = get<int32_t>(level0(d.mode_ticks));
    p.hero_bound = get<std::uint32_t>(d.hero + TA_HERO_MOBY) != 0;
    p.hp = get<int32_t>(d.hero + TA_HERO_HP);
    p.fall_fade = get<std::uint8_t>(d.hero + TA_HERO_FALL_FADE);
    p.state = get<int32_t>(d.hero + TA_HERO_STATE);
    p.control_mode = get<int32_t>(d.hero + TA_HERO_CONTROL_MODE);
    p.vehicle = get<std::uint8_t>(d.hero + TA_HERO_VEHICLE);
    p.hoverboard = get<std::uint8_t>(d.hero + TA_HERO_HOVERBOARD);
    p.switch_lock = get<std::uint8_t>(d.hero + TA_HERO_SWITCH_LOCK);

    const gaddr ship = get<std::uint32_t>(d.ship + TA_SHIP_MOBY);

    // The game boards only a ship that has a moby and is not hidden (DESIGN.md section 4).
    if (valid(ship)) {
        p.ship = (get<std::uint16_t>(ship + TA_MOBY_FLAGS) & TA_MOBY_HIDDEN) == 0;
        p.ship_scenes = get<std::uint8_t>(ship + TA_MOBY_STATE) == TA_SHIP_MOBY_SCENES;
    }

    p.ship_request = get<int32_t>(level0(d.ship_request)) != 0;

    p.freeze = (get<std::uint32_t>(d.freeze_flags) & d.freeze_mask) != 0;
    p.menu_pending = get<std::uint32_t>(exe(d.menu) + TA_MENU_PENDING_PAGE) != 0;
    p.camera_block = get<int32_t>(exe(d.camera_suppress)) != 0;
    p.card_busy =
        get<int32_t>(d.card + d.card_state) >= 3 || get<int32_t>(d.card + d.card_pending) >= 0;

    const gaddr debug = level0(d.debug);
    p.stepping = (get<int32_t>(debug + TA_DEBUG_FLAGS) & TA_DEBUG_SINGLE_STEP) != 0
                 || get<int32_t>(debug + TA_DEBUG_STEPS) != 0;
    return p;
}

// Formats the game state for the trace when the trigger fires.
std::string describe(const ta_play& p) {
    char line[320];
    std::snprintf(
        line,
        sizeof line,
        "level %d leaving %d mode %d ticks %d hero %d hp %d fall %d state %#x control %#x "
        "vehicle %d board %d lock %d ship %d scenes %d request %d freeze %d pending %d "
        "camera %d card %d step %d",
        p.level,
        p.leaving,
        p.mode,
        p.mode_ticks,
        p.hero_bound,
        p.hp,
        p.fall_fade,
        p.state,
        p.control_mode,
        p.vehicle,
        p.hoverboard,
        p.switch_lock,
        p.ship,
        p.ship_scenes,
        p.ship_request,
        p.freeze,
        p.menu_pending,
        p.camera_block,
        p.card_busy,
        p.stepping
    );
    return line;
}

// Shows `text` at the top of the screen for kNoticeTicks, where the port has notices. An empty
// text clears the notice.
void notice(const char* text) {
    // A port before interface 1.9 shows no notices, so the timer starts only when this one did.
    if (s_mod->play_notice(text)) {
        s_state.notice_ticks = text[0] != 0 ? kNoticeTicks : 0;
    }
}

// Returns the state of the page this poll's presses reach (ta_menu_page), read before the menu
// updates.
int menu_page() {
    const gaddr menu = exe(s_disc->menu);
    const gaddr next = get<std::uint32_t>(menu + TA_MENU_NEXT);
    const int next_state = valid(next) ? get<int32_t>(next + TA_PAGE_STATE) : 0;
    return ta_menu_page(
        get<int32_t>(menu + TA_MENU_STATE),
        get<int32_t>(menu + TA_MENU_TIMER),
        next != 0,
        next_state
    );
}

// Returns whether the ship's list (gGalaxyList) holds the level being played.
bool current_listed() {
    std::int32_t list[TA_GALAXY_LIST_LEN];

    // Copy the list out of game memory for ta_listed.
    for (int i = 0; i < TA_GALAXY_LIST_LEN; i++) {
        list[i] = get<std::int32_t>(s_disc->galaxy_list + 4u * i);
    }

    return ta_listed(list, TA_GALAXY_LIST_LEN, get<int32_t>(s_disc->current_level));
}

// Sets a restart's load level. ShipTravelTo's travel branch, run for no planet, sets the load level
// to -1 (the title); this puts the planet being played there instead, in the tick the branch ran or
// the next. The level loop reads it only after the ship's flight out, seconds later.
void set_restart_level() {
    const gaddr load = level0(s_disc->load_level);

    // Wait for a restart whose travel branch has run: game mode 6, load level -1.
    if (s_state.restart < 0 || game_mode() != 6 || get<int32_t>(load) != TA_NO_PLANET) {
        return;
    }

    put<int32_t>(load, s_state.restart);
    trace("restart: the load level set to planet " + std::to_string(s_state.restart));
    s_state.restart = -1;
}

// Steers the mod's menu while it closes (state 20, two ticks before the restore) by changing what
// update_pause_menu's restore reads (DESIGN.md sections 1 and 2). A back-out gets the map's close
// request instead of the ship's, so the game resumes play where Ratchet was. When the planet being
// played is chosen, the restore's target becomes no planet, so ShipTravelTo takes its travel branch
// (the save, the flight) rather than the landing; set_restart_level then puts this planet back as
// the load level.
void steer_close() {
    // Steer only the mod's own menu, and only once.
    if (!s_state.session.open || s_state.steered) {
        return;
    }

    const gaddr menu = exe(s_disc->menu);
    const bool closing = get<int32_t>(menu + TA_MENU_STATE) == TA_MENU_CLOSING
                         && get<int32_t>(menu + TA_MENU_CLOSE) == TA_CLOSE_SHIP;

    // Only a close that would call ShipTravelTo is steered.
    if (!closing) {
        return;
    }

    const gaddr target = level0(s_disc->menu_target);
    const int dest = get<int32_t>(target);
    const int current = get<int32_t>(s_disc->current_level);
    const int close = ta_close(&s_state.session, dest, current, current_listed());
    s_state.steered = true;
    s_state.steer = close;

    // A back-out takes the map's close, which resumes play in place.
    if (close == TA_CLOSE_BACK) {
        put<int32_t>(menu + TA_MENU_CLOSE, TA_CLOSE_TO_PLAY);
        trace(
            "backed out on planet " + std::to_string(current) + ": the map's close, play goes on"
        );
        return;
    }

    // The planet being played, chosen: send ShipTravelTo down its travel branch.
    if (close == TA_CLOSE_RESTART) {
        put<int32_t>(target, TA_NO_PLANET);
        s_state.restart = current;
        trace(
            "planet " + std::to_string(current)
            + " chosen, the one he is on: the game's travel to it"
        );
        return;
    }

    trace(
        "menu closing for planet " + std::to_string(dest)
        + (dest == current ? ": the ship's landing (not in the ship's list)"
                           : ": the ship's travel")
    );
}

// Handles a pad report before the game reads it (on_pad).
void pad(int port, std::uint8_t* report, int size) {
    // Only pad 0 drives the hero, and the report must hold the two button bytes.
    if (port != 0 || size < 2) {
        return;
    }

    const std::uint32_t held = ta_report_held(report);
    const std::uint32_t pressed = held & ~s_state.pad_held;
    s_state.pad_held = held;

    // While the mod's menu is up, record the presses that can close it; the menu reads this report.
    if (s_state.session.open && game_mode() == 3) {
        ta_session_press(&s_state.session, menu_page(), pressed);
        steer_close();
    }

    set_restart_level();

    // Hide the combo's last button from the game until it is released.
    if (s_state.swallow) {
        // Clear it in the report while it is still held.
        if (held & s_settings.combo.press) {
            ta_report_release(report, s_settings.combo.press);
            return;
        }

        s_state.swallow = false;
    }

    const int level = level_loaded();
    const bool combo = ta_combo_fires(s_settings.combo, held, pressed);

    // The mod takes the trigger only in a level's play; elsewhere the game keeps every button.
    if (level < 0
        || !ta_combo_is_ours(level, get<int32_t>(level0(s_disc->level_exit)) != 0, game_mode())) {
        // Log the ignored combo, for bug reports.
        if (combo) {
            trace(
                "combo outside play (level " + std::to_string(level) + ", mode "
                + std::to_string(level < 0 ? -1 : game_mode()) + "): the game's"
            );
        }

        return;
    }

    const bool key =
        s_settings.key != 0 && !s_mod->input().overlay() && s_mod->input().pressed(s_settings.key);

    // Neither trigger fired.
    if (!combo && !key) {
        return;
    }

    // Release the combo's last button, or the game's map (Select) would open too.
    if (combo) {
        s_state.swallow = true;
        ta_report_release(report, s_settings.combo.press);
    }

    s_state.request = true;
    trace(std::string(combo ? "combo" : "key") + " pressed in play");
}

// Forgets the mod's menu once it has gone: the level changed, or play is back with no menu waiting
// to reopen.
void end_session_if_over() {
    // No menu of the mod's is open.
    if (!s_state.session.open) {
        return;
    }

    const int level = level_loaded();

    // A new level (a travel or a load) took the menu away with the old one.
    if (level != s_state.session.level) {
        trace("menu over: the level changed");
        s_state.session = ta_session{};
        s_state.steered = false;
        s_state.restart = -1;
        return;
    }

    const bool back =
        game_mode() == 0 && get<std::uint32_t>(exe(s_disc->menu) + TA_MENU_PENDING_PAGE) == 0;

    // Play is back and no movie will reopen the menu.
    if (back) {
        trace(
            s_state.steered && s_state.steer == TA_CLOSE_BACK ? "menu over: play goes on in place"
                                                              : "menu over: play again"
        );
        s_state.session = ta_session{};
        s_state.steered = false;
        s_state.restart = -1;
    }
}

// Opens the planet select the way the end of the ship's take-off does
// (update_resident_gameplay_state: PauseAllSounds with the menu in state 14). Also zeroes the level
// loop's count of ticks in the game mode, as the loop does in the tick the mode changes; this tick
// has already been counted.
void open(const ta_play& p) {
    s_open_menu(TA_MENU_SHIP_ENTRY);

    // While the camera holds the pause off, PauseAllSounds only notes the pause and opens nothing.
    if (game_mode() != 3) {
        trace("PauseAllSounds left the game mode at " + std::to_string(game_mode()));
        return;
    }

    put<int32_t>(level0(s_disc->mode_ticks), 0);
    s_state.session = ta_session{1, p.level, 0};
    s_state.steered = false;
    s_state.restart = -1;

    // Clear the notice of an earlier refusal.
    if (s_state.notice_ticks > 0) {
        notice("");
    }

    trace("planet select open on level " + std::to_string(p.level));
}

// Runs after each game tick (on_frame).
void tick() {
    // Clear the notice when its time is up.
    if (s_state.notice_ticks > 0 && --s_state.notice_ticks == 0) {
        notice("");
    }

    set_restart_level();
    end_session_if_over();
    steer_close();

    // Nothing more to do unless the trigger fired.
    if (!s_state.request) {
        return;
    }

    s_state.request = false;
    const ta_play p = read_play();
    const int why = ta_may_open(&p);
    trace(std::string("trigger: ") + ta_why_name(why) + " (" + describe(p) + ")");

    // When refused, the game goes on and a notice tells the player why.
    if (why != TA_OPEN) {
        notice(ta_why_text(why));
        trace(
            std::string("notice ")
            + (s_state.notice_ticks > 0 ? "shown: " : "not shown (no notices): ") + ta_why_text(why)
        );
        return;
    }

    open(p);
}

// Replaces ShipTravelTo (rac1.ship.travel_to), which update_pause_menu's restore calls for close
// request 2, the planet select's close.
template <class Original>
void travel_to(Original original, int32_t dest) {
    // A menu the ship opened is left to the game.
    if (!s_state.session.open) {
        original(dest);
        return;
    }

    const int current = get<int32_t>(s_disc->current_level);
    // A restart arrives as no planet, the target steer_close set. One that arrives as the planet
    // being played (the steer missed, or an earlier mod changed the target) is the game's landing.
    const bool restart = dest == TA_NO_PLANET && s_state.restart >= 0;
    const int close =
        restart ? TA_CLOSE_RESTART : ta_close(&s_state.session, dest, current, current_listed());
    s_state.session = ta_session{};
    s_state.steered = false;

    // Without a restart there is no load level to put back.
    if (!restart) {
        s_state.restart = -1;
    }

    // A back-out that steer_close missed: do what the map's close does (update_pause_menu's play
    // branch). With close request 1, the restore then unpauses the music.
    if (close == TA_CLOSE_BACK) {
        put<int32_t>(level0(s_disc->mode), 0);
        put<int32_t>(exe(s_disc->menu) + TA_MENU_CLOSE, TA_CLOSE_TO_PLAY);
        trace("ShipTravelTo(" + std::to_string(dest) + ") backed out: play goes on in place");
        return;
    }

    // A steered restart: run the game's travel branch for no planet, then put the load level back.
    if (restart) {
        trace(
            "ShipTravelTo(" + std::to_string(dest) + ") on planet " + std::to_string(current)
            + ": the ship's travel to it, the level anew"
        );
        original(TA_NO_PLANET);
        set_restart_level();
        return;
    }

    trace(
        "ShipTravelTo(" + std::to_string(dest) + ") on planet " + std::to_string(current)
        + (dest == current ? ": the ship's landing" : ": the ship's travel")
    );
    original(dest);
}

// Returns the TA_COMBO_* choice for the combo setting's value; an unknown value is the default.
int combo_choice(const std::string& value) {
    // L2 and R2, then Select.
    if (value == "l2_r2_select") {
        return TA_COMBO_L2_R2_SELECT;
    }

    // No combo: only the key opens the menu, if one is set.
    if (value == "off") {
        return TA_COMBO_OFF;
    }

    return TA_COMBO_L1_R1_SELECT;
}

}  // namespace

void load(openrac::Mod& mod) {
    s_mod = &mod;

    // 1. The disc: find its row, or refuse. Never guess addresses.
    s_disc = nullptr;

    // Each row is a disc the mod has addresses for.
    for (const ta_disc& d : k_ta_discs) {
        // The row for the disc the port runs.
        if (mod.version() == d.version) {
            s_disc = &d;
        }
    }

    // Refuse a disc with no row.
    if (s_disc == nullptr) {
        throw openrac::Error(
            "Travel anywhere only works with the PAL v2.00 and NTSC-U v1.00 discs, not "
            + std::string(mod.version())
        );
    }

    // 2. The settings, read once.
    s_settings = Settings{};
    const std::string combo = mod.setting<std::string>("combo");
    s_settings.combo = ta_combo_of(combo_choice(combo));
    const std::string key = mod.setting<std::string>("key");
    const std::size_t comma = key.find(',');
    s_settings.key = openrac::bind_from_name(key.substr(0, comma));
    const char* env = std::getenv("OPENRAC_TRAVEL_ANYWHERE_TRACE");
    s_settings.trace = mod.setting<bool>("trace") || (env != nullptr && std::strcmp(env, "1") == 0);
    std::string missing;

    // A key needs the port's keyboard input (interface 1.2); without it the key is dropped.
    if (s_settings.key != 0 && !mod.has_input()) {
        s_settings.key = 0;
        missing = " (keyboard key ignored: this OpenRAC version can't give mods keyboard input)";
    }

    // 3. Reset every piece of state the callbacks keep (the load test loads the mod several times).
    s_state = State{};

    // With no trigger set, take nothing and leave the game as it is.
    if (s_settings.combo.press == 0 && s_settings.key == 0) {
        mod.info("inactive: Buttons is Off and no keyboard key is set" + missing);
        return;
    }

    // 4. The game function it calls, PauseAllSounds, which opens the menu. Missing means refuse.
    s_open_menu = mod.required_function<OpenMenuFn>(s_disc->open_menu);

    // 5. The hook point, then the pad and tick callbacks.
    mod.hook<hooks::Ship_TravelTo>([](auto original, int32_t dest) { travel_to(original, dest); });
    mod.on_pad([](int port, std::uint8_t* report, int size) { pad(port, report, size); });
    mod.on_frame([] { tick(); });

    // 6. One log line saying how it will play: the disc and the trigger.
    std::string trigger;

    // Name the combo, if one is set.
    if (s_settings.combo.press != 0) {
        trigger = s_settings.combo.hold == (TA_PAD_L2 | TA_PAD_R2) ? "L2 + R2 + Select"
                                                                   : "L1 + R1 + Select";
    }

    // Name the key, if one is set.
    if (s_settings.key != 0) {
        trigger += std::string(trigger.empty() ? "" : " or ") + "the key "
                   + std::string(openrac::bind_name(s_settings.key));
    }

    mod.info(
        "on the " + std::string(mod.version()) + " disc, " + trigger + " opens the planet select"
        + (s_settings.trace ? " (trace on)" : "") + missing
    );
}

}  // namespace travel_anywhere::rac1
