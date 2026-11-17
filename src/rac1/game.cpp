// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ratchet & Clank's part of the mod: the ship's planet select opened from anywhere in play, a
// back-out from it that plays on where Ratchet was, and the planet he is on, chosen, travelled to as
// any other planet is, the level starting anew (DESIGN.md). The decisions are src/rac1/travel.h,
// apart from game memory; this file reads the game into them and applies what they return.
// Addresses: discs.h; offsets: rac1.h.
//
// Where each part acts:
//   pad report  on_pad: the combo (or the key) in play asks for the menu, and its last button is
//               the mod's until let go; while the mod's menu is up, the presses that can close it,
//               and the close steered (below); after a restart's travel branch, its load level
//   tick's end  on_frame: the menu opened as the ship's take-off opens it (PauseAllSounds in the
//               ship's state 14), or a notice saying why not; the close steered: a back-out turned
//               into the map's close, which plays on in place, the planet he is on into the game's
//               travel branch (the restore's target -1); the menu's end
//   the close   rac1.ship.travel_to: the game's travel or landing, as the ship's; a restart's load
//               level put back at once; what the poll and the tick's end missed, done all the same
//               (on PAL the port calls no hook here: DESIGN.md section 2)
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
namespace hp = openrac::rac1;

// The levels the game has (0 Veldin to 18 Veldin's second visit).
constexpr int kLevels = 19;
// The ticks a refusal's notice stays on screen.
constexpr int kNoticeTicks = 150;
// The end of the console's 32 MB, past which no game pointer goes.
constexpr gaddr kMemoryEnd = 0x02000000u;

// The settings, read once at load.
struct Settings {
    ta_combo combo = ta_combo_of(TA_COMBO_L1_R1_SELECT);
    int key = 0;  // a keyboard key (openrac/keys.h's bind), 0 none
    bool trace = false;
};

// What the callbacks keep between calls. Reset in load (the load test loads the mod several times).
struct State {
    std::uint32_t pad_held = 0;  // pad 0's buttons in its last report, for the presses
    bool swallow = false;        // the combo's last button is the mod's until it is let go
    bool request = false;        // the trigger came: the menu opens at the tick's end
    ta_session session{};        // the mod's planet select, from its open to its close
    bool steered = false;        // its close looked at while closing (steer_close)
    int steer = TA_CLOSE_GAME;   // what that close was made: the game's, the map's, a restart
    int restart = -1;            // the level a restart travels to, until its load level is set
    int notice_ticks = 0;        // the ticks the mod's notice has left on screen
};

openrac::Mod* s_mod = nullptr;
const ta_disc* s_disc = nullptr;
Settings s_set;
State s;

using OpenMenuFn = void (*)(int32_t);
OpenMenuFn s_open_menu = nullptr;

template <class T>
T get(gaddr address) {
    return s_mod->memory().read<T>(address);
}

template <class T>
void put(gaddr address, T value) {
    s_mod->memory().write<T>(address, value);
}

// An executable global's address for the code loaded now.
gaddr exe(gaddr address) {
    return s_mod->relocate(address);
}

// A global of level 0's program, at the loaded level's copy.
gaddr level0(gaddr address) {
    return s_mod->relocate_level(0, address);
}

// Whether a pointer read from game memory can be followed.
bool valid(gaddr address) {
    return address != 0 && address < kMemoryEnd;
}

// The level whose program runs, or -1.
int level_loaded() {
    const int level = s_mod->loaded_level();
    return level >= 0 && level < kLevels ? level : -1;
}

int32_t game_mode() {
    return get<int32_t>(level0(s_disc->mode));
}

// A line in the log when the trace is on.
void trace(const std::string& line) {
    // Only for a bug report.
    if (s_set.trace) {
        s_mod->info(line);
    }
}

