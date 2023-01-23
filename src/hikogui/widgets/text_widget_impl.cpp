// Copyright Take Vos 2021-2022.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

#include "text_widget.hpp"
#include "../os_settings.hpp"
#include "../scoped_task.hpp"
#include "../when_any.hpp"
#include "../unicode/unicode_bidi.hpp"

namespace hi::inline v1 {

text_widget::text_widget(widget *parent, std::shared_ptr<delegate_type> delegate) noexcept :
    super(parent), delegate(std::move(delegate))
{
    mode = widget_mode::select;

    hi_assert_not_null(this->delegate);
    _delegate_cbt = this->delegate->subscribe([&] {
        // On every text edit, immediately/synchronously update the shaped text.
        // This is needed for handling multiple edit commands before the next frame update.
        if (_layout) {
            auto new_layout = _layout;
            hilet old_constraints = _constraints_cache;

            // Constrain and layout according to the old layout.
            hilet new_constraints = update_constraints();
            new_layout.shape.rectangle = aarectanglei{
                new_layout.shape.x(),
                new_layout.shape.y(),
                std::max(new_layout.shape.width(), new_constraints.minimum.width()),
                std::max(new_layout.shape.height(), new_constraints.minimum.height())};
            set_layout(new_layout);

            if (new_constraints != old_constraints) {
                // The constraints have changed, properly constrain and layout on the next frame.
                ++global_counter<"text_widget:delegate:constrain">;
                request_scroll();
                process_event({gui_event_type::window_reconstrain});
            }
        } else {
            // The layout is incomplete, properly constrain and layout on the next frame.
            ++global_counter<"text_widget:delegate:constrain">;
            request_scroll();
            process_event({gui_event_type::window_reconstrain});
        }
    });

    _text_style_cbt = text_style.subscribe([&](auto...) {
        ++global_counter<"text_widget:text_style:constrain">;
        request_scroll();
        process_event({gui_event_type::window_reconstrain});
    });

    _cursor_state_cbt = _cursor_state.subscribe([&](auto...) {
        ++global_counter<"text_widget:cursor_state:redraw">;
        request_redraw();
    });

    // If the text_widget is used as a label the blink_cursor() co-routine
    // is only waiting on `model` and `focus`, so this is cheap.
    _blink_cursor = blink_cursor();

    this->delegate->init(*this);
}

text_widget::~text_widget()
{
    hi_assert_not_null(delegate);
    delegate->deinit(*this);
}

[[nodiscard]] box_constraints text_widget::update_constraints() noexcept
{
    _layout = {};

    // Read the latest text from the delegate.
    hi_assert_not_null(delegate);
    _text_cache = delegate->read(*this);

    // Make sure that the current selection fits the new text.
    _selection.resize(_text_cache.size());

    hilet actual_text_style = theme().text_style(*text_style);

    // Create a new text_shaper with the new text.
    auto alignment_ = os_settings::left_to_right() ? *alignment : mirror(*alignment);

    _shaped_text = text_shaper{
        font_book::global(), _text_cache, actual_text_style, theme().scale, alignment_, os_settings::writing_direction()};

    hilet shaped_text_rectangle =
        narrow_cast<aarectanglei>(ceil(_shaped_text.bounding_rectangle(std::numeric_limits<float>::infinity())));
    hilet shaped_text_size = shaped_text_rectangle.size();

    if (*mode == widget_mode::partial) {
        // In line-edit mode the text should not wrap.
        return _constraints_cache = {
                   shaped_text_size, shaped_text_size, shaped_text_size, _shaped_text.resolved_alignment(), theme().margin()};

    } else {
        // Allow the text to be 550.0f pixels wide.
        hilet preferred_shaped_text_rectangle = narrow_cast<aarectanglei>(ceil(_shaped_text.bounding_rectangle(550.0f)));
        hilet preferred_shaped_text_size = preferred_shaped_text_rectangle.size();

        hilet height = std::max(shaped_text_size.height(), preferred_shaped_text_size.height());
        return _constraints_cache = {
                   extent2i{preferred_shaped_text_size.width(), height},
                   extent2i{preferred_shaped_text_size.width(), height},
                   extent2i{shaped_text_size.width(), height},
                   _shaped_text.resolved_alignment(),
                   theme().margin()};
    }
}

void text_widget::set_layout(widget_layout const& context) noexcept
{
    if (compare_store(_layout, context)) {
        hi_assert(context.shape.baseline);

        _shaped_text.layout(
            narrow_cast<aarectangle>(context.rectangle()), narrow_cast<float>(*context.shape.baseline), context.sub_pixel_size);
    }
}

void text_widget::scroll_to_show_selection() noexcept
{
    if (*mode > widget_mode::invisible and *focus) {
        hilet cursor = _selection.cursor();
        hilet char_it = _shaped_text.begin() + cursor.index();
        if (char_it < _shaped_text.end()) {
            scroll_to_show(narrow_cast<aarectanglei>(char_it->rectangle));
        }
    }
}

void text_widget::request_scroll() noexcept
{
    // At a minimum we need to request a redraw so that
    // `scroll_to_show_selection()` is called on the next frame.
    _request_scroll = true;
    ++global_counter<"text_widget:request_scroll:redraw">;
    request_redraw();
}

scoped_task<> text_widget::blink_cursor() noexcept
{
    while (true) {
        if (*mode >= widget_mode::partial and *focus) {
            switch (*_cursor_state) {
            case cursor_state_type::busy:
                _cursor_state = cursor_state_type::on;
                co_await when_any(os_settings::cursor_blink_delay(), mode, focus);
                break;

            case cursor_state_type::on:
                _cursor_state = cursor_state_type::off;
                co_await when_any(os_settings::cursor_blink_interval() / 2, mode, focus);
                break;

            case cursor_state_type::off:
                _cursor_state = cursor_state_type::on;
                co_await when_any(os_settings::cursor_blink_interval() / 2, mode, focus);
                break;

            default:
                _cursor_state = cursor_state_type::busy;
            }

        } else {
            _cursor_state = cursor_state_type::none;
            co_await when_any(mode, focus);
        }
    }
}

void text_widget::draw(draw_context const& context) noexcept
{
    using namespace std::literals::chrono_literals;

    // After potential reconstrain and relayout, updating the shaped-text, ask the parent window to scroll if needed.
    if (std::exchange(_request_scroll, false)) {
        scroll_to_show_selection();
    }

    if (_last_drag_mouse_event) {
        if (_last_drag_mouse_event_next_repeat == utc_nanoseconds{}) {
            _last_drag_mouse_event_next_repeat = context.display_time_point + os_settings::keyboard_repeat_delay();

        } else if (context.display_time_point >= _last_drag_mouse_event_next_repeat) {
            _last_drag_mouse_event_next_repeat = context.display_time_point + os_settings::keyboard_repeat_interval();

            // The last drag mouse event was stored in window coordinate to compensate for scrolling, translate it
            // back to local coordinates before handling the mouse event again.
            auto new_mouse_event = _last_drag_mouse_event;
            new_mouse_event.mouse().position = _layout.from_window * _last_drag_mouse_event.mouse().position;

            // When mouse is dragging a selection, start continues redraw and scroll parent views to display the selection.
            text_widget::handle_event(new_mouse_event);
        }
        scroll_to_show_selection();
        ++global_counter<"text_widget:mouse_drag:redraw">;
        request_redraw();
    }

    if (*mode > widget_mode::invisible and overlaps(context, layout())) {
        context.draw_text(layout(), _shaped_text);

        context.draw_text_selection(layout(), _shaped_text, _selection, theme().color(semantic_color::text_select));

        if (*_cursor_state == cursor_state_type::on or *_cursor_state == cursor_state_type::busy) {
            context.draw_text_cursors(
                layout(),
                _shaped_text,
                _selection.cursor(),
                _overwrite_mode,
                to_bool(_has_dead_character),
                theme().color(semantic_color::primary_cursor),
                theme().color(semantic_color::secondary_cursor));
        }
    }
}

void text_widget::undo_push() noexcept
{
    _undo_stack.emplace(_text_cache, _selection);
}

void text_widget::undo() noexcept
{
    if (_undo_stack.can_undo()) {
        hilet & [ text, selection ] = _undo_stack.undo(_text_cache, _selection);

        delegate->write(*this, text);
        _selection = selection;
    }
}

void text_widget::redo() noexcept
{
    if (_undo_stack.can_redo()) {
        hilet & [ text, selection ] = _undo_stack.redo();

        delegate->write(*this, text);
        _selection = selection;
    }
}

[[nodiscard]] gstring_view text_widget::selected_text() const noexcept
{
    hilet[first, last] = _selection.selection_indices();

    return gstring_view{_text_cache}.substr(first, last - first);
}

void text_widget::fix_cursor_position() noexcept
{
    hilet size = _text_cache.size();
    if (_overwrite_mode and _selection.empty() and _selection.cursor().after()) {
        _selection = _selection.cursor().before_neighbor(size);
    }
    _selection.resize(size);
}

void text_widget::replace_selection(gstring const& replacement) noexcept
{
    undo_push();

    hilet[first, last] = _selection.selection_indices();

    auto text = _text_cache;
    text.replace(first, last - first, replacement);
    delegate->write(*this, text);

    _selection = text_cursor{first + replacement.size() - 1, true};
    fix_cursor_position();
}

void text_widget::add_character(grapheme c, add_type keyboard_mode) noexcept
{
    hilet original_cursor = _selection.cursor();
    auto original_grapheme = grapheme{char32_t{0xffff}};

    if (_selection.empty() and _overwrite_mode and original_cursor.before()) {
        original_grapheme = _text_cache[original_cursor.index()];

        hilet[first, last] = _shaped_text.select_char(original_cursor);
        _selection.drag_selection(last);
    }
    replace_selection(gstring{c});

    if (keyboard_mode == add_type::insert) {
        // The character was inserted, put the cursor back where it was.
        _selection = original_cursor;

    } else if (keyboard_mode == add_type::dead) {
        _selection = original_cursor.before_neighbor(_text_cache.size());
        _has_dead_character = original_grapheme;
    }
}

void text_widget::delete_dead_character() noexcept
{
    if (_has_dead_character) {
        hi_assert(_selection.cursor().before());
        hi_assert_bounds(_selection.cursor().index(), _text_cache);
        if (_has_dead_character.valid()) {
            auto text = _text_cache;
            text[_selection.cursor().index()] = _has_dead_character;
            delegate->write(*this, text);
        } else {
            auto text = _text_cache;
            text.erase(_selection.cursor().index(), 1);
            delegate->write(*this, text);
        }
    }
    _has_dead_character.clear();
}

void text_widget::delete_character_next() noexcept
{
    if (_selection.empty()) {
        auto cursor = _selection.cursor();
        cursor = cursor.before_neighbor(_shaped_text.size());

        hilet[first, last] = _shaped_text.select_char(cursor);
        _selection.drag_selection(last);
    }

    return replace_selection(gstring{});
}

void text_widget::delete_character_prev() noexcept
{
    if (_selection.empty()) {
        auto cursor = _selection.cursor();
        cursor = cursor.after_neighbor(_shaped_text.size());

        hilet[first, last] = _shaped_text.select_char(cursor);
        _selection.drag_selection(first);
    }

    return replace_selection(gstring{});
}

void text_widget::delete_word_next() noexcept
{
    if (_selection.empty()) {
        auto cursor = _selection.cursor();
        cursor = cursor.before_neighbor(_shaped_text.size());

        hilet[first, last] = _shaped_text.select_word(cursor);
        _selection.drag_selection(last);
    }

    return replace_selection(gstring{});
}

void text_widget::delete_word_prev() noexcept
{
    if (_selection.empty()) {
        auto cursor = _selection.cursor();
        cursor = cursor.after_neighbor(_shaped_text.size());

        hilet[first, last] = _shaped_text.select_word(cursor);
        _selection.drag_selection(first);
    }

    return replace_selection(gstring{});
}

void text_widget::reset_state(char const *states) noexcept
{
    while (*states != 0) {
        switch (*states) {
        case 'D':
            delete_dead_character();
            break;
        case 'X':
            _vertical_movement_x = std::numeric_limits<float>::quiet_NaN();
            break;
        case 'B':
            if (*_cursor_state == cursor_state_type::on or *_cursor_state == cursor_state_type::off) {
                _cursor_state = cursor_state_type::busy;
            }
            break;
        default:
            hi_no_default();
        }
        ++states;
    }
}

bool text_widget::handle_event(gui_event const& event) noexcept
{
    hi_axiom(loop::main().on_thread());

    switch (event.type()) {
        using enum gui_event_type;
        using enum widget_mode;

    case gui_widget_next:
    case gui_widget_prev:
    case keyboard_exit:
        // When the next widget is selected due to pressing the Tab key the text should be committed.
        // The `text_widget` does not handle gui_activate, so it will be forwarded to parent widgets,
        // such as `text_field_widget` which does.
        process_event(gui_event_type::gui_activate);
        return super::handle_event(event);

    case keyboard_grapheme:
        if (*mode >= partial) {
            reset_state("BDX");
            add_character(event.grapheme(), add_type::append);
            return true;
        }
        break;

    case keyboard_partial_grapheme:
        if (*mode >= partial) {
            reset_state("BDX");
            add_character(event.grapheme(), add_type::dead);
            return true;
        }
        break;

    case text_mode_insert:
        if (*mode >= partial) {
            reset_state("BDX");
            _overwrite_mode = not _overwrite_mode;
            fix_cursor_position();
            return true;
        }
        break;

    case text_edit_paste:
        if (*mode >= partial) {
            reset_state("BDX");
            replace_selection(to_gstring(event.clipboard_data(), U' '));
            return true;

        } else if (*mode >= enabled) {
            reset_state("BDX");
            replace_selection(to_gstring(event.clipboard_data()));
            return true;
        }
        break;

    case text_edit_copy:
        if (*mode >= select) {
            reset_state("BDX");
            if (hilet selected_text_ = selected_text(); not selected_text_.empty()) {
                process_event(gui_event::make_clipboard_event(gui_event_type::window_set_clipboard, to_string(selected_text_)));
            }
            return true;
        }
        break;

    case text_edit_cut:
        if (*mode >= select) {
            reset_state("BDX");
            process_event(gui_event::make_clipboard_event(gui_event_type::window_set_clipboard, to_string(selected_text())));
            if (*mode >= partial) {
                replace_selection(gstring{});
            }
            return true;
        }
        break;

    case text_undo:
        if (*mode >= partial) {
            reset_state("BDX");
            undo();
            return true;
        }
        break;

    case text_redo:
        if (*mode >= partial) {
            reset_state("BDX");
            redo();
            return true;
        }
        break;

    case text_insert_line:
        if (*mode >= enabled) {
            reset_state("BDX");
            add_character(grapheme{unicode_PS}, add_type::append);
            return true;
        }
        break;

    case text_insert_line_up:
        if (*mode >= enabled) {
            reset_state("BDX");
            _selection = _shaped_text.move_begin_paragraph(_selection.cursor());
            add_character(grapheme{unicode_PS}, add_type::insert);
            return true;
        }
        break;

    case text_insert_line_down:
        if (*mode >= enabled) {
            reset_state("BDX");
            _selection = _shaped_text.move_end_paragraph(_selection.cursor());
            add_character(grapheme{unicode_PS}, add_type::insert);
            return true;
        }
        break;

    case text_delete_char_next:
        if (*mode >= partial) {
            reset_state("BDX");
            delete_character_next();
            return true;
        }
        break;

    case text_delete_char_prev:
        if (*mode >= partial) {
            reset_state("BDX");
            delete_character_prev();
            return true;
        }
        break;

    case text_delete_word_next:
        if (*mode >= partial) {
            reset_state("BDX");
            delete_word_next();
            return true;
        }
        break;

    case text_delete_word_prev:
        if (*mode >= partial) {
            reset_state("BDX");
            delete_word_prev();
            return true;
        }
        break;

    case text_cursor_left_char:
        if (*mode >= partial) {
            reset_state("BDX");
            _selection = _shaped_text.move_left_char(_selection.cursor(), _overwrite_mode);
            request_scroll();
            return true;
        }
        break;

    case text_cursor_right_char:
        if (*mode >= partial) {
            reset_state("BDX");
            _selection = _shaped_text.move_right_char(_selection.cursor(), _overwrite_mode);
            request_scroll();
            return true;
        }
        break;

    case text_cursor_down_char:
        if (*mode >= partial) {
            reset_state("BD");
            _selection = _shaped_text.move_down_char(_selection.cursor(), _vertical_movement_x);
            request_scroll();
            return true;
        }
        break;

    case text_cursor_up_char:
        if (*mode >= partial) {
            reset_state("BD");
            _selection = _shaped_text.move_up_char(_selection.cursor(), _vertical_movement_x);
            request_scroll();
            return true;
        }
        break;

    case text_cursor_left_word:
        if (*mode >= partial) {
            reset_state("BDX");
            _selection = _shaped_text.move_left_word(_selection.cursor(), _overwrite_mode);
            request_scroll();
            return true;
        }
        break;

    case text_cursor_right_word:
        if (*mode >= partial) {
            reset_state("BDX");
            _selection = _shaped_text.move_right_word(_selection.cursor(), _overwrite_mode);
            request_scroll();
            return true;
        }
        break;

    case text_cursor_begin_line:
        if (*mode >= partial) {
            reset_state("BDX");
            _selection = _shaped_text.move_begin_line(_selection.cursor());
            request_scroll();
            return true;
        }
        break;

    case text_cursor_end_line:
        if (*mode >= partial) {
            reset_state("BDX");
            _selection = _shaped_text.move_end_line(_selection.cursor());
            request_scroll();
            return true;
        }
        break;

    case text_cursor_begin_sentence:
        if (*mode >= partial) {
            reset_state("BDX");
            _selection = _shaped_text.move_begin_sentence(_selection.cursor());
            request_scroll();
            return true;
        }
        break;

    case text_cursor_end_sentence:
        if (*mode >= partial) {
            reset_state("BDX");
            _selection = _shaped_text.move_end_sentence(_selection.cursor());
            request_scroll();
            return true;
        }
        break;

    case text_cursor_begin_document:
        if (*mode >= partial) {
            reset_state("BDX");
            _selection = _shaped_text.move_begin_document(_selection.cursor());
            request_scroll();
            return true;
        }
        break;

    case text_cursor_end_document:
        if (*mode >= partial) {
            reset_state("BDX");
            _selection = _shaped_text.move_end_document(_selection.cursor());
            request_scroll();
            return true;
        }
        break;

    case gui_cancel:
        if (*mode >= select) {
            reset_state("BDX");
            _selection.clear_selection(_shaped_text.size());
            return true;
        }
        break;

    case text_select_left_char:
        if (*mode >= partial) {
            reset_state("BDX");
            _selection.drag_selection(_shaped_text.move_left_char(_selection.cursor(), false));
            request_scroll();
            return true;
        }
        break;

    case text_select_right_char:
        if (*mode >= partial) {
            reset_state("BDX");
            _selection.drag_selection(_shaped_text.move_right_char(_selection.cursor(), false));
            request_scroll();
            return true;
        }
        break;

    case text_select_down_char:
        if (*mode >= partial) {
            reset_state("BD");
            _selection.drag_selection(_shaped_text.move_down_char(_selection.cursor(), _vertical_movement_x));
            request_scroll();
            return true;
        }
        break;

    case text_select_up_char:
        if (*mode >= partial) {
            reset_state("BD");
            _selection.drag_selection(_shaped_text.move_up_char(_selection.cursor(), _vertical_movement_x));
            request_scroll();
            return true;
        }
        break;

    case text_select_left_word:
        if (*mode >= partial) {
            reset_state("BDX");
            _selection.drag_selection(_shaped_text.move_left_word(_selection.cursor(), false));
            request_scroll();
            return true;
        }
        break;

    case text_select_right_word:
        if (*mode >= partial) {
            reset_state("BDX");
            _selection.drag_selection(_shaped_text.move_right_word(_selection.cursor(), false));
            request_scroll();
            return true;
        }
        break;

    case text_select_begin_line:
        if (*mode >= partial) {
            reset_state("BDX");
            _selection.drag_selection(_shaped_text.move_begin_line(_selection.cursor()));
            request_scroll();
            return true;
        }
        break;

    case text_select_end_line:
        if (*mode >= partial) {
            reset_state("BDX");
            _selection.drag_selection(_shaped_text.move_end_line(_selection.cursor()));
            request_scroll();
            return true;
        }
        break;

    case text_select_begin_sentence:
        if (*mode >= partial) {
            reset_state("BDX");
            _selection.drag_selection(_shaped_text.move_begin_sentence(_selection.cursor()));
            request_scroll();
            return true;
        }
        break;

    case text_select_end_sentence:
        if (*mode >= partial) {
            reset_state("BDX");
            _selection.drag_selection(_shaped_text.move_end_sentence(_selection.cursor()));
            request_scroll();
            return true;
        }
        break;

    case text_select_begin_document:
        if (*mode >= partial) {
            reset_state("BDX");
            _selection.drag_selection(_shaped_text.move_begin_document(_selection.cursor()));
            request_scroll();
            return true;
        }
        break;

    case text_select_end_document:
        if (*mode >= partial) {
            reset_state("BDX");
            _selection.drag_selection(_shaped_text.move_end_document(_selection.cursor()));
            request_scroll();
            return true;
        }
        break;

    case text_select_document:
        if (*mode >= partial) {
            reset_state("BDX");
            _selection = _shaped_text.move_begin_document(_selection.cursor());
            _selection.drag_selection(_shaped_text.move_end_document(_selection.cursor()));
            request_scroll();
            return true;
        }
        break;

    case mouse_up:
        if (*mode >= select) {
            // Stop the continues redrawing during dragging.
            // Also reset the time, so on drag-start it will initialize the time, which will
            // cause a smooth startup of repeating.
            _last_drag_mouse_event = {};
            _last_drag_mouse_event_next_repeat = {};
            return true;
        }
        break;

    case mouse_down:
        if (*mode >= select) {
            hilet cursor = _shaped_text.get_nearest_cursor(narrow_cast<point2>(event.mouse().position));
            switch (event.mouse().click_count) {
            case 1:
                reset_state("BDX");
                _selection = cursor;
                break;
            case 2:
                reset_state("BDX");
                _selection.start_selection(cursor, _shaped_text.select_word(cursor));
                break;
            case 3:
                reset_state("BDX");
                _selection.start_selection(cursor, _shaped_text.select_sentence(cursor));
                break;
            case 4:
                reset_state("BDX");
                _selection.start_selection(cursor, _shaped_text.select_paragraph(cursor));
                break;
            case 5:
                reset_state("BDX");
                _selection.start_selection(cursor, _shaped_text.select_document(cursor));
                break;
            default:;
            }

            ++global_counter<"text_widget:mouse_down:relayout">;
            process_event({gui_event_type::window_relayout});
            request_scroll();
            return true;
        }
        break;

    case mouse_drag:
        if (*mode >= select) {
            hilet cursor = _shaped_text.get_nearest_cursor(narrow_cast<point2>(event.mouse().position));
            switch (event.mouse().click_count) {
            case 1:
                reset_state("BDX");
                _selection.drag_selection(cursor);
                break;
            case 2:
                reset_state("BDX");
                _selection.drag_selection(cursor, _shaped_text.select_word(cursor));
                break;
            case 3:
                reset_state("BDX");
                _selection.drag_selection(cursor, _shaped_text.select_sentence(cursor));
                break;
            case 4:
                reset_state("BDX");
                _selection.drag_selection(cursor, _shaped_text.select_paragraph(cursor));
                break;
            default:;
            }

            // Drag events must be repeated, so that dragging is continues when it causes scrolling.
            // Normally mouse positions are kept in the local coordinate system, but scrolling
            // causes this coordinate system to shift, so translate it to the window coordinate system here.
            _last_drag_mouse_event = event;
            _last_drag_mouse_event.mouse().position = _layout.to_window * event.mouse().position;
            ++global_counter<"text_widget:mouse_drag:redraw">;
            request_redraw();
            return true;
        }
        break;

    default:;
    }

    return super::handle_event(event);
}

hitbox text_widget::hitbox_test(point2i position) const noexcept
{
    hi_axiom(loop::main().on_thread());

    if (layout().contains(position)) {
        if (*mode >= widget_mode::partial) {
            return hitbox{id, _layout.elevation, hitbox_type::text_edit};

        } else if (*mode >= widget_mode::select) {
            return hitbox{id, _layout.elevation, hitbox_type::_default};

        } else {
            return hitbox{};
        }
    } else {
        return hitbox{};
    }
}

[[nodiscard]] bool text_widget::accepts_keyboard_focus(keyboard_focus_group group) const noexcept
{
    if (*mode >= widget_mode::partial) {
        return to_bool(group & keyboard_focus_group::normal);
    } else if (*mode >= widget_mode::select) {
        return to_bool(group & keyboard_focus_group::mouse);
    } else {
        return false;
    }
}

} // namespace hi::inline v1