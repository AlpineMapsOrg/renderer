/*****************************************************************************
 * Alpine Maps and weBIGeo
 * Copyright (C) 2024 Gerald Kimmersdorfer
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
 *****************************************************************************/

// Limit the dimensions of images to 8192x8192. This is already quite restricting
// in terms of that a lot of GPUs don't support textures that large. Make sure
// you know what you are doing, before you change this value.
#define STBI_MAX_DIMENSIONS 8192

// Only include the code for the formats we need. This reduces the code footprint
// and allows for faster compilation times. Add more formats here, if you need them.
// For possible options check the stb_image.h file.
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG

// Remove if you intend to use stbi_failure_reason()
#define STBI_NO_FAILURE_STRINGS

#define STB_IMAGE_IMPLEMENTATION
#include <stb_slim/stb_image.h>
