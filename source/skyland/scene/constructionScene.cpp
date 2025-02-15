#include "constructionScene.hpp"
#include "globals.hpp"
#include "localization.hpp"
#include "platform/platform.hpp"
#include "readyScene.hpp"
#include "skyland/network.hpp"
#include "skyland/room_metatable.hpp"
#include "skyland/scene_pool.hpp"
#include "skyland/skyland.hpp"
#include "skyland/tile.hpp"
#include "worldScene.hpp"
#include "salvageRoomScene.hpp"



namespace skyland {



int construction_zone_min_y = 7;



static Coins get_cost(App& app, const RoomMeta& meta)
{
    Coins cost = meta->cost();
    for (int i = 0; i < app.player_island().workshop_count(); ++i) {
        cost *= 0.9f;
    }
    if (cost < meta->cost() * salvage_factor) {
        // You shouldn't be able to farm coins by building a bunch of workshops
        // and salvaging rooms repeatedly.
        return meta->cost() * salvage_factor;
    }
    return cost;
}



static bool show_construction_icons = true;



ScenePtr<Scene>
ConstructionScene::update(Platform& pfrm, App& app, Microseconds delta)
{
    if (pfrm.keyboard().down_transition<Key::alt_2>() or
        (state_ == State::select_loc and key_down<Key::action_2>(pfrm))) {
        auto& cursor_loc =
            std::get<SkylandGlobalData>(globals()).near_cursor_loc_;
        if (not construction_sites_.empty()) {
            cursor_loc.x = construction_sites_[selector_].x;
            cursor_loc.y = construction_sites_[selector_].y;
        }
        return scene_pool::alloc<ReadyScene>();
    }

    if (auto new_scene = ActiveWorldScene::update(pfrm, app, delta)) {
        return new_scene;
    }

    switch (state_) {
    case State::select_loc:
        if (key_down<Key::right>(pfrm) and
            selector_ < construction_sites_.size() - 1) {
            ++selector_;
        }

        if (key_down<Key::left>(pfrm) and selector_ > 0) {
            --selector_;
        }

        if (not construction_sites_.empty()) {
            auto& cursor_loc =
                std::get<SkylandGlobalData>(globals()).near_cursor_loc_;
            cursor_loc.x = construction_sites_[selector_].x;
            cursor_loc.y = construction_sites_[selector_].y;
        }

        if (key_down<Key::action_1>(pfrm) and not construction_sites_.empty()) {

            if (construction_sites_[selector_].y == 15) {
                // Special case: we want to add to the terrain level, not
                // construct a building.
                state_ = State::add_terrain;
                msg(pfrm, ":build :add-terrain");

            } else {
                collect_available_buildings(pfrm, app);

                if (not available_buildings_.empty()) {
                    state_ = State::choose_building;
                    show_current_building_text(pfrm, app);
                }
            }
        }
        break;

    case State::choose_building:
        if (key_down<Key::action_2>(pfrm)) {
            find_construction_sites(pfrm, app);
            state_ = State::select_loc;
            msg(pfrm, ":build");
            break;
        }

        if (key_down<Key::start>(pfrm)) {
            show_construction_icons = not show_construction_icons;
            show_current_building_text(pfrm, app);
        }

        if (key_down<Key::down>(pfrm)) {
            building_selector_ += 5;
            building_selector_ %= available_buildings_.size();
            show_current_building_text(pfrm, app);
        }

        if (key_down<Key::up>(pfrm)) {
            building_selector_ -= 5;
            building_selector_ %= available_buildings_.size();
            show_current_building_text(pfrm, app);
        }

        if (key_down<Key::right>(pfrm)) {
            if (building_selector_ < (int)available_buildings_.size() - 1) {
                ++building_selector_;
                show_current_building_text(pfrm, app);
            } else {
                building_selector_ = 0;
                show_current_building_text(pfrm, app);
            }
        }

        if (key_down<Key::left>(pfrm)) {
            if (building_selector_ > 0) {
                --building_selector_;
                show_current_building_text(pfrm, app);
            } else {
                building_selector_ = available_buildings_.size() - 1;
                show_current_building_text(pfrm, app);
            }
        }

        if (key_down<Key::action_1>(pfrm)) {
            const auto& target = *available_buildings_[building_selector_];

            if (app.coins() < get_cost(app, target)) {
                msg(pfrm, "insufficent funds!");
                state_ = State::insufficent_funds;
                break;
            }

            if (app.player_island().power_supply() -
                    app.player_island().power_drain() <
                target->consumes_power()) {
                msg(pfrm, "insufficient power supply!");
                state_ = State::insufficent_funds;
                break;
            }

            const auto diff = get_cost(app, target);
            app.coins() -= diff;
            app.level_coins_spent() += diff;

            const auto sz = target->size().y;
            const u8 dest_x = construction_sites_[selector_].x;
            const u8 dest_y = construction_sites_[selector_].y - (sz - 1);
            target->create(pfrm, &app.player_island(), {dest_x, dest_y});

            app.player().rooms_built_++;

            network::packet::RoomConstructed packet;
            packet.metaclass_index_.set(metaclass_index(target->name()));
            packet.x_ = dest_x;
            packet.y_ = dest_y;
            network::transmit(pfrm, packet);

            find_construction_sites(pfrm, app);

            state_ = State::select_loc;
            msg(pfrm, ":build");
        }
        break;

    case State::insufficent_funds:
        if (pfrm.keyboard().down_transition<Key::action_2, Key::action_1>()) {
            find_construction_sites(pfrm, app);
            state_ = State::select_loc;
            msg(pfrm, ":build");
        }
        break;

    case State::add_terrain:
        if (key_down<Key::action_2>(pfrm)) {
            find_construction_sites(pfrm, app);
            state_ = State::select_loc;
            msg(pfrm, ":build");
            break;
        }

        if (key_down<Key::action_1>(pfrm)) {
            if (app.coins() < app.terrain_cost()) {
                msg(pfrm, "insufficent funds!");
                state_ = State::insufficent_funds;
                break;
            }

            app.coins() -= app.terrain_cost();

            auto& terrain = app.player_island().terrain();
            terrain.pop_back(); // the old edge tile
            terrain.push_back(Tile::terrain_middle);
            terrain.push_back(Tile::terrain_right);

            app.player_island().render_terrain(pfrm);

            network::packet::TerrainConstructed packet;
            packet.new_terrain_size_ = app.player_island().terrain().size();
            network::transmit(pfrm, packet);


            find_construction_sites(pfrm, app);
            state_ = State::select_loc;

            msg(pfrm, ":build");
        }
        break;
    }

    return null_scene();
}


void ConstructionScene::show_current_building_text(Platform& pfrm, App& app)
{
    StringBuffer<32> str = ":build :";

    str += (*available_buildings_[building_selector_])->name();
    str += " ";
    str += to_string<10>(
        get_cost(app, (*available_buildings_[building_selector_])));
    str += "$";
    str += " ";
    str += to_string<10>(
        (*available_buildings_[building_selector_])->consumes_power());
    str += "`";

    msg(pfrm, str.c_str());


    auto st = calc_screen_tiles(pfrm);

    if (not show_construction_icons) {
        return;
    }

    for (int i = st.x - 25; i < st.x - 5; ++i) {
        pfrm.set_tile(Layer::overlay, i, st.y - 6, 425);
    }

    for (int y = st.y - 5; y < st.y - 2; ++y) {
        pfrm.set_tile(Layer::overlay, st.x - 26, y, 128);
        pfrm.set_tile(Layer::overlay, st.x - 5, y, 433);
    }

    pfrm.set_tile(Layer::overlay, st.x - 26, st.y - 2, 419);
    pfrm.set_tile(Layer::overlay, st.x - 5, st.y - 2, 418);

    {
        int index = building_selector_;
        if (index - 2 < -1) {
            index = available_buildings_.size() - 2;
        } else if (index - 2 < 0) {
            index = available_buildings_.size() - 1;
        } else {
            index = index - 2;
        }

        auto icon = (*available_buildings_[index])->unsel_icon();
        draw_image(pfrm,
                   258,
                   st.x - 25,
                   st.y - 5,
                   4,
                   4,
                   Layer::overlay);

        pfrm.load_overlay_chunk(258, icon, 16);
    }

    {
        int index = building_selector_;
        if (index - 1 < 0) {
            index = available_buildings_.size() - 1;
        } else {
            index = index - 1;
        }

        auto icon = (*available_buildings_[index])->unsel_icon();
        draw_image(pfrm,
                   181,
                   st.x - 21,
                   st.y - 5,
                   4,
                   4,
                   Layer::overlay);

        pfrm.load_overlay_chunk(181, icon, 16);
    }

    {
        auto icon = (*available_buildings_[building_selector_])->icon();
        draw_image(pfrm,
                   197,
                   st.x - 17,
                   st.y - 5,
                   4,
                   4,
                   Layer::overlay);

        pfrm.load_overlay_chunk(197, icon, 16);
    }

    {
        int index = building_selector_;
        if (index + 1 >= (int)available_buildings_.size()) {
            index = 0;
        } else {
            index = index + 1;
        }

        auto icon = (*available_buildings_[index])->unsel_icon();
        draw_image(pfrm,
                   213,
                   st.x - 13,
                   st.y - 5,
                   4,
                   4,
                   Layer::overlay);

        pfrm.load_overlay_chunk(213, icon, 16);
    }

    {
        int index = building_selector_;
        if (index + 1 >= (int)available_buildings_.size()) {
            index = 1;
        } else if (index + 2 >= (int)available_buildings_.size()) {
            index = 0;
        } else {
            index = index + 2;
        }

        auto icon = (*available_buildings_[index])->unsel_icon();
        draw_image(pfrm,
                   274,
                   st.x - 9,
                   st.y - 5,
                   4,
                   4,
                   Layer::overlay);

        pfrm.load_overlay_chunk(274, icon, 16);
    }

}



void ConstructionScene::display(Platform& pfrm, App& app)
{
    WorldScene::display(pfrm, app);

    switch (state_) {
    case State::insufficent_funds:
        break;

    case State::select_loc:
        if (not construction_sites_.empty()) {
            auto origin = app.player_island().origin();

            origin.x += construction_sites_[selector_].x * 16;
            origin.y += (construction_sites_[selector_].y) * 16;

            Sprite sprite;
            sprite.set_position(origin);
            sprite.set_texture_index(12);
            sprite.set_size(Sprite::Size::w16_h32);


            pfrm.screen().draw(sprite);
        }
        break;


    case State::choose_building:
        if (not available_buildings_.empty()) {
            const auto& meta = *available_buildings_[building_selector_];
            const auto sz = meta->size();

            auto origin = app.player_island().origin();
            origin.x += construction_sites_[selector_].x * 16;
            origin.y += (construction_sites_[selector_].y - (sz.y - 1)) * 16;

            if (sz.x == 1 and sz.y == 1) {
                Sprite sprite;
                sprite.set_texture_index(14);
                sprite.set_size(Sprite::Size::w16_h32);
                sprite.set_position({origin.x, origin.y - 16});
                pfrm.screen().draw(sprite);
            } else {
                Sprite sprite;
                sprite.set_texture_index(13);
                sprite.set_size(Sprite::Size::w16_h32);

                for (int x = 0; x < sz.x; ++x) {
                    for (int y = 0; y < sz.y / 2; ++y) {
                        sprite.set_position(
                            {origin.x + x * 16, origin.y + y * 32});
                        pfrm.screen().draw(sprite);
                    }
                }
            }
        }
        break;


    case State::add_terrain: {
        auto& terrain = app.player_island().terrain();
        const Vec2<u8> loc = {u8(terrain.size()), 15};
        auto origin = app.player_island().origin();
        origin.x += loc.x * 16;
        origin.y -= 32;
        Sprite sprite;
        sprite.set_texture_index(14);
        sprite.set_size(Sprite::Size::w16_h32);
        sprite.set_position(origin);
        pfrm.screen().draw(sprite);
        break;
    }
    }
}



void ConstructionScene::find_construction_sites(Platform& pfrm, App& app)
{
    construction_sites_.clear();

    bool matrix[16][16];

    app.player_island().plot_construction_zones(matrix);

    for (u8 x = 0; x < 16; ++x) {
        for (u8 y = 0; y < 16; ++y) {
            if (matrix[x][y] and y > construction_zone_min_y) {
                construction_sites_.push_back({x, y});
            }
        }
    }

    auto& terrain = app.player_island().terrain();
    if (not terrain.full() and not pfrm.network_peer().is_connected()) {
        construction_sites_.push_back({u8(terrain.size()), 15});
    }

    if (construction_sites_.empty()) {
        selector_ = 0;
    } else if (selector_ >= construction_sites_.size()) {
        selector_--;
    }
}



void ConstructionScene::msg(Platform& pfrm, const char* text)
{
    auto st = calc_screen_tiles(pfrm);
    text_.emplace(pfrm, text, OverlayCoord{0, u8(st.y - 1)});

    const int count = st.x - text_->len();
    for (int i = 0; i < count; ++i) {
        pfrm.set_tile(Layer::overlay, i + text_->len(), st.y - 1, 426);
    }

    for (int i = 0; i < st.x; ++i) {
        pfrm.set_tile(Layer::overlay, i, st.y - 2, 425);
        pfrm.set_tile(Layer::overlay, i, st.y - 3, 0);
        pfrm.set_tile(Layer::overlay, i, st.y - 4, 0);
        pfrm.set_tile(Layer::overlay, i, st.y - 5, 0);
        pfrm.set_tile(Layer::overlay, i, st.y - 6, 0);
    }
}



void ConstructionScene::collect_available_buildings(Platform& pfrm, App& app)
{
    available_buildings_.clear();

    int avail_space = 1;

    const auto current = construction_sites_[selector_];
    for (auto& site : construction_sites_) {
        if (site.y == current.y and site.x == current.x + 1) {
            // FIXME: buildings wider than 2, various other cases
            avail_space = 2;
        }
    }

    const int avail_y_space = current.y - construction_zone_min_y;

    const auto w_count = app.player_island().workshop_count();

    auto metatable = room_metatable();
    for (int i = 0; i < metatable.second; ++i) {
        auto& meta = metatable.first[i];

        const bool workshop_required =
            (meta->conditions() & Conditions::workshop_required);

        if (meta->size().x <= avail_space and
            meta->size().y <= avail_y_space and
            (not workshop_required or (workshop_required and w_count > 0)) and
            not(meta->conditions() & Conditions::not_constructible)) {
            available_buildings_.push_back(&meta);
        }
    }

    if (building_selector_ >= (int)available_buildings_.size()) {
        building_selector_ = available_buildings_.size() - 1;
    }
}



void ConstructionScene::enter(Platform& pfrm, App& app, Scene& prev)
{
    WorldScene::enter(pfrm, app, prev);

    persist_ui();

    find_construction_sites(pfrm, app);

    if (not construction_sites_.empty()) {
        auto& cursor_loc =
            std::get<SkylandGlobalData>(globals()).near_cursor_loc_;

        for (u32 i = 0; i < construction_sites_.size(); ++i) {
            if (construction_sites_[i].x == cursor_loc.x) {

                selector_ = i;
            }
        }
    }

    msg(pfrm, ":build");
}



void ConstructionScene::exit(Platform& pfrm, App& app, Scene& next)
{
    WorldScene::exit(pfrm, app, next);

    text_.reset();
    pfrm.fill_overlay(0);
}



} // namespace skyland
