// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ratchet & Clank's part of the mod (game.cpp): the disc, the settings, the trigger, the hook.
#ifndef RAC1_TRAVEL_ANYWHERE_GAME_H
#define RAC1_TRAVEL_ANYWHERE_GAME_H

#include "openrac/mod.hpp"

namespace travel_anywhere::rac1 {

// Loads the mod on rac1: refuses a disc it has no addresses for, reads the settings, takes the pad,
// the tick and the ship's travel. Throws openrac::Error to refuse.
void load(openrac::Mod& mod);

}  // namespace travel_anywhere::rac1

#endif  // RAC1_TRAVEL_ANYWHERE_GAME_H
