
/**
 ******************************************************************************
 * @file       fonts.h
 * @author     dRonin, http://dRonin.org/, Copyright (C) 2016
 * @author     The OpenPilot Team, http://www.openpilot.org, Copyright (C) 2012
 * @author     Thomas Oldbury Copyright (C) 2010
 * @addtogroup dRonin Modules
 * @{
 * @addtogroup OnScreenDisplay Module
 * @{
 * @brief Fonts for OSD
 *****************************************************************************/
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Additional note on redistribution: The copyright and license notices above
 * must be maintained in each individual source file that is a derivative work
 * of this source file; otherwise redistribution is prohibited.
 *
 * NOTE: This file is generated by encode_fonts.py DO NOT EDIT!
 * NOTE: Some fonts are CC 3.0 BY-SA and note GPL licensed. See FONT_LICENSE.txt
 */



#ifndef FONTS_H
#define FONTS_H

#include <stdint.h>


struct FontEntry {
	uint8_t width;
	uint8_t height;
	const uint8_t* lookup;
	const uint16_t* data;
};

#define NUM_FONTS  5

#define BETAFLIGHT_DEFAULT 0
#define BETAFLIGHT_LARGE 1
#define BETAFLIGHT_BOLD 2
#define FONT8X10 3
#define FONT_OUTLINED8X8 4


#endif /* FONTS_H */
