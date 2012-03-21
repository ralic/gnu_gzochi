/* avatars.c: Avatar bitmap data for gzochi mazewar example game
 * Copyright (C) 2012 Julian Graham
 *
 * This is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
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
 */

#include "avatars.h"

/* The avatar sprite data, in the form of a single bitmap in a packed PBM-like 
   format. This data is partially derived from data in the HP-UX Mazewar 
   distribution by Christopher A. Kent. */

unsigned char mazewar_avatar_bits[3072] = 
  {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf8, 0x1f, 0xff, 
    0xff, 0xf8, 0x1f, 0xff, 0xf0, 0x4f, 0xff, 0xc3, 0xff, 0xff, 0xc3, 0xff, 
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe0, 
    0x07, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x03, 0xff, 
    0xff, 0xc0, 0x03, 0xff, 0xe0, 0x37, 0xff, 0x06, 0xff, 0xff, 0x0e, 0xff, 
    0xff, 0xff, 0xff, 0xe0, 0x07, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x00, 
    0x00, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe0, 0x07, 0xff, 0xff, 0xff, 
    0xff, 0xff, 0xff, 0xe0, 0x07, 0xff, 0xff, 0xff, 0xff, 0x00, 0x30, 0xff, 
    0xff, 0x00, 0x30, 0xff, 0xc0, 0x0b, 0xfc, 0x01, 0x3f, 0xfc, 0x01, 0x3f, 
    0xff, 0xff, 0xfe, 0x00, 0x00, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x00, 
    0x00, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x7f, 0xff, 0xff, 
    0xff, 0xff, 0xfe, 0x00, 0x00, 0x7f, 0xff, 0xff, 0xfe, 0x00, 0x1c, 0x7f, 
    0xfe, 0x00, 0x1c, 0x7f, 0xc0, 0x0b, 0xf8, 0x00, 0xdf, 0xf8, 0x00, 0xdf, 
    0xff, 0xff, 0xf0, 0x00, 0x00, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x00, 
    0x00, 0x03, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x00, 0x00, 0x0f, 0xff, 0xff, 
    0xff, 0xff, 0xf0, 0x00, 0x00, 0x0f, 0xff, 0xff, 0xfc, 0x00, 0x07, 0x3f, 
    0xfc, 0x00, 0x07, 0x3f, 0x80, 0x1d, 0xf0, 0x00, 0x2f, 0xf0, 0x00, 0x2f, 
    0xff, 0xff, 0xc0, 0x00, 0x00, 0x03, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 
    0x38, 0x00, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x03, 0xff, 0xff, 
    0xff, 0xff, 0xc0, 0x00, 0x00, 0x03, 0xff, 0xff, 0xf8, 0x00, 0x01, 0x9f, 
    0xf8, 0x00, 0x01, 0x9f, 0x80, 0x39, 0xf0, 0x00, 0x0f, 0xf0, 0x00, 0x2f, 
    0xff, 0xff, 0x00, 0x00, 0x38, 0x00, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x00, 
    0x06, 0x18, 0x7f, 0xff, 0xff, 0xff, 0x00, 0x00, 0x38, 0x00, 0xff, 0xff, 
    0xff, 0xff, 0x00, 0x00, 0x0e, 0x00, 0xff, 0xff, 0xf0, 0x00, 0x00, 0xcf, 
    0xf0, 0x00, 0x00, 0xcf, 0x80, 0x39, 0xe0, 0x00, 0x07, 0xe0, 0x00, 0x77, 
    0xff, 0xfe, 0x00, 0x00, 0x06, 0x18, 0x7f, 0xff, 0xff, 0xfc, 0x00, 0x00, 
    0x03, 0xf0, 0x3f, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x06, 0x18, 0x7f, 0xff, 
    0xff, 0xfe, 0x00, 0x00, 0x0f, 0x98, 0x7f, 0xff, 0xf0, 0x00, 0x00, 0x2f, 
    0xf0, 0x00, 0x00, 0x2f, 0x80, 0x1d, 0xe0, 0x00, 0x07, 0xe0, 0x00, 0xf7, 
    0xff, 0xfc, 0x00, 0x00, 0x03, 0xf0, 0x3f, 0xff, 0xff, 0xf8, 0x00, 0x00, 
    0x01, 0xfc, 0x1f, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x03, 0xf0, 0x3f, 0xff, 
    0xff, 0xfc, 0x00, 0x00, 0x03, 0xe0, 0x3f, 0xff, 0xe0, 0x00, 0x00, 0x07, 
    0xe0, 0x00, 0x00, 0x67, 0xc0, 0x0b, 0xc0, 0x00, 0x03, 0xc0, 0x01, 0xf3, 
    0xff, 0xf8, 0x00, 0x00, 0x01, 0xfc, 0x1f, 0xff, 0xff, 0xf0, 0x00, 0x00, 
    0x00, 0x7f, 0x4f, 0xff, 0xff, 0xf8, 0x00, 0x00, 0x01, 0xfc, 0x1f, 0xff, 
    0xff, 0xf8, 0x00, 0x00, 0x03, 0xfc, 0x1f, 0xff, 0xe0, 0x00, 0x00, 0x07, 
    0xe0, 0x00, 0x00, 0xf7, 0xc0, 0x03, 0xc0, 0x00, 0x03, 0xc0, 0x03, 0xe3, 
    0xff, 0xf0, 0x00, 0x00, 0x00, 0x7f, 0x4f, 0xff, 0xff, 0xe0, 0x00, 0x00, 
    0x00, 0x4f, 0xe7, 0xff, 0xff, 0xf0, 0x00, 0x00, 0x00, 0x7f, 0x4f, 0xff, 
    0xff, 0xf0, 0x00, 0x00, 0x00, 0x7f, 0x0f, 0xff, 0xe0, 0x00, 0x00, 0x07, 
    0xe0, 0x00, 0x01, 0xef, 0xe0, 0x07, 0xc0, 0x00, 0x03, 0xc0, 0x03, 0xe3, 
    0xff, 0xe0, 0x00, 0x00, 0x00, 0x4f, 0xe7, 0xff, 0xff, 0xc0, 0x00, 0x00, 
    0x00, 0x83, 0xc3, 0xff, 0xff, 0xe0, 0x00, 0x00, 0x00, 0x4f, 0xe7, 0xff, 
    0xff, 0xe0, 0x00, 0x00, 0x00, 0x4f, 0xc7, 0xff, 0xc0, 0x00, 0x00, 0x03, 
    0xc0, 0x00, 0x03, 0xe7, 0xc4, 0x27, 0xc0, 0x00, 0x03, 0xc0, 0x01, 0xf3, 
    0xff, 0xc0, 0x00, 0x00, 0x00, 0x83, 0xc3, 0xff, 0xff, 0x80, 0x00, 0x00, 
    0x00, 0xc0, 0xe1, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x83, 0xc3, 0xff, 
    0xff, 0xc0, 0x00, 0x00, 0x00, 0x83, 0xc3, 0xff, 0xc0, 0x00, 0x00, 0x03, 
    0xc0, 0x00, 0x07, 0xc7, 0xa3, 0x8b, 0xe0, 0x00, 0x07, 0xe0, 0x00, 0xf7, 
    0xff, 0x80, 0x00, 0x00, 0x00, 0xc0, 0xe1, 0xff, 0xff, 0x00, 0x00, 0x00, 
    0x00, 0x81, 0x70, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0xc0, 0xe1, 0xff, 
    0xff, 0x80, 0x00, 0x00, 0x00, 0xc0, 0xe1, 0xff, 0xc0, 0x00, 0x00, 0x03, 
    0xc0, 0x00, 0x0f, 0xc3, 0xd0, 0x17, 0xe0, 0x00, 0x07, 0xe0, 0x00, 0x77, 
    0xff, 0x00, 0x00, 0x00, 0x00, 0x81, 0x70, 0xff, 0xff, 0x00, 0x00, 0x00, 
    0x00, 0x06, 0x38, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x81, 0x70, 0xff, 
    0xff, 0x00, 0x00, 0x00, 0x00, 0x81, 0x70, 0xff, 0xc0, 0x00, 0x00, 0x03, 
    0xc0, 0x00, 0x0f, 0xc3, 0xff, 0xff, 0xf0, 0x00, 0x0f, 0xf0, 0x00, 0x0f, 
    0xff, 0x00, 0x00, 0x00, 0x00, 0x06, 0x38, 0xff, 0xfe, 0x00, 0x00, 0x00, 
    0x00, 0x0c, 0x08, 0x7f, 0xff, 0x00, 0x00, 0x00, 0x00, 0x06, 0x38, 0xff, 
    0xff, 0x00, 0x00, 0x00, 0x00, 0x06, 0x38, 0xff, 0xc0, 0x00, 0x00, 0x03, 
    0xc0, 0x00, 0x07, 0xc3, 0xfc, 0x3f, 0xf0, 0x00, 0x0f, 0xf0, 0x00, 0x0f, 
    0xfe, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x08, 0x7f, 0xfe, 0xc0, 0x00, 0x00, 
    0x00, 0x00, 0x04, 0x7f, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x08, 0x7f, 
    0xfe, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x18, 0x7f, 0xc0, 0x00, 0x00, 0x03, 
    0xc0, 0x00, 0x03, 0xe3, 0xf0, 0x4f, 0xf8, 0x00, 0x1f, 0xf8, 0x00, 0x1f, 
    0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x7f, 0xfc, 0xfc, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x3f, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x7f, 
    0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x7f, 0xe0, 0x00, 0x00, 0x07, 
    0xe0, 0x00, 0x01, 0xe7, 0xe0, 0x37, 0xfc, 0x00, 0x3f, 0xfc, 0x00, 0x3f, 
    0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x3f, 0xfd, 0xff, 0x80, 0x00, 
    0x00, 0x00, 0x00, 0x3f, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 
    0xfc, 0x00, 0x00, 0x1f, 0xf8, 0x00, 0x00, 0x3f, 0xe0, 0x00, 0x00, 0x07, 
    0xe0, 0x00, 0x00, 0xf7, 0xc0, 0x0b, 0xe9, 0x81, 0x2f, 0xe9, 0x81, 0x2f, 
    0xfc, 0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xbf, 0xfd, 0xff, 0xe0, 0x00, 
    0x00, 0x00, 0x00, 0x3f, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 
    0xfc, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x3f, 0xe0, 0x00, 0x00, 0x07, 
    0xe0, 0x00, 0x00, 0x67, 0xd0, 0x03, 0xd4, 0x60, 0x57, 0xd4, 0x60, 0x57, 
    0xfc, 0x00, 0x00, 0x00, 0x00, 0x07, 0xff, 0xbf, 0xf9, 0xff, 0xf8, 0x00, 
    0x00, 0x00, 0x00, 0x1f, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 
    0xfc, 0x00, 0x07, 0xff, 0xff, 0xe0, 0x00, 0x3f, 0xf0, 0x00, 0x00, 0x0f, 
    0xf0, 0x00, 0x00, 0x0f, 0xb8, 0x01, 0xea, 0x8a, 0xaf, 0xea, 0x8a, 0xaf, 
    0xf8, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0x9f, 0xf8, 0xff, 0xfc, 0x00, 
    0x00, 0x00, 0x00, 0x1f, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 
    0xf8, 0x00, 0x3f, 0xff, 0xff, 0xfc, 0x00, 0x1f, 0xf0, 0x00, 0x00, 0x0f, 
    0xf0, 0x00, 0x00, 0x0f, 0x9c, 0x01, 0xfd, 0x55, 0x7f, 0xfd, 0x55, 0x7f, 
    0xf8, 0x00, 0x00, 0x00, 0x00, 0x0c, 0xff, 0x1f, 0xf8, 0xff, 0xaf, 0x00, 
    0x00, 0x00, 0x00, 0x1f, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 
    0xf8, 0x00, 0xff, 0xf8, 0x1f, 0xff, 0x00, 0x1f, 0xf9, 0x00, 0x00, 0x1f, 
    0xf9, 0x00, 0x00, 0x1f, 0x9c, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
    0xf8, 0x00, 0x00, 0x00, 0x00, 0xf3, 0xff, 0x1f, 0xf8, 0xff, 0xcf, 0x80, 
    0x00, 0x00, 0x00, 0x1f, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 
    0xf8, 0x03, 0xff, 0xe2, 0x4f, 0xff, 0xc0, 0x1f, 0xfc, 0xc0, 0x00, 0x3f, 
    0xfc, 0xc0, 0x00, 0x3f, 0xb8, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
    0xf8, 0x00, 0x00, 0x00, 0x01, 0xfd, 0xff, 0x1f, 0xf0, 0x7f, 0xe3, 0xc0, 
    0x00, 0x00, 0x00, 0x0f, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 
    0xf8, 0x0f, 0xff, 0xc2, 0x5f, 0xff, 0xf0, 0x1f, 0xe9, 0x00, 0x00, 0x1f, 
    0xe9, 0x00, 0x00, 0x1f, 0xd0, 0x03, 0xff, 0xc3, 0xff, 0xff, 0xc3, 0xff, 
    0xf0, 0x00, 0x00, 0x00, 0x03, 0xff, 0x7e, 0x0f, 0xf2, 0x7e, 0x51, 0xe0, 
    0x00, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 
    0xf0, 0x3f, 0xbf, 0x91, 0x3f, 0xff, 0xfc, 0x0f, 0xd4, 0x60, 0x00, 0x57, 
    0xd4, 0x60, 0x00, 0x57, 0xc0, 0x03, 0xff, 0x06, 0xff, 0xff, 0x06, 0xff, 
    0xf0, 0x00, 0x00, 0x00, 0x07, 0xbf, 0xfe, 0x4f, 0xf0, 0x7d, 0xbe, 0x30, 
    0x00, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 
    0xf0, 0x4e, 0xff, 0x8c, 0x3d, 0xff, 0x7c, 0x0f, 0xaa, 0x38, 0x04, 0xab, 
    0xaa, 0x38, 0x04, 0xab, 0xe0, 0x07, 0xfc, 0x01, 0x3f, 0xfc, 0x01, 0x3f, 
    0xf0, 0x00, 0x00, 0x00, 0x0c, 0x7f, 0xfe, 0x0f, 0xf1, 0x7f, 0xff, 0x88, 
    0x00, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 
    0xf0, 0xf1, 0xff, 0x08, 0x3c, 0xff, 0xab, 0x0f, 0xd5, 0x03, 0xc1, 0x57, 
    0xd5, 0x03, 0xc1, 0x57, 0xc4, 0x27, 0xf8, 0x00, 0xdf, 0xf8, 0x00, 0xdf, 
    0xf0, 0x00, 0x00, 0x00, 0x11, 0x99, 0xfe, 0x8f, 0xf1, 0x7f, 0xff, 0xc0, 
    0x00, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 
    0xf1, 0xfe, 0xff, 0x60, 0x76, 0xff, 0x49, 0x8f, 0xea, 0xa0, 0x0a, 0xaf, 
    0xea, 0xa0, 0x0a, 0xaf, 0xa3, 0x8b, 0xf0, 0x00, 0x2f, 0xf0, 0x00, 0x2f, 
    0xf0, 0x00, 0x00, 0x00, 0x03, 0xe7, 0xfe, 0x8f, 0xf1, 0x7f, 0xff, 0xfc, 
    0x00, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 
    0xf2, 0x3f, 0xff, 0x00, 0xe0, 0xff, 0xf7, 0xcf, 0xfd, 0x55, 0x55, 0x7f, 
    0xfd, 0x55, 0x55, 0x7f, 0xd0, 0x17, 0xf0, 0x7e, 0x0f, 0xf4, 0x00, 0x0f, 
    0xf0, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xfe, 0x8f, 0xf0, 0x7f, 0xff, 0xf8, 
    0x00, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 
    0xf0, 0x5b, 0xff, 0x10, 0x00, 0xff, 0xcf, 0xcf, 0xff, 0xff, 0xff, 0xff, 
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe1, 0xff, 0x87, 0xee, 0x00, 0x07, 
    0xf0, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xfe, 0x0f, 0xf0, 0x7f, 0xff, 0xf0, 
    0x00, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 
    0xf1, 0xc7, 0xff, 0x60, 0x16, 0xff, 0xbf, 0x8f, 0xff, 0xff, 0xff, 0xff, 
    0xff, 0xff, 0xff, 0xff, 0xfc, 0x3f, 0xe7, 0xe7, 0xe7, 0xef, 0x00, 0x07, 
    0xf0, 0x00, 0x00, 0x00, 0x0f, 0xff, 0xfe, 0x0f, 0xf0, 0x7f, 0xff, 0xe0, 
    0x00, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 
    0xf0, 0xff, 0xff, 0x18, 0x20, 0xff, 0x7f, 0x0f, 0xff, 0xf8, 0x1f, 0xff, 
    0xff, 0xf8, 0x1f, 0xff, 0xf0, 0x4f, 0xcf, 0xc7, 0xf3, 0xcf, 0x80, 0x03, 
    0xf0, 0x00, 0x00, 0x00, 0x07, 0xf9, 0xfe, 0x0f, 0xf2, 0x7f, 0xff, 0xc0, 
    0x00, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 
    0xf0, 0x75, 0xff, 0x84, 0x51, 0xff, 0xfe, 0x0f, 0xff, 0xc0, 0x03, 0xff, 
    0xff, 0xc0, 0x03, 0xff, 0xe0, 0x37, 0xdf, 0x8d, 0xfb, 0xc7, 0xc0, 0x03, 
    0xf0, 0x00, 0x00, 0x00, 0x00, 0x76, 0xfe, 0x4f, 0xf8, 0x7f, 0xff, 0x80, 
    0x00, 0x00, 0x00, 0x1f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 
    0xf0, 0x2e, 0xdf, 0x97, 0x09, 0xff, 0xfc, 0x0f, 0xff, 0x00, 0x30, 0xff, 
    0xff, 0x00, 0x30, 0xff, 0xc0, 0x0b, 0xdf, 0x89, 0xfb, 0xc7, 0xc0, 0x03, 
    0xf8, 0x00, 0x00, 0x00, 0x01, 0x0f, 0xfe, 0x1f, 0xf8, 0xff, 0xff, 0x00, 
    0x00, 0x00, 0x00, 0x1f, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 
    0xf8, 0x1f, 0x3f, 0xc1, 0x63, 0xff, 0xf0, 0x1f, 0xfe, 0x00, 0x1c, 0x7f, 
    0xfe, 0x00, 0x1c, 0x7f, 0xc0, 0x03, 0xcf, 0x81, 0xf3, 0xcf, 0x80, 0x03, 
    0xf8, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x1f, 0xf8, 0xff, 0xfc, 0x00, 
    0x00, 0x00, 0x00, 0x1f, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 
    0xf8, 0x07, 0xf7, 0xe2, 0x47, 0xff, 0xc0, 0x1f, 0xfc, 0x00, 0x07, 0x3f, 
    0xfc, 0x00, 0x07, 0x3f, 0x80, 0x01, 0xe7, 0xc3, 0xe7, 0xef, 0x00, 0x07, 
    0xf8, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xf7, 0x1f, 0xf8, 0xff, 0xf8, 0x00, 
    0x00, 0x00, 0x00, 0x1f, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 
    0xf8, 0x01, 0xdf, 0x78, 0x1f, 0xff, 0x00, 0x1f, 0xf8, 0x00, 0x01, 0x9f, 
    0xf8, 0x00, 0x01, 0x9f, 0x80, 0x01, 0xe1, 0xe7, 0x87, 0xee, 0x00, 0x07, 
    0xf8, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xef, 0x1f, 0xfc, 0xff, 0xe0, 0x00, 
    0x00, 0x00, 0x00, 0x3f, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 
    0xf8, 0x00, 0x76, 0x7f, 0xff, 0xfc, 0x00, 0x1f, 0xf0, 0x00, 0x00, 0xcf, 
    0xf0, 0x00, 0x00, 0xcf, 0x80, 0x01, 0xf0, 0x7e, 0x0f, 0xf0, 0x00, 0x0f, 
    0xfc, 0x00, 0x00, 0x00, 0x00, 0x07, 0xeb, 0xbf, 0xfc, 0x7f, 0x80, 0x00, 
    0x00, 0x00, 0x00, 0x3f, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 
    0xfc, 0x00, 0x11, 0xff, 0xff, 0xe0, 0x00, 0x3f, 0xf0, 0x00, 0x00, 0x2f, 
    0xf0, 0x00, 0x00, 0x2f, 0x80, 0x01, 0xf0, 0x00, 0x0f, 0xf0, 0x00, 0x0f, 
    0xfc, 0x00, 0x00, 0x00, 0x00, 0x01, 0xd7, 0xbf, 0xfc, 0x7c, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x3f, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 
    0xfc, 0x00, 0x07, 0xff, 0xff, 0x00, 0x00, 0x3f, 0xe0, 0x0f, 0xf0, 0x07, 
    0xe6, 0x00, 0x00, 0x07, 0xc0, 0x03, 0xf8, 0x00, 0x1f, 0xf8, 0x00, 0x1f, 
    0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xbf, 0xfe, 0x60, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x7f, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 
    0xfc, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x3f, 0xe0, 0x7f, 0xfe, 0x07, 
    0xef, 0x00, 0x00, 0x07, 0xc0, 0x03, 0xfc, 0x00, 0x3f, 0xfc, 0x00, 0x3f, 
    0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x7f, 0xfe, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x7f, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 
    0xfe, 0x00, 0x00, 0x1f, 0xf8, 0x00, 0x00, 0x7f, 0xe1, 0xfc, 0x3f, 0x87, 
    0xe7, 0x80, 0x00, 0x07, 0xe0, 0x07, 0xe9, 0x81, 0x2f, 0xe9, 0x81, 0x2f, 
    0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0x04, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 
    0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xc7, 0xf8, 0x7f, 0xe3, 
    0xc7, 0xc0, 0x00, 0x03, 0xd4, 0x27, 0xd4, 0x60, 0x57, 0xd4, 0x60, 0x57, 
    0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0xff, 0xff, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0xff, 0xff, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 
    0xff, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xcf, 0xf0, 0x6f, 0xf3, 
    0xc3, 0xe0, 0x00, 0x03, 0xab, 0x8b, 0xea, 0x8a, 0xaf, 0xea, 0x8a, 0xaf, 
    0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x82, 0x00, 0x00, 
    0x00, 0x00, 0x01, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 
    0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xdf, 0xf0, 0xcf, 0xfb, 
    0xc3, 0xf0, 0x00, 0x03, 0xd4, 0x17, 0xfd, 0x55, 0x7f, 0xfd, 0x55, 0x7f, 
    0xff, 0x82, 0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xc1, 0x00, 0x00, 
    0x00, 0x00, 0x03, 0xff, 0xff, 0x82, 0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 
    0xff, 0x82, 0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xdf, 0xf0, 0x0f, 0xfb, 
    0xc3, 0xf0, 0x00, 0x03, 0xff, 0xff, 0xff, 0xfc, 0x1e, 0x0f, 0x07, 0x83, 
    0xff, 0xc1, 0x00, 0x00, 0x00, 0x00, 0x03, 0xff, 0xff, 0xe0, 0xe0, 0x00, 
    0x00, 0x00, 0x07, 0xff, 0xff, 0xc1, 0x00, 0x00, 0x00, 0x00, 0x03, 0xff, 
    0xff, 0xc1, 0x00, 0x00, 0x00, 0x00, 0x03, 0xff, 0xcf, 0xf0, 0x0f, 0xf3, 
    0xc3, 0xe0, 0x00, 0x03, 0xfc, 0x3f, 0xff, 0xf8, 0x0c, 0x06, 0x03, 0x01, 
    0xff, 0xe0, 0xe0, 0x00, 0x00, 0x00, 0x07, 0xff, 0xff, 0xf0, 0x78, 0x00, 
    0x00, 0x00, 0x0f, 0xff, 0xff, 0xe0, 0xe0, 0x00, 0x00, 0x00, 0x07, 0xff, 
    0xff, 0xe0, 0xe0, 0x00, 0x00, 0x00, 0x07, 0xff, 0xc7, 0xf8, 0x1f, 0xe3, 
    0xc7, 0xc0, 0x00, 0x03, 0xf0, 0x4f, 0xff, 0xf0, 0x80, 0x00, 0x00, 0x00, 
    0xff, 0xf0, 0x78, 0x00, 0x00, 0x00, 0x0f, 0xff, 0xff, 0xfa, 0x1f, 0x00, 
    0x00, 0x00, 0x3f, 0xff, 0xff, 0xf0, 0x78, 0x00, 0x00, 0x00, 0x0f, 0xff, 
    0xff, 0xf0, 0x78, 0x00, 0x00, 0x00, 0x0f, 0xff, 0xe1, 0xfc, 0x3f, 0x87, 
    0xe7, 0x80, 0x00, 0x07, 0xe0, 0x37, 0xff, 0xf3, 0xe0, 0x03, 0x00, 0x03, 
    0xff, 0xfa, 0x1f, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xff, 0xfd, 0x8f, 0xf0, 
    0x00, 0x00, 0x1f, 0xff, 0xff, 0xfa, 0x1f, 0x00, 0x00, 0x00, 0x3f, 0xff, 
    0xff, 0xfa, 0x1f, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xe0, 0x7f, 0xfe, 0x07, 
    0xef, 0x00, 0x00, 0x07, 0xc0, 0x0b, 0xb4, 0xa7, 0x70, 0x01, 0xc0, 0x0e, 
    0xff, 0xfd, 0x8f, 0xf0, 0x00, 0x00, 0x1f, 0xff, 0xff, 0x00, 0x41, 0xc0, 
    0x00, 0x00, 0x2a, 0xff, 0xff, 0xfd, 0x8f, 0xf0, 0x00, 0x00, 0x1f, 0xff, 
    0xff, 0xfd, 0x8f, 0xf0, 0x00, 0x00, 0x1f, 0xff, 0xe0, 0x0f, 0xf0, 0x07, 
    0xe6, 0x00, 0x00, 0x07, 0xc7, 0xe3, 0xff, 0xf3, 0xe0, 0x03, 0x00, 0x03, 
    0xff, 0x00, 0x41, 0xc0, 0x00, 0x00, 0x2a, 0xff, 0xfd, 0x54, 0x38, 0x00, 
    0x00, 0x00, 0x55, 0x7f, 0xff, 0x00, 0x41, 0xc0, 0x00, 0x00, 0x2a, 0xff, 
    0xff, 0x00, 0x41, 0xc0, 0x00, 0x00, 0x2a, 0xff, 0xf0, 0x00, 0x00, 0x0f, 
    0xf0, 0x00, 0x00, 0x0f, 0x9e, 0x79, 0xff, 0xf0, 0x80, 0x00, 0x00, 0x00, 
    0xfd, 0x54, 0x38, 0x00, 0x00, 0x00, 0x55, 0x7f, 0xfa, 0xaa, 0x0c, 0x00, 
    0x01, 0x00, 0xaa, 0xbf, 0xfd, 0x54, 0x38, 0x00, 0x00, 0x00, 0x55, 0x7f, 
    0xfd, 0x54, 0x38, 0x00, 0x00, 0x00, 0x55, 0x7f, 0xf0, 0x00, 0x00, 0x0f, 
    0xf0, 0x00, 0x00, 0x0f, 0xbc, 0xbd, 0xff, 0xf8, 0x0c, 0x06, 0x03, 0x01, 
    0xfa, 0xaa, 0x0c, 0x00, 0x01, 0x00, 0xaa, 0xbf, 0xf5, 0x55, 0x01, 0xc0, 
    0x03, 0x05, 0x55, 0x5f, 0xfa, 0xaa, 0x0c, 0x00, 0x01, 0x00, 0xaa, 0xbf, 
    0xfa, 0xaa, 0x0c, 0x00, 0x01, 0x00, 0xaa, 0xbf, 0xf9, 0x00, 0x00, 0x1f, 
    0xf9, 0x00, 0x00, 0x1f, 0xbc, 0x3d, 0xff, 0xfc, 0x1e, 0x0f, 0x07, 0x83, 
    0xf5, 0x55, 0x01, 0xc0, 0x03, 0x05, 0x55, 0x5f, 0xfa, 0xaa, 0x80, 0x0c, 
    0x30, 0x0a, 0xaa, 0xaf, 0xf5, 0x55, 0x01, 0xc0, 0x03, 0x05, 0x55, 0x5f, 
    0xf5, 0x55, 0x01, 0xc0, 0x03, 0x05, 0x55, 0x5f, 0xfc, 0xc0, 0x00, 0x3f, 
    0xfc, 0xc0, 0x00, 0x3f, 0x9e, 0x79, 0xff, 0xfb, 0x6d, 0xcf, 0x3c, 0xf3, 
    0xfa, 0xaa, 0x80, 0x0c, 0x30, 0x0a, 0xaa, 0xaf, 0xfd, 0x55, 0x50, 0x07, 
    0xe0, 0x15, 0x55, 0x5f, 0xfa, 0xaa, 0x80, 0x0c, 0x30, 0x0a, 0xaa, 0xaf, 
    0xfa, 0xaa, 0x80, 0x0c, 0x30, 0x0a, 0xaa, 0xaf, 0xe9, 0x00, 0x00, 0x1f, 
    0xe9, 0x00, 0x00, 0x1f, 0xc7, 0xe3, 0xff, 0xf4, 0x33, 0x86, 0x18, 0x61, 
    0xfd, 0x55, 0x50, 0x07, 0xe0, 0x15, 0x55, 0x5f, 0xfe, 0xaa, 0xaa, 0x00, 
    0x00, 0xaa, 0xaa, 0xbf, 0xfd, 0x55, 0x50, 0x07, 0xe0, 0x15, 0x55, 0x5f, 
    0xfd, 0x55, 0x50, 0x07, 0xe0, 0x15, 0x55, 0x5f, 0xd4, 0x60, 0x00, 0x57, 
    0xd4, 0x60, 0x00, 0x57, 0xc0, 0x03, 0xff, 0xfb, 0x6d, 0x30, 0x0c, 0x03, 
    0xfe, 0xaa, 0xaa, 0x00, 0x00, 0xaa, 0xaa, 0xbf, 0xff, 0x55, 0x55, 0x50, 
    0x05, 0x55, 0x55, 0xff, 0xfe, 0xaa, 0xaa, 0x00, 0x00, 0xaa, 0xaa, 0xbf, 
    0xfe, 0xaa, 0xaa, 0x00, 0x00, 0xaa, 0xaa, 0xbf, 0xaa, 0x38, 0x04, 0xab, 
    0xaa, 0x38, 0x04, 0xab, 0xe0, 0x07, 0xff, 0x99, 0x99, 0x30, 0x0c, 0x03, 
    0xff, 0x55, 0x55, 0x50, 0x05, 0x55, 0x55, 0xff, 0xff, 0xfa, 0xaa, 0xaa, 
    0xaa, 0xaa, 0xaf, 0xff, 0xff, 0x55, 0x55, 0x50, 0x05, 0x55, 0x55, 0xff, 
    0xff, 0x55, 0x55, 0x50, 0x05, 0x55, 0x55, 0xff, 0xd5, 0x03, 0xc1, 0x57, 
    0xd5, 0x03, 0xc1, 0x57, 0xd4, 0x37, 0xff, 0x60, 0xc3, 0x86, 0x18, 0x61, 
    0xff, 0xfa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaf, 0xff, 0xff, 0xff, 0xf5, 0x55, 
    0x55, 0x5f, 0xff, 0xff, 0xff, 0xfa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaf, 0xff, 
    0xff, 0xfa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaf, 0xff, 0xea, 0xa0, 0x0a, 0xaf, 
    0xea, 0xa0, 0x0a, 0xaf, 0xab, 0xab, 0xff, 0x60, 0xc3, 0xcf, 0x3c, 0xf3, 
    0xff, 0xff, 0xf5, 0x55, 0x55, 0x5f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf5, 0x55, 0x55, 0x5f, 0xff, 0xff, 
    0xff, 0xff, 0xf5, 0x55, 0x55, 0x5f, 0xff, 0xff, 0xfd, 0x55, 0x55, 0x7f, 
    0xfd, 0x55, 0x55, 0x7f, 0xd5, 0x57, 0xff, 0x99, 0x99, 0xff, 0xff, 0xff
  };