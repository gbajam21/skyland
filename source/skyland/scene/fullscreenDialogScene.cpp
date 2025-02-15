#include "fullscreenDialogScene.hpp"
#include "fadeInScene.hpp"
#include "graphics/overlay.hpp"
#include "localization.hpp"
#include "skyland/scene_pool.hpp"



Platform::TextureCpMapper locale_texture_map();



namespace skyland {



const int y_start = 9;



bool FullscreenDialogScene::advance_text(Platform& pfrm,
                                         App& app,
                                         Microseconds delta,
                                         bool sfx)
{
    const auto delay = milliseconds(80);

    text_state_.timer_ += delta;

    const auto st = calc_screen_tiles(pfrm);

    if (text_state_.timer_ > delay) {
        text_state_.timer_ = 0;

        if (sfx) {
            pfrm.speaker().play_sound("msg", 5);
        }

        if (text_state_.current_word_remaining_ == 0) {
            while (*text_state_.current_word_ == ' ') {
                text_state_.current_word_++;
                if (text_state_.pos_ < st.x - 2) {
                    text_state_.pos_ += 1;
                }
            }
            bool done = false;
            utf8::scan(
                [&](const utf8::Codepoint& cp, const char*, int) {
                    if (done) {
                        return;
                    }
                    if (cp == ' ') {
                        done = true;
                    } else {
                        text_state_.current_word_remaining_++;
                    }
                },
                text_state_.current_word_,
                str_len(text_state_.current_word_));
        }

        // At this point, we know the length of the next space-delimited word in
        // the string. Now we can print stuff...

        const auto st = calc_screen_tiles(pfrm);
        static const auto margin_sum = 4;
        const auto text_box_width = st.x - margin_sum;
        const auto remaining = (text_box_width - text_state_.pos_) -
                               (text_state_.line_ == 0 ? 0 : 2);

        if (remaining < text_state_.current_word_remaining_) {
            if (text_state_.line_ == 0) {
                text_state_.line_++;
                text_state_.pos_ = 0;
                return true;
            } else {
                return false;
            }
        }

        int bytes_consumed = 0;
        const auto cp = utf8::getc(text_state_.current_word_, &bytes_consumed);

        const auto mapping_info = locale_texture_map()(cp);

        u16 t = 495; // bad glyph, FIXME: add a constant

        if (mapping_info) {
            t = pfrm.map_glyph(cp, *mapping_info);
        }

        const int y_offset = text_state_.line_ == 0 ? 4 + y_start : 2 + y_start;
        const int x_offset = text_state_.pos_ + 2;

        pfrm.set_tile(Layer::overlay, x_offset, st.y - (y_offset), t);

        text_state_.current_word_remaining_--;
        text_state_.current_word_ += bytes_consumed;
        text_state_.pos_++;

        if (*text_state_.current_word_ == '\0') {
            display_mode_ = DisplayMode::key_released_check2;
        }
    }

    return true;
}



void FullscreenDialogScene::clear_textbox(Platform& pfrm)
{
    const auto st = calc_screen_tiles(pfrm);

    for (int x = 1; x < st.x - 1; ++x) {
        pfrm.set_tile(Layer::overlay, x, st.y - (4 + y_start), 82);
        pfrm.set_tile(Layer::overlay, x, st.y - (3 + y_start), 82);
        pfrm.set_tile(Layer::overlay, x, st.y - (2 + y_start), 82);
    }

    text_state_.line_ = 0;
    text_state_.pos_ = 0;
}



void FullscreenDialogScene::enter(Platform& pfrm, App& app, Scene& prev)
{
    pfrm.load_overlay_texture("overlay_dialog");

    pfrm.screen().fade(1.f, custom_color(0), {}, true, false);
    pfrm.screen().fade(1.f, ColorConstant::rich_black, {}, true, false);

    clear_textbox(pfrm);

    text_state_.current_word_remaining_ = 0;
    text_state_.current_word_ = buffer_->c_str();
    text_state_.timer_ = 0;
    text_state_.line_ = 0;
    text_state_.pos_ = 0;
}



void FullscreenDialogScene::exit(Platform& pfrm, App& app, Scene& prev)
{
    pfrm.fill_overlay(0);

    pfrm.load_overlay_texture("overlay");
}



ScenePtr<Scene>
FullscreenDialogScene::update(Platform& pfrm, App& app, Microseconds delta)
{
    auto animate_moretext_icon = [&] {
        static const auto duration = milliseconds(500);
        text_state_.timer_ += delta;
        if (text_state_.timer_ > duration) {
            text_state_.timer_ = 0;
            const auto st = calc_screen_tiles(pfrm);
            if (pfrm.get_tile(Layer::overlay, st.x - 3, st.y - (2 + y_start)) ==
                91) {
                pfrm.set_tile(
                    Layer::overlay, st.x - 3, st.y - (2 + y_start), 92);
            } else {
                pfrm.set_tile(
                    Layer::overlay, st.x - 3, st.y - (2 + y_start), 91);
            }
        }
    };

    switch (display_mode_) {
    case DisplayMode::animate_in:
        display_mode_ = DisplayMode::busy;
        break;

    case DisplayMode::busy: {

        const bool text_busy = advance_text(pfrm, app, delta, true);

        if (not text_busy) {
            display_mode_ = DisplayMode::key_released_check1;
        } else {
            if (key_down<Key::action_2>(pfrm) or
                key_down<Key::action_1>(pfrm)) {

                while (advance_text(pfrm, app, delta, false)) {
                    if (display_mode_ not_eq DisplayMode::busy) {
                        break;
                    }
                }

                if (display_mode_ == DisplayMode::busy) {
                    display_mode_ = DisplayMode::key_released_check1;
                }
            }
        }
    } break;

    case DisplayMode::wait: {
        animate_moretext_icon();

        if (key_down<Key::action_2>(pfrm) or key_down<Key::action_1>(pfrm)) {

            text_state_.timer_ = 0;

            clear_textbox(pfrm);
            display_mode_ = DisplayMode::busy;
        }
        break;
    }

    case DisplayMode::key_released_check1:
        // if (key_down<Key::action_2>(pfrm) or
        //     key_down<Key::action_1>(pfrm)) {

        text_state_.timer_ = seconds(1);
        display_mode_ = DisplayMode::wait;
        // }
        break;

    case DisplayMode::key_released_check2:
        // if (key_down<Key::action_2>(pfrm) or
        //     key_down<Key::action_1>(pfrm)) {

        text_state_.timer_ = seconds(1);
        display_mode_ = DisplayMode::done;
        // }
        break;

    case DisplayMode::done:
        animate_moretext_icon();
        if (key_down<Key::action_2>(pfrm) or key_down<Key::action_1>(pfrm)) {

            // if (text_[1] not_eq LocaleString::empty) {
            //     ++text_;
            //     init_text(pfrm, *text_);
            //     display_mode_ = DisplayMode::animate_in;
            // } else {
            display_mode_ = DisplayMode::animate_out;
            // }
        }
        break;

    case DisplayMode::animate_out:
        display_mode_ = DisplayMode::clear;
        pfrm.fill_overlay(0);
        break;

    case DisplayMode::clear:
        return scene_pool::alloc<FadeInScene>();
    }

    return null_scene();
}



} // namespace skyland