// The game as ta_may_open reads it.
ta_play read_play() {
    ta_play p{};
    p.level = level_loaded();

    // No level: nothing else is the game's yet.
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

    // The ship to board: its moby, shown (the game's own boarding needs both, DESIGN.md section 4).
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

// What the trace says of the game when the trigger comes.
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

// A line at the top of the picture, for a while, where the port can show one.
void notice(const char* text) {
    // A port before 1.9 shows none.
    if (s_mod->play_notice(text)) {
        s.notice_ticks = text[0] != 0 ? kNoticeTicks : 0;
    }
}

// The menu's page that a poll's presses reach (ta_menu_page), from the menu as the poll finds it.
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

// Whether the ship's list (gGalaxyList) holds the level played.
bool current_listed() {
    std::int32_t list[TA_GALAXY_LIST_LEN];

    // Each entry.
    for (int i = 0; i < TA_GALAXY_LIST_LEN; i++) {
        list[i] = get<std::int32_t>(s_disc->galaxy_list + 4u * i);
    }

    return ta_listed(list, TA_GALAXY_LIST_LEN, get<int32_t>(s_disc->current_level));
}

// The restart's load level: ShipTravelTo's travel branch, run for no planet, leaves the level loop's
// end bound for -1 (the title); the planet he is on goes there, in the tick it ran or the next
// (the loop's end reads it after the ship's flight out, seconds later).
void set_restart_level() {
    const gaddr load = level0(s_disc->load_level);

    // No restart waiting, or its travel branch not run yet.
    if (s.restart < 0 || game_mode() != 6 || get<int32_t>(load) != TA_NO_PLANET) {
        return;
    }

    put<int32_t>(load, s.restart);
    trace("restart: the load level set to planet " + std::to_string(s.restart));
    s.restart = -1;
}

// The mod's menu while it closes (its state 20, two ticks before play): what its restore does next,
// steered by what it reads (update_pause_menu's restore, DESIGN.md sections 1 and 2). A back-out:
// the close request the ship's travel would follow becomes the one the map's close follows, so the
// game itself plays on where Ratchet was. The planet he is on, chosen: the restore's target becomes
// no planet, so the game's ShipTravelTo takes its travel branch (the save, the flight) rather than
// the landing; the load level it then sets goes back to this planet (set_restart_level).
void steer_close() {
    // Not the mod's menu, or done already.
    if (!s.session.open || s.steered) {
        return;
    }

    const gaddr menu = exe(s_disc->menu);
    const bool closing = get<int32_t>(menu + TA_MENU_STATE) == TA_MENU_CLOSING
                         && get<int32_t>(menu + TA_MENU_CLOSE) == TA_CLOSE_SHIP;

    // Not closing for the ship yet.
    if (!closing) {
        return;
    }

    const gaddr target = level0(s_disc->menu_target);
    const int dest = get<int32_t>(target);
    const int current = get<int32_t>(s_disc->current_level);
    const int close = ta_close(&s.session, dest, current, current_listed());
    s.steered = true;
    s.steer = close;

    // Backed out: the map's close.
    if (close == TA_CLOSE_BACK) {
        put<int32_t>(menu + TA_MENU_CLOSE, TA_CLOSE_TO_PLAY);
        trace(
            "backed out on planet " + std::to_string(current) + ": the map's close, play goes on"
        );
        return;
    }

    // The planet he is on: the game's travel to it.
    if (close == TA_CLOSE_RESTART) {
        put<int32_t>(target, TA_NO_PLANET);
        s.restart = current;
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

// on_pad: pad 0's report, before the game reads it.
void pad(int port, std::uint8_t* report, int size) {
    // The hero's pad, with its buttons.
    if (port != 0 || size < 2) {
        return;
    }

    const std::uint32_t held = ta_report_held(report);
    const std::uint32_t pressed = held & ~s.pad_held;
    s.pad_held = held;

    // The mod's menu: the presses that can close it (the menu reads this report this tick).
    if (s.session.open && game_mode() == 3) {
        ta_session_press(&s.session, menu_page(), pressed);
        steer_close();
    }

    set_restart_level();

    // The combo's last button stays the mod's until it is let go: the game never sees it.
    if (s.swallow) {
        // Still held.
        if (held & s_set.combo.press) {
            ta_report_release(report, s_set.combo.press);
            return;
        }

        s.swallow = false;
    }

    const int level = level_loaded();
    const bool combo = ta_combo_fires(s_set.combo, held, pressed);

    // The trigger is the mod's only in a level's play: anywhere else the game keeps every button.
    if (level < 0
        || !ta_combo_is_ours(level, get<int32_t>(level0(s_disc->level_exit)) != 0, game_mode())) {
        // Said for a bug report.
        if (combo) {
            trace(
                "combo outside play (level " + std::to_string(level) + ", mode "
                + std::to_string(level < 0 ? -1 : game_mode()) + "): the game's"
            );
        }

        return;
    }

    const bool key =
        s_set.key != 0 && !s_mod->input().overlay() && s_mod->input().pressed(s_set.key);

    // Neither.
    if (!combo && !key) {
        return;
    }

    // The combo's last button: the game's map (Select) would open with it.
    if (combo) {
        s.swallow = true;
        ta_report_release(report, s_set.combo.press);
    }

    s.request = true;
    trace(std::string(combo ? "combo" : "key") + " pressed in play");
}

// The mod's menu has gone: a level left, or play again with no menu coming back.
void end_session_if_over() {
    // None open.
    if (!s.session.open) {
        return;
    }

    const int level = level_loaded();

    // Another level (the travel, a load): the menu went with the old one.
    if (level != s.session.level) {
        trace("menu over: the level changed");
        s.session = ta_session{};
        s.steered = false;
        s.restart = -1;
        return;
    }

    const bool back =
        game_mode() == 0 && get<std::uint32_t>(exe(s_disc->menu) + TA_MENU_PENDING_PAGE) == 0;

    // Play again, and no movie bringing the menu back.
    if (back) {
        trace(
            s.steered && s.steer == TA_CLOSE_BACK ? "menu over: play goes on in place"
                                                  : "menu over: play again"
        );
        s.session = ta_session{};
        s.steered = false;
        s.restart = -1;
    }
}

// Opens the planet select as the ship's take-off opens it (update_resident_gameplay_state at the
// take-off's end: PauseAllSounds, the menu in state 14), and the level loop's count of ticks in a
// game mode starts again, as it does in the tick the mode changes (that tick has had its count).
void open(const ta_play& p) {
    s_open_menu(TA_MENU_SHIP_ENTRY);

    // PauseAllSounds did not open it (it only notes a pause the camera holds off).
    if (game_mode() != 3) {
        trace("PauseAllSounds left the game mode at " + std::to_string(game_mode()));
        return;
    }

    put<int32_t>(level0(s_disc->mode_ticks), 0);
    s.session = ta_session{1, p.level, 0};
    s.steered = false;
    s.restart = -1;

    // A refusal's notice from before goes.
    if (s.notice_ticks > 0) {
        notice("");
    }

    trace("planet select open on level " + std::to_string(p.level));
}

// on_frame: after each game tick.
void tick() {
    // The notice's time.
    if (s.notice_ticks > 0 && --s.notice_ticks == 0) {
        notice("");
    }

    set_restart_level();
    end_session_if_over();
    steer_close();

    // No trigger.
    if (!s.request) {
        return;
    }

    s.request = false;
    const ta_play p = read_play();
    const int why = ta_may_open(&p);
    trace(std::string("trigger: ") + ta_why_name(why) + " (" + describe(p) + ")");

    // Refused: the game goes on, and the player is told why.
    if (why != TA_OPEN) {
        notice(ta_why_text(why));
        trace(
            std::string("notice ") + (s.notice_ticks > 0 ? "shown: " : "not shown (no notices): ")
            + ta_why_text(why)
        );
        return;
    }

    open(p);
}

// rac1.ship.travel_to: the planet select's close (update_pause_menu's restore, close request 2).
template <class Original>
void travel_to(Original original, int32_t dest) {
    // The ship's own menu: the game's.
    if (!s.session.open) {
        original(dest);
        return;
    }

    const int current = get<int32_t>(s_disc->current_level);
    // A restart: the target steered to no planet. The planet he is on handed here instead (the steer
    // missed, or a mod before this one changed the target) is the game's landing.
    const bool restart = dest == TA_NO_PLANET && s.restart >= 0;
    const int close =
        restart ? TA_CLOSE_RESTART : ta_close(&s.session, dest, current, current_listed());
    s.session = ta_session{};
    s.steered = false;

    // Not the steered target: no load level to put back.
    if (!restart) {
        s.restart = -1;
    }

    // A back-out the tick's end did not turn into the map's close: what that close does, by hand
    // (update_pause_menu's last branch, play; close request 1, after which it unpauses the music).
    if (close == TA_CLOSE_BACK) {
        put<int32_t>(level0(s_disc->mode), 0);
        put<int32_t>(exe(s_disc->menu) + TA_MENU_CLOSE, TA_CLOSE_TO_PLAY);
        trace("ShipTravelTo(" + std::to_string(dest) + ") backed out: play goes on in place");
        return;
    }

    // The steered restart: the game's travel branch, for no planet, then its load level put back.
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

// The combo setting's choice.
int combo_choice(const std::string& value) {
    // L2 and R2.
    if (value == "l2_r2_select") {
        return TA_COMBO_L2_R2_SELECT;
    }

    // None: the key alone, if one is set.
    if (value == "off") {
        return TA_COMBO_OFF;
    }

    return TA_COMBO_L1_R1_SELECT;
}

}  // namespace

void load(openrac::Mod& mod) {
    s_mod = &mod;

    // 1. The disc: its row, or a refusal. Never a guessed address.
    s_disc = nullptr;

    // Each disc the mod has addresses for.
    for (const ta_disc& d : k_ta_discs) {
        // The disc the port runs.
        if (mod.version() == d.version) {
            s_disc = &d;
        }
    }

    // A disc without a row.
    if (s_disc == nullptr) {
        throw openrac::Error(
            "rac1-travel-anywhere knows the PAL v2.00 and NTSC-U v1.00 discs, not "
            + std::string(mod.version())
        );
    }

    // 2. The settings, once.
    s_set = Settings{};
    const std::string combo = mod.setting<std::string>("combo");
    s_set.combo = ta_combo_of(combo_choice(combo));
    const std::string key = mod.setting<std::string>("key");
    const std::size_t comma = key.find(',');
    s_set.key = openrac::bind_from_name(key.substr(0, comma));
    const char* env = std::getenv("OPENRAC_TRAVEL_ANYWHERE_TRACE");
    s_set.trace = mod.setting<bool>("trace") || (env != nullptr && std::strcmp(env, "1") == 0);
    std::string missing;

    // A key needs the port's keyboard (interface 1.2).
    if (s_set.key != 0 && !mod.has_input()) {
        s_set.key = 0;
        missing = " (no key: this OpenRAC has no keyboard input for mods)";
    }

    // 3. Every piece of state the callbacks keep.
    s = State{};

    // No trigger at all: no callbacks, the game as it is.
    if (s_set.combo.press == 0 && s_set.key == 0) {
        mod.info("no trigger set (combo off, no key): nothing taken" + missing);
        return;
    }

    // 4. The game function: PauseAllSounds, the menu's open.
    s_open_menu = mod.required_function<OpenMenuFn>(s_disc->open_menu);

    // 5. The hook point, then the pad and the tick.
    mod.hook<hp::Ship_TravelTo>([](auto original, int32_t dest) { travel_to(original, dest); });
    mod.on_pad([](int port, std::uint8_t* report, int size) { pad(port, report, size); });
    mod.on_frame([] { tick(); });

    // 6. One line saying how it will play.
    std::string trigger;

    // The combo, if any.
    if (s_set.combo.press != 0) {
        trigger = s_set.combo.hold == (TA_PAD_L2 | TA_PAD_R2) ? "L2 + R2, then Select"
                                                              : "L1 + R1, then Select";
    }

    // The key, if any.
    if (s_set.key != 0) {
        trigger += std::string(trigger.empty() ? "" : " or ") + "the key "
                   + std::string(openrac::bind_name(s_set.key));
    }

    mod.info(
        "on the " + std::string(mod.version()) + " disc: the planet select opens with " + trigger
        + (s_set.trace ? ", trace on" : "") + missing
    );
}

}  // namespace travel_anywhere::rac1
