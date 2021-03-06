/*
 * This file is part of Cleanflight.
 *
 * Cleanflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cleanflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 */

// Menu contents for PID, RATES, RC preview, misc
// Should be part of the relevant .c file.

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#include "platform.h"

#ifdef CMS

#include "build/version.h"

#include "drivers/system.h"

//#include "common/typeconversion.h"

#include "cms/cms.h"
#include "cms/cms_types.h"
#include "cms/cms_menu_imu.h"

#include "fc/config.h"
#include "fc/rc_controls.h"
#include "fc/runtime_config.h"

#include "flight/pid.h"
#include "config/feature.h"

#include "brainfpv/video.h"
#include "brainfpv/osd_utils.h"
#include "brainfpv/ir_transponder.h"
#include "brainfpv/brainfpv_osd.h"

bfOsdConfig_t bfOsdConfigCms;

static long menuBrainFPVOnEnter(void)
{
    memcpy(&bfOsdConfigCms, bfOsdConfig(), sizeof(bfOsdConfig_t));
    return 0;
}

static long menuBrainFPVOnExit(const OSD_Entry *self)
{
    UNUSED(self);

    memcpy(bfOsdConfigMutable(), &bfOsdConfigCms, sizeof(bfOsdConfig_t));
    return 0;
}

OSD_UINT8_t entryAhiSteps =  {&bfOsdConfigCms.ahi_steps, 0, 4, 1};
const char *STICKS_DISPLAY_NAMES[] = {"OFF", "MODE2", "MODE1"};
OSD_TAB_t entrySticksDisplay = {&bfOsdConfigCms.sticks_display, 2, &STICKS_DISPLAY_NAMES[0]};
const char *FONT_NAMES[] = {"DEFAULT", "LARGE", "BOLD"};
OSD_TAB_t entryOSDFont = {&bfOsdConfigCms.font, 2, &FONT_NAMES[0]};
OSD_UINT8_t entryWhiteLevel =  {&bfOsdConfigCms.white_level, 100, 120, 1};
OSD_UINT8_t entryBlackLevel =  {&bfOsdConfigCms.black_level, 15, 40, 1};
OSD_UINT8_t entrySyncTh =  {&bfOsdConfigCms.sync_threshold, BRAINFPV_OSD_SYNC_TH_MIN, BRAINFPV_OSD_SYNC_TH_MAX, 1};
OSD_INT8_t entryXoff =  {&bfOsdConfigCms.x_offset, -8, 7, 1};
OSD_UINT8_t entryXScale =  {&bfOsdConfigCms.x_scale, 0, 15, 1};
OSD_UINT8_t entry3DShift =  {&bfOsdConfigCms.sbs_3d_right_eye_offset, 10, 40, 1};



OSD_Entry cmsx_menuBrainFPVOsdEntries[] =
{
    {"------- OSD --------", OME_Label, NULL, NULL, 0},
    {"AHI STEPS", OME_UINT8, NULL, &entryAhiSteps, 0},
    {"ALTITUDE SCALE", OME_Bool, NULL, &bfOsdConfigCms.altitude_scale, 0},
    {"SHOW STICKS", OME_TAB, NULL, &entrySticksDisplay, 0},
    {"FONT", OME_TAB, NULL, &entryOSDFont, 0},
    {"OSD WHITE", OME_UINT8, NULL, &entryWhiteLevel, 0},
    {"OSD BLACK", OME_UINT8, NULL, &entryBlackLevel, 0},
    {"OSD SYNC TH", OME_UINT8, NULL, &entrySyncTh, 0},
    {"OSD X OFF", OME_INT8, NULL, &entryXoff, 0},
    {"OSD X SC", OME_UINT8, NULL, &entryXScale, 0},
    {"3D MODE", OME_Bool, NULL, &bfOsdConfigCms.sbs_3d_enabled, 0},
    {"3D R SHIFT", OME_UINT8, NULL, &entry3DShift, 0},

    {"BACK", OME_Back, NULL, NULL, 0},
    {NULL, OME_END, NULL, NULL, 0}
};

CMS_Menu cmsx_menuBrainFPVOsd = {
    .GUARD_text = "MENUBrainFPVOSD",
    .GUARD_type = OME_MENU,
    .onEnter = NULL,
    .onExit = NULL,
    .onGlobalExit = NULL,
    .entries = cmsx_menuBrainFPVOsdEntries,
};


const char * IR_NAMES[] = {"OFF", "I-LAP", "TRACKMATE"};
OSD_TAB_t entryIRSys = {&bfOsdConfigCms.ir_system, 2, &IR_NAMES[0]};
OSD_UINT32_t entryIRIlap =  {&bfOsdConfigCms.ir_ilap_id, 0, 9999999, 1};
OSD_UINT16_t entryIRTrackmate =  {&bfOsdConfigCms.ir_trackmate_id, 0, 4095, 1};

OSD_Entry cmsx_menuBrainFPVIrEntries[] =
{
    {"-- IR TRANSPONDER --", OME_Label, NULL, NULL, 0},
    {"IR SYS", OME_TAB, NULL, &entryIRSys, 0},
    {"I LAP ID", OME_UINT32, NULL, &entryIRIlap, 0},
    {"TRACKMATE ID", OME_UINT16, NULL, &entryIRTrackmate, 0},

    {"BACK", OME_Back, NULL, NULL, 0},
    {NULL, OME_END, NULL, NULL, 0}
};

CMS_Menu cmsx_menuBrainFPVIr = {
    .GUARD_text = "MENUBrainFPVIR",
    .GUARD_type = OME_MENU,
    .onEnter = NULL,
    .onExit = NULL,
    .onGlobalExit = NULL,
    .entries = cmsx_menuBrainFPVIrEntries,
};

OSD_Entry cmsx_menuBrainFPVEntires[] =
{
    {"--- BRAINFPV ---", OME_Label, NULL, NULL, 0},
    {"OSD", OME_Submenu, cmsMenuChange, &cmsx_menuBrainFPVOsd, 0},

    {"IR TRANSPONDER", OME_Submenu, cmsMenuChange, &cmsx_menuBrainFPVIr, 0},
#if defined(USE_BRAINFPV_SPECTROGRAPH)
    {"SPECTROGRAPH", OME_Bool, NULL, &bfOsdConfigCms.spec_enabled, 0},
#endif /* defined(USE_BRAINFPV_SPECTROGRAPH) */
    {"SHOW LOGO ON ARM", OME_Bool, NULL, &bfOsdConfigCms.show_logo_on_arm, 0},
    {"SHOW PILOT LOGO", OME_Bool, NULL, &bfOsdConfigCms.show_pilot_logo, 0},
    {"BACK", OME_Back, NULL, NULL, 0},
    {NULL, OME_END, NULL, NULL, 0}
};

CMS_Menu cmsx_menuBrainFPV = {
    .GUARD_text = "MENUBrainFPV",
    .GUARD_type = OME_MENU,
    .onEnter = menuBrainFPVOnEnter,
    .onExit = menuBrainFPVOnExit,
    .onGlobalExit = NULL,
    .entries = cmsx_menuBrainFPVEntires,
};
#endif // CMS
