#include "enemyAI.hpp"
#include "number/random.hpp"
#include "skyland/entity/projectile/missile.hpp"
#include "skyland/room_metatable.hpp"
#include "skyland/rooms/cannon.hpp"
#include "skyland/rooms/core.hpp"
#include "skyland/rooms/ionCannon.hpp"
#include "skyland/rooms/missileSilo.hpp"
#include "skyland/rooms/transporter.hpp"
#include "skyland/skyland.hpp"



// I wrote this AI code frantically for a game jam. I know it's kind of a mess.



namespace skyland {



void EnemyAI::update(Platform& pfrm, App& app, Microseconds delta)
{
    if (app.player_island().is_destroyed()) {
        return;
    }

    if (app.opponent_island()) {
        if (app.opponent_island()->power_supply() <
            app.opponent_island()->power_drain()) {
            resolve_insufficient_power(pfrm, app);
        }
    }


    next_action_timer_ -= delta;
    character_reassign_timer_ -= delta;

    if (next_action_timer_ <= 0) {
        next_action_timer_ = next_action_timeout;

        u8 matrix[16][16];
        app.player_island().plot_rooms(matrix);


        // NOTE: there should never be as many as 8 boarded ai characters at
        // once! I doubt that the game can realistically run AI code and
        // pathfinding for all of those entities. Of course, the game _can_
        // handle that many entities, but doing so would result in periodic long
        // pauses.
        Buffer<std::pair<BasicCharacter*, Room*>, 8> boarded_ai_characters;
        for (auto& room : app.player_island().rooms()) {
            for (auto& character : room->characters()) {
                if (character->owner() == this) {
                    boarded_ai_characters.push_back({
                            character.get(),
                            room.get()
                        });
                }
            }
        }


        if (app.opponent_island()) {
            for (auto& room : app.opponent_island()->rooms()) {
                if (auto cannon = dynamic_cast<Cannon*>(&*room)) {
                    set_target(pfrm, app, matrix, *cannon);
                } else if (auto silo = dynamic_cast<MissileSilo*>(&*room)) {
                    set_target(pfrm, app, matrix, *silo);
                } else if (auto ion_cannon = dynamic_cast<IonCannon*>(&*room)) {
                    set_target(pfrm, app, matrix, *ion_cannon);
                } else if (auto transporter =
                               dynamic_cast<Transporter*>(&*room)) {
                    if (length(transporter->characters()) and
                        transporter->ready()) {
                        auto transport_chr = transporter->characters().begin();
                        if ((*transport_chr)->state() not_eq
                                BasicCharacter::State::repair_room and
                            (*transport_chr)->owner() == this) {


                            // Justification for particularly nasty looking
                            // if-statement (below):
                            //
                            // Ok, so we only want to transport a character if
                            // another one of the characters isn't busy walking
                            // through the room.
                            //
                            // This fixes a bug where a character would be
                            // standing idle in the transporter, another
                            // character would be walking through, on the way to
                            // a different room, and the walking character,
                            // rather than the idle character, would be
                            // transported, creating a whole bunch of glitches.
                            if (not((*transport_chr)->has_movement_path() and
                                    not(*transport_chr)
                                           ->get_movement_path()
                                           ->empty() and
                                    (not((*(*transport_chr)
                                               ->get_movement_path())[0] ==
                                         (*transport_chr)->grid_position())))) {

                                transporter->random_transport_occupant(pfrm,
                                                                       app);
                            }
                        }
                    } else if (transporter->ready()) {
                        // If we have an infirmary, potentially transport some
                        // of our injured AIs back to heal.

                        bool found_infirmary = false;

                        auto metac = load_metaclass("infirmary");
                        for (auto& room : app.opponent_island()->rooms()) {
                            if (room->metaclass() == metac) {
                                found_infirmary = true;
                            }
                        }

                        if (found_infirmary) {
                            auto recover_pos =
                                [&]() -> std::optional<Vec2<u8>> {
                                for (auto it = boarded_ai_characters.begin();
                                     it not_eq boarded_ai_characters.end();) {
                                    if ((*it).first->health() < 25) {
                                        it = boarded_ai_characters.erase(it);
                                        return (*it).first->grid_position();
                                    } else {
                                        ++it;
                                    }
                                }
                                return {};
                            }();

                            if (not recover_pos) {
                                for (auto it = boarded_ai_characters.begin();
                                     it not_eq boarded_ai_characters.end();) {
                                    if (str_cmp((*(*it).second->metaclass())->name(),
                                                "plundered-room") == 0
                                        and (*it).first->idle_count() > 600) {
                                        // If we've plundered a room, and we've
                                        // been waiting a while, i.e. we have
                                        // nowhere to go, then we should recover
                                        // the character.
                                        it = boarded_ai_characters.erase(it);
                                        recover_pos = (*it).first->grid_position();
                                    } else {
                                        ++it;
                                    }
                                }
                            }

                            if (recover_pos) {
                                transporter->recover_character(
                                    pfrm, app, *recover_pos);
                            }
                        }
                    }
                }
            }
        }
    }

    if (character_reassign_timer_ <= 0) {
        character_reassign_timer_ = character_reassign_timeout;

        for (auto& room : app.player_island().rooms()) {
            for (auto& character : room->characters()) {
                if (character->owner() == this) {
                    assign_boarded_character(pfrm, app, *character);
                }
            }
        }

        if (app.opponent_island()) {
            for (auto& room : app.opponent_island()->rooms()) {
                for (auto& character : room->characters()) {
                    if (character->owner() == this) {
                        assign_local_character(pfrm, app, *character);
                    }
                }
            }
        }
    }
}



void EnemyAI::resolve_insufficient_power(Platform& pfrm, App& app)
{
    // When we have more power drain than power supply, we need to salvage a
    // bunch of structures to rebalance our power usage.

    // TODO: Make the AI less stupid.

    Room* lowest_weighted_room = nullptr;
    Float lowest_weight = 2000000.f;

    for (auto& room : app.opponent_island()->rooms()) {
        auto pwr = (*room->metaclass())->consumes_power();
        if (pwr > 0) {
            auto w = (*room->metaclass())->ai_base_weight();
            if (w < lowest_weight) {
                lowest_weighted_room = room.get();
                lowest_weight = w;
            }
        }
    }

    if (lowest_weighted_room) {
        // TODO: give coins back to the AI, so that it can potentially construct
        // something else. For the time being, we just want to salvage the
        // lowest-weighted rooms, until we have a stable balance of power, to
        // bring our systems back online.
        lowest_weighted_room->apply_damage(pfrm, app, 9999);
    }
}



u32 flood_fill(Platform& pfrm, u8 matrix[16][16], u8 replace, u8 x, u8 y);



void EnemyAI::assign_local_character(Platform& pfrm,
                                     App& app,
                                     BasicCharacter& character)
{
    // This code is so cluttered and sprawling. I had to write an AI and only
    // had two weeks for this project, so it's kind of a mess.

    // Basically, we first want to collect all of the reachable slots from the
    // character's position, via a breadth-first search. Then, we assign a
    // weight to each destination slot based on a series of heuristics. The
    // heursitics are the most convoluted and messy part. Then, we sort the
    // reachable slots by the assigned weights, and run an implementation of
    // Dijkstra's algorithm to find a path.

    if (character.has_movement_path()) {
        return;
    }

    Buffer<Vec2<u8>, 16> exclude_slots;

    // We may want to keep track of how many of the player's characters have
    // boarded our island. We might not want to transport to the player's island
    // if many of the player's characters have boarded.
    int player_characters_local = 0;
    int player_characters_remote = 0; // number of player characters on the
                                      // player's island.

    // The number of ai characters that have already boarded the player's
    // island.
    int ai_characters_remote = 0;
    int ai_characters_local = 0;

    // Should we board the player's castle? Well, if we have no weapons
    // remaining, a boarding party is the only way that we can deal damage,
    // so...
    int cannon_count = 0;
    int missile_count = 0;

    int player_cannon_count = 0;
    int player_missile_count = 0;

    for (auto& room : app.opponent_island()->rooms()) {
        if (room->metaclass() == cannon_mt) {
            ++cannon_count;
        }
        if (room->metaclass() == missile_silo_mt) {
            ++missile_count;
        }
        for (auto& other : room->characters()) {
            if (other->owner() == this and other.get() not_eq &character) {
                if (auto dest = other->destination()) {
                    exclude_slots.push_back(*dest);
                } else {
                    exclude_slots.push_back(other->grid_position());
                }
                ++ai_characters_local;
            } else if (other->owner() not_eq this) {
                ++player_characters_local;
            } else if (other.get() == &character) {
                ++ai_characters_local;
            }
        }
    }

    for (auto& room : app.player_island().rooms()) {
        if (room->metaclass() == cannon_mt) {
            ++player_cannon_count;
        }
        if (room->metaclass() == missile_silo_mt) {
            ++player_missile_count;
        }
        for (auto& chr : room->characters()) {
            if (chr->owner() == this) {
                ++ai_characters_remote;
            } else {
                ++player_characters_remote;
            }
        }
    }

    DynamicMemory<bool[16][16]> matrix_ = allocate_dynamic<bool[16][16]>(pfrm);

    app.opponent_island()->plot_walkable_zones(*matrix_);

    u8 matrix[16][16];
    for (int x = 0; x < 16; ++x) {
        for (int y = 0; y < 16; ++y) {
            if ((*matrix_)[x][y]) {
                matrix[x][y] = 1;
            } else {
                matrix[x][y] = 0;
            }
        }
    }

    auto current_pos = character.grid_position();

    flood_fill(pfrm, matrix, 2, current_pos.x, current_pos.y);

    struct Destination {
        Vec2<u8> coord_;
        Float ai_weight_;
    };

    Buffer<Destination, 48> slots;

    for (u8 x = 0; x < 16; ++x) {
        for (u8 y = 0; y < 16; ++y) {
            if (matrix[x][y] == 2) {
                slots.push_back({{x, y}, 0.f});
            }
        }
    }

    if (slots.empty()) {
        return; // hmm...
    }


    const auto infirmary_metac = load_metaclass("infirmary");


    for (auto& slot : slots) {
        if (auto room = app.opponent_island()->get_room(slot.coord_)) {

            const auto base_weight = (*room->metaclass())->ai_base_weight();

            // Increase room weight if damaged.
            slot.ai_weight_ =
                base_weight +
                (base_weight -
                 base_weight * (Float(room->health()) / room->max_health()));

            if (room->health() not_eq room->max_health()) {
                slot.ai_weight_ += 500.f;
            }

            slot.ai_weight_ -= 3 * manhattan_length(slot.coord_, current_pos);

            if (room->metaclass() == infirmary_metac) {
                // If our health is really low, we probably want to go to the
                // infirmary. If our health is just kinda low, we maybe want to
                // go to the infirmary.
                if (character.health() < 25) {
                    slot.ai_weight_ += 2000.f;
                } else if (character.health() < 200 and
                           not player_characters_local) {
                    slot.ai_weight_ += 2000.f;
                }
            } else if (auto transporter = dynamic_cast<Transporter*>(room)) {
                // Now, let's see. We want to raid the player's island if we
                // have more characters, but also...
                //
                // TODO: Increase the transporter weight a lot if we don't have
                // any remaining offensive capabilities. In that case, raids
                // would be our only offense.
                if (transporter->ready()) {

                    if (player_characters_remote >= ai_characters_remote and
                        ai_characters_remote + ai_characters_local >
                            player_characters_remote) {
                        slot.ai_weight_ += 400.f * ai_characters_local;
                    }
                    if (player_characters_local > ai_characters_local) {
                        slot.ai_weight_ -= 250.f * (player_characters_local -
                                                    ai_characters_local);
                    }
                    const auto weapon_count = cannon_count + missile_count;
                    if (weapon_count == 0) {
                        // If we don't have any remaining weapons, potentially
                        // board the player's castle, even if doing so would be
                        // a suicide mission.
                        slot.ai_weight_ += 1000.f;
                    } else if (missile_count and cannon_count == 0 and player_cannon_count) {
                        slot.ai_weight_ += 400.f;
                    } else if (cannon_count and missile_count == 0 and player_missile_count) {
                        slot.ai_weight_ += 400.f;
                    }
                } else {
                    slot.ai_weight_ -= 300;
                }
            }
        }
        for (auto& exc : exclude_slots) {
            if (slot.coord_ == exc) {
                // Don't move into a slot targeted by another one of our ai
                // characters.
                slot.ai_weight_ = -2000.f;
            }
        }
    }

    std::sort(slots.begin(),
              slots.end(),
              [&](const Destination& lhs, const Destination& rhs) {
                  return lhs.ai_weight_ < rhs.ai_weight_;
              });

    if (slots.back().ai_weight_ == 0.f) {
        // Again, perhaps this is overly defensive coding. But we should never
        // end up in a situation where the weights of the rooms are all
        // uninitialized...
        return;
    }

    auto target = slots.back();

    if (auto path = find_path(
            pfrm, &*app.opponent_island(), current_pos, target.coord_)) {
        if (not((*path)->size() == 1 and
                (**path)[0] == character.grid_position())) {
            // Don't waste a path buffer on an entity if the ideal path
            // represents a single node with the character's current position.
            character.set_movement_path(std::move(*path));
        }
    }
}



void EnemyAI::assign_boarded_character(Platform& pfrm,
                                       App& app,
                                       BasicCharacter& character)
{
    // Ok, so here, our ai character has boarded the player's island. We want to
    // find an ideal position to move the character to, with consideration to
    // the layout of the player's island. We can only move into a connected
    // component, and we want to attack a room with a high weight.

    // TODO: we may want to decrease the weight of a room if one of the player's
    // own characters stands between us and the target room. An unprotected room
    // is of course better than a guarded one.

    if (character.has_movement_path()) {
        // we've already assigned a movement path to the character. We may want
        // to recalculate the path at some point. But the computations below are
        // a bit expensive, so, instead, why don't we just defer the computation
        // until a room in the player's island is actually destroyed?
        return;
    }


    Buffer<Vec2<u8>, 16> exclude_slots; // Don't move into currently occupied
                                        // slots, or slots that will be
                                        // occupied.

    for (auto& room : app.player_island().rooms()) {
        for (auto& other : room->characters()) {
            if (other->owner() == this and other.get() not_eq &character) {
                if (auto dest = other->destination()) {
                    exclude_slots.push_back(*dest);
                } else {
                    exclude_slots.push_back(other->grid_position());
                }
            }
        }
    }


    DynamicMemory<bool[16][16]> matrix_ = allocate_dynamic<bool[16][16]>(pfrm);

    app.player_island().plot_walkable_zones(*matrix_);

    u8 matrix[16][16];
    for (int x = 0; x < 16; ++x) {
        for (int y = 0; y < 16; ++y) {
            if ((*matrix_)[x][y]) {
                matrix[x][y] = 1;
            } else {
                matrix[x][y] = 0;
            }
        }
    }

    auto current_pos = character.grid_position();

    flood_fill(pfrm, matrix, 2, current_pos.x, current_pos.y);

    struct Destination {
        Vec2<u8> coord_;
        Float ai_weight_;
    };

    Buffer<Destination, 48> slots;

    for (u8 x = 0; x < 16; ++x) {
        for (u8 y = 0; y < 16; ++y) {
            if (matrix[x][y] == 2) {
                slots.push_back({{x, y}, 0.f});
            }
        }
    }

    if (slots.empty()) {
        // TODO: what to do in this case? How can we even end up in a situation
        // like this? If all of the player's rooms were destroyed, and one of
        // the player's rooms retain a unique pointer to the invading enemy
        // character, then it should be theoretically impossible for us to end
        // up in this state.
        return;
    }

    for (auto& slot : slots) {
        if (auto room = app.player_island().get_room(slot.coord_)) {
            slot.ai_weight_ = (*room->metaclass())->ai_base_weight();
            slot.ai_weight_ -= 3 * manhattan_length(slot.coord_, current_pos);

            Float player_chr_remove_weight = 0.f;
            for (auto& chr : room->characters()) {
                // If the player has a bunch of characters in the room, label it
                // as "toxic" and resist allocating entities to the room (unless
                // it's really valuable).
                if (chr->owner() not_eq this) {
                    player_chr_remove_weight += 100.f;

                    if (character.health() < 50) {
                        // More likely to flee the room if we're badly injured.
                        player_chr_remove_weight += 100.f;
                    }
                }
            }

            // NOTE: exclude_slots.size() + 1 gives us the total number of
            // boarded (invading) characters that the AI controls.  We want to
            // avoid entering a room if it is crammed with defending characters,
            // but also, we want to divide up the weight that we plan to
            // subtract off by the number of our own AI characters, so that we
            // will still potentially attack the room if we have enough
            // characters to (potentially) overwhelm the occupants.
            slot.ai_weight_ -=
                player_chr_remove_weight / (0.75f * (exclude_slots.size() + 1));
        }
        for (auto& exc : exclude_slots) {
            if (slot.coord_ == exc) {
                // Don't move into a slot targeted by another one of our ai
                // characters.
                slot.ai_weight_ = -2000.f;
            }
        }
    }

    std::sort(slots.begin(),
              slots.end(),
              [&](const Destination& lhs, const Destination& rhs) {
                  return lhs.ai_weight_ < rhs.ai_weight_;
              });

    if (slots.back().ai_weight_ == 0.f) {
        // Again, perhaps this is overly defensive coding. But we should never
        // end up in a situation where the weights of the rooms are all
        // uninitialized...
        return;
    }

    auto target = slots.back();

    if (auto path =
            find_path(pfrm, &app.player_island(), current_pos, target.coord_)) {
        if (not((*path)->size() == 1 and
                (**path)[0] == character.grid_position())) {
            // Don't waste a path buffer on an entity if the ideal path
            // represents a single node with the character's current position.
            character.set_movement_path(std::move(*path));
        }
    }
}



void EnemyAI::set_target(Platform& pfrm,
                         App& app,
                         const u8 matrix[16][16],
                         IonCannon& ion_cannon)
{
    Room* highest_weighted_room = nullptr;
    Float highest_weight = 3E-5;

    for (auto& room : app.player_island().rooms()) {
        auto meta_c = room->metaclass();

        if (meta_c not_eq forcefield_mt) {
            continue;
        }

        auto w = (*meta_c)->ai_base_weight();


        w += 3 * manhattan_length(room->origin(), ion_cannon.origin());


        if (w > highest_weight) {
            highest_weighted_room = room.get();
            highest_weight = w;
        }
    }

    if (highest_weighted_room) {
        ion_cannon.set_target(highest_weighted_room->position());
    }
}



void EnemyAI::set_target(Platform& pfrm,
                         App& app,
                         const u8 matrix[16][16],
                         MissileSilo& silo)
{
    if (silo.parent()->get_drift()) {
        // Wait until we've stopped moving
        return;
    }

    Buffer<Room*, 32> visible_rooms;

    for (int x = 0; x < 16; ++x) {
        for (int y = 0; y < 16; ++y) {
            if (matrix[x][y]) {
                if (auto room = app.player_island().get_room({u8(x), u8(y)})) {
                    visible_rooms.push_back(room);
                }
                break;
            }
        }
    }

    Room* highest_weighted_room = nullptr;
    Float highest_weight = 3E-5;

    auto cannon_metac = load_metaclass("cannon");

    bool cannons_remaining = false;

    for (auto& room : silo.parent()->rooms()) {
        if (room->metaclass() == cannon_metac) {
            cannons_remaining = true;
            break;
        }
    }

    for (auto room : visible_rooms) {
        auto meta_c = room->metaclass();
        auto w = (*meta_c)->ai_base_weight();

        for (auto& chr : room->characters()) {
            const bool ai_owns_character = chr->owner() == this;
            if (ai_owns_character) {
                // Sometimes we want to blow up a room that contains some of our
                // own invading characters, but often, we don't :)
                //
                // We can make this calculation a bit more complex,
                // though. E.G. if we're attacking a power core, and we don't
                // realistically have enough weapons to destroy the player's
                // offensive capabilities, then the only path to victory may be
                // through destroying rooms containing our own characters.
                w -= 200.f;
            }
        }

        // Give the room some extra weight, if firing a missile into it would be
        // really destructive.
        if (w > 400 and room->health() <= Missile::deals_damage) {
            w += 300.f;
        }

        // We don't have any cannons left, but the other player does. Try to
        // take out some of those cannons with missiles.
        if (meta_c == cannon_metac and not cannons_remaining) {
            w += 200.f;
        }

        if (w > highest_weight) {
            highest_weighted_room = room;
            highest_weight = w;
        }
    }

    if (highest_weighted_room) {
        if (visible_rooms.size() > 1 and rng::choice<4>(rng::utility_state) == 0) {
            silo.set_target(visible_rooms[rng::choice(visible_rooms.size(),
                                                      rng::utility_state)]->position());
        } else {
            silo.set_target(highest_weighted_room->position());
        }
    }
}



void EnemyAI::set_target(Platform& pfrm,
                         App& app,
                         const u8 matrix[16][16],
                         Cannon& cannon)
{
    // Ok, lets start by finding all of the line-of-sight targets:
    // (FIXME: actually draw the line-of-sight correctly...)
    Buffer<Room*, 32> visible_rooms;

    for (u8 y = 0; y < 16; ++y) {
        for (int x = 15; x > -1; --x) {
            if (matrix[x][y]) {
                if (auto room = app.player_island().get_room({u8(x), y})) {
                    visible_rooms.push_back(room);
                }
                break;
            }
        }
    }

    Room* highest_weighted_room = nullptr;
    Float highest_weight = 3E-5;

    for (auto room : visible_rooms) {
        auto meta_c = room->metaclass();
        auto w = (*meta_c)->ai_base_weight();

        if (w > highest_weight) {
            highest_weighted_room = room;
            highest_weight = w;
        }
    }

    // Potentially attack the second highest weighted visible room, just to keep
    // things interesting.
    if (visible_rooms.size() > 1 and rng::choice<3>(rng::utility_state) == 0) {
        highest_weighted_room = visible_rooms[1];
    }

    if (highest_weighted_room) {
        cannon.set_target(highest_weighted_room->position());
    }
}



void EnemyAI::on_room_damaged(Platform& pfrm, App& app, Room& room)
{
    if (app.opponent_island()) {
    }
}



} // namespace skyland
