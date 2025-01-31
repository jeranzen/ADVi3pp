/**
 * ADVi3++ Firmware For Wanhao Duplicator i3 Plus (based on Marlin 2)
 *
 * Copyright (C) 2017-2022 Sebastien Andrivet [https://github.com/andrivet/]
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

// Minimal implementation of MarlinUI for ADVi3++

#include "../parameters.h"
#include "../core/logging.h"
#include "../core/status.h"
#include "../core/buzzer.h"
#include "../screens/print/pause.h"
#include "../screens/core/wait.h"

#include "../../module/printcounter.h"
#include "../../MarlinCore.h"
#include "../../sd/cardreader.h"
#include "../../gcode/queue.h"
#include "../../module/planner.h"
#include "../../feature/host_actions.h"

using namespace ADVi3pp;

preheat_t MarlinUI::material_preset[PREHEAT_COUNT];  // Initialized by settings.load()

extern PauseMode pause_mode;

namespace
{
    bool external_control = false;
    uint8_t progress = 0;
    uint8_t alert_level = 0;
    int16_t brightnes = DEFAULT_LCD_BRIGHTNESS;
}

void MarlinUI::pause_show_message(const PauseMessage message, const PauseMode mode, const uint8_t /*extruder*/)
{
    Log::log() << F("lcd_pause_show_message(")
        << static_cast<uint16_t>(message)
        << F(", ")
        << static_cast<uint16_t>(mode)
        << F(")")
        << Log::endl();
    if (mode != PAUSE_MODE_SAME) pause_mode = mode;
    pause.show_message(message);
}

void MarlinUI::return_to_status(bool show_main)
{
    Log::log() << F("return_to_status") << Log::endl();
    pages.reset();
    if(!show_main && print_job_timer.isRunning())
        pages.show(Page::Print);
    else
        pages.show(Page::Main);
}

void MarlinUI::reset_alert_level()
{
    Log::log() << F("reset_alert_level") << Log::endl();
    alert_level = 0;
}

void MarlinUI::finish_status(const bool /*persist*/)
{
    // Nothing to do
}

bool MarlinUI::has_status()
{
    Log::log() << F("has_status") << Log::endl();
    return status.has();
}

void MarlinUI::set_status(const char* const message, const bool persist)
{
    if(alert_level) return;
    status.set(message);
    finish_status(persist);
}

void MarlinUI::status_printf(const uint8_t level, FSTR_P const fmt, ...)
{
    if (level < alert_level) return;
    alert_level = level;

    va_list args;
    va_start(args, fmt);
    status.set(fmt, args);
    va_end(args);

    finish_status(level > 0);
}

void MarlinUI::set_status(FSTR_P const fstr, int8_t level)
{
    if (level < 0) level = alert_level = 0;
    if (level < alert_level) return;
    alert_level = level;

    status.set(fstr);
    finish_status(level > 0);
}

void MarlinUI::set_alert_status(FSTR_P const fstr)
{
    set_status(fstr, 1);
    return_to_status();
}

void MarlinUI::reset_status(const bool no_welcome)
{
    FSTR_P msg;
    if(printingIsPaused())
        msg = GET_TEXT_F(MSG_PRINT_PAUSED);
    else if (IS_SD_PRINTING())
        msg = GET_TEXT_F(MSG_PRINTING);
    else if (print_job_timer.isRunning())
        msg = GET_TEXT_F(MSG_PRINTING);
    else if (!no_welcome)
        msg = GET_TEXT_F(WELCOME_MSG);
    else
        return;

    set_status(msg, -1);
}

void MarlinUI::abort_print()
{
#if ENABLED(SDSUPPORT)
    wait_for_heatup = wait_for_user = false;
    card.abortFilePrintSoon();
#endif
#ifdef ACTION_ON_CANCEL
    hostui.cancel();
#endif
    IF_DISABLED(SDSUPPORT, print_job_timer.stop());
    TERN_(HOST_PROMPT_SUPPORT, hostui.prompt_open(PROMPT_INFO, F("UI Aborted"), FPSTR(DISMISS_STR)));
    set_status(GET_TEXT_F(MSG_PRINT_ABORTED));
    return_to_status(true);
}

void MarlinUI::pause_print() {
    synchronize(GET_TEXT_F(MSG_PAUSE_PRINT));

    TERN_(HOST_PROMPT_SUPPORT, hostui.prompt_open(PROMPT_PAUSE_RESUME, F("UI Pause"), F("Resume")));

    set_status(GET_TEXT_F(MSG_PRINT_PAUSED));

#if ENABLED(PARK_HEAD_ON_PAUSE)
    pause_show_message(PAUSE_MESSAGE_PARKING, PAUSE_MODE_PAUSE_PRINT); // Show message immediately to let user know about pause in progress
    queue.inject(F("M25 P\nM24"));
#endif
}

void MarlinUI::resume_print()
{
    reset_status();
    TERN_(PARK_HEAD_ON_PAUSE, wait_for_heatup = wait_for_user = false);
    TERN_(SDSUPPORT, if (IS_SD_PAUSED()) queue.inject_P(M24_STR));
#ifdef ACTION_ON_RESUME
    hostui.resume();
#endif
    print_job_timer.start(); // Also called by M24
}

void MarlinUI::synchronize(FSTR_P msg)
{
    static bool no_reentry = false;
    if(no_reentry) return;

    if(msg == nullptr)
        msg = GET_TEXT_F(MSG_MOVING);
    wait.wait(msg);

    planner.synchronize(); // idle() is called until moves complete
    no_reentry = false;

    pages.show_back_page();
}

void MarlinUI::set_brightness(const int16_t value)
{
    brightnes = value;
}

int16_t MarlinUI::get_brightness()
{
    return brightnes;
}

void MarlinUI::update_buttons()
{
    // Nothing to do here, UI updates are made within idle()
}

bool MarlinUI::button_pressed()
{
    return false;
}

void MarlinUI::quick_feedback(const bool /*clear_buttons*/)
{
    refresh();
    buzzer.buzz_on_action();
}

void MarlinUI::refresh()
{
    // Nothing to do
}

void MarlinUI::wait_for_release()
{
    safe_delay(50);
}

bool MarlinUI::use_click()
{
    // TODO
    return false;
}

void MarlinUI::capture()
{
    external_control = true;
}

void MarlinUI::release()
{
    external_control = false;
}

void MarlinUI::chirp()
{
    buzzer.buzz_on_action();
}

void MarlinUI::set_progress(const progress_t p)
{
    progress = p;
}

void MarlinUI::set_progress_done()
{
    progress = 100;
}

uint8_t MarlinUI::get_progress_percent()
{
    return card.isPrinting() ? card.percentDone() : progress;
}

void MarlinUI::buzz(const long duration, const uint16_t freq)
{
    buzzer.buzz_on_action();
}
