#include "selectChallengeScene.hpp"
#include "fadeInScene.hpp"
#include "skyland/scene_pool.hpp"
#include "skyland/skyland.hpp"
#include "titleScreenScene.hpp"



namespace skyland {



static const Float default_fade = 0.6f;



void SelectChallengeScene::enter(Platform& pfrm, App&, Scene& prev)
{
    pfrm.load_overlay_texture("overlay_challenges");

    pfrm.enable_feature("v-parallax", false);

    if (auto script = pfrm.load_file_contents("scripts", "challenge.lisp")) {
        auto result = lisp::dostring(script, [&pfrm](lisp::Value& err) {
            lisp::DefaultPrinter p;
            lisp::format(&err, p);
            pfrm.fatal(p.fmt_.c_str());
        });
        challenges_ = result;
        const auto challenge_count = lisp::length(result);
        page_count_ = challenge_count / 5 + (challenge_count % 5 ? 1 : 0);

    } else {
        pfrm.fatal("missing file challenge.lisp");
    }

    show_options(pfrm);

    for (int i = 0; i < 16; ++i) {
        for (int j = 0; j < 16; ++j) {
            pfrm.set_tile(Layer::map_0_ext, i, j, 0);
            pfrm.set_tile(Layer::map_1_ext, i, j, 0);
        }
    }

    pfrm.delta_clock().reset();

    pfrm.screen().set_view({});

    pfrm.screen().fade(default_fade, ColorConstant::rich_black, {}, false);
}



void SelectChallengeScene::show_options(Platform& pfrm)
{
    pfrm.screen().clear();
    text_.clear();
    pfrm.screen().display();

    if (not challenges_) {
        return;
    }

    int index = 0;
    int start_index = page_ * 5;

    lisp::foreach (*challenges_, [&](lisp::Value* val) {
        if (val->type_ not_eq lisp::Value::Type::cons) {
            pfrm.fatal("challenge list format invalid");
        }

        auto name = val->cons_.car();
        if (name->type_ not_eq lisp::Value::Type::string) {
            pfrm.fatal("challenge list format invalid");
        }

        if (index++ < start_index) {
            return;
        }

        text_.emplace_back(pfrm,
                           name->string_.value(),
                           OverlayCoord{4, u8(4 + text_.size() * 2)});
    });


    if (page_count_ > 1) {
        int margin = (calc_screen_tiles(pfrm).x - page_count_ * 2) / 2;
        for (int i = 0; i < page_count_; ++i) {
            if (i == page_) {
                pfrm.set_tile(Layer::overlay, margin + i * 2, 18, 83);
            } else {
                pfrm.set_tile(Layer::overlay, margin + i * 2, 18, 82);
            }
        }
    }
}



void prep_level(Platform& pfrm, App& app);



void SelectChallengeScene::exit(Platform& pfrm, App&, Scene& next)
{
    text_.clear();
    pfrm.fill_overlay(0);
    pfrm.load_overlay_texture("overlay");

    pfrm.enable_feature("v-parallax", true);
}



void SelectChallengeScene::display(Platform& pfrm, App& app)
{
    if (state_ not_eq State::idle) {
        return;
    }
    Sprite cursor;
    cursor.set_size(Sprite::Size::w16_h32);
    cursor.set_texture_index(59);

    Vec2<Float> origin;

    auto ambient_movement = 2 * float(sine(4 * 3.14f * 0.004f * timer_ + 180)) /
                            std::numeric_limits<s16>::max();

    origin.x += 16 + ambient_movement;
    origin.y += 32 + cursor_ * 16 - 1;

    cursor.set_position(origin);

    pfrm.screen().draw(cursor);
}



ScenePtr<Scene>
SelectChallengeScene::update(Platform& pfrm, App& app, Microseconds delta)
{
    if (exit_) {
        return scene_pool::alloc<TitleScreenScene>();
    }

    timer_ += delta;

    switch (state_) {
    case State::fade_in:
        break;

    case State::idle: {
        if (not challenges_) {
            return null_scene();
        }

        if (key_down<Key::down>(pfrm)) {
            if ((u32)cursor_ < text_.size() - 1) {
                cursor_++;
            }
        }

        if (key_down<Key::up>(pfrm)) {
            if (cursor_) {
                cursor_--;
            }
        }

        if (key_down<Key::right>(pfrm)) {
            if (page_ < page_count_ - 1) {
                ++page_;
                show_options(pfrm);
                if ((u32)cursor_ >= text_.size()) {
                    cursor_ = text_.size() - 1;
                }
            }
        }

        if (key_down<Key::left>(pfrm)) {
            if (page_ > 0) {
                --page_;
                show_options(pfrm);
                if ((u32)cursor_ >= text_.size()) {
                    cursor_ = text_.size() - 1;
                }
            }
        }

        if (key_down<Key::action_1>(pfrm)) {
            state_ = State::fade_out;
            timer_ = 0;
            text_.clear();
        } else if (key_down<Key::action_2>(pfrm)) {
            text_.clear();
            pfrm.fill_overlay(0);
            exit_ = true;
        }
        break;
    }

    case State::fade_out: {
        constexpr auto fade_duration = milliseconds(800);
        if (timer_ > fade_duration) {
            app.camera().reset();
            pfrm.screen().fade(1.f, ColorConstant::rich_black, {}, true, true);
            auto index = page_ * 5 + cursor_;
            auto choice = lisp::get_list(*challenges_, index);

            auto file_name = choice->cons_.cdr();
            if (file_name->type_ not_eq lisp::Value::Type::string) {
                pfrm.fatal("challenge list format invalid");
            }

            if (auto script = pfrm.load_file_contents("scripts",
                                                      file_name->string_.value())) {
                lisp::dostring(script, [&pfrm](lisp::Value& err) {
                    lisp::DefaultPrinter p;
                    lisp::format(&err, p);
                    pfrm.fatal(p.fmt_.c_str());
                });
                prep_level(pfrm, app);
                app.player_island().repaint(pfrm);

                return scene_pool::alloc<FadeInScene>();
            } else {
                StringBuffer<32> err("file ");
                err += file_name->string_.value();
                err += " missing";
                pfrm.fatal(err.c_str());
            }

        } else {
            const auto amount =
                default_fade +
                (1.f - default_fade) * smoothstep(0.f, fade_duration, timer_);
            pfrm.screen().fade(amount);
        }
        break;
    }
    }

    app.update_parallax(delta);

    return null_scene();
}



} // namespace skyland
