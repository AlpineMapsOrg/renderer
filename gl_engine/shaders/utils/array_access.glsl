/*****************************************************************************
* AlpineMaps.org
* Copyright (C) 2025 Adam Celarek
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


/*
  Returns the 8-bit value at 'index' from the array 'arr[ALP_UTIL_ARRAY_ACCESS_SIZE]'. #define ALP_UTIL_ARRAY_ACCESS_SIZE size before inclusion
  Each uvec4 holds 16 x 8-bit elements.
*/
highp uint get_8bit_element(in highp uvec4 arr[ALP_UTIL_ARRAY_ACCESS_SIZE], highp uint index)
{
    highp uint big_index = index >> 4u;
    highp uint sub_index = index & 15u;
    highp uint slot_index = sub_index >> 2u;
    highp uint offset_within_slot = sub_index & 3u;
    highp uint packed_val = arr[big_index][slot_index];
    highp uint shift_amount = offset_within_slot << 3u;
    return (packed_val >> shift_amount) & 0xFFu;
}

/*
  Returns the 16-bit value at 'index' from the array 'arr[ALP_UTIL_ARRAY_ACCESS_SIZE]'. #define ALP_UTIL_ARRAY_ACCESS_SIZE size before inclusion
  Each uvec4 holds 8 x 16-bit elements.
*/
highp uint get_16bit_element(in highp uvec4 arr[ALP_UTIL_ARRAY_ACCESS_SIZE], highp uint index)
{
    highp uint big_index = index >> 3u;
    highp uint sub_index = index & 7u;
    highp uint slot_index = sub_index >> 1u;
    highp uint offset_within_slot = sub_index & 1u;
    highp uint packed_val = arr[big_index][slot_index];
    highp uint shift_amount = offset_within_slot << 4u;
    return (packed_val >> shift_amount) & 0xFFFFu;
}

/*
  Returns the 32-bit value at 'index' from the array 'arr[ALP_UTIL_ARRAY_ACCESS_SIZE]'. #define ALP_UTIL_ARRAY_ACCESS_SIZE size before inclusion
  Each uvec4 holds 4 x 32-bit elements.
*/
highp uint get_32bit_element(in highp uvec4 arr[ALP_UTIL_ARRAY_ACCESS_SIZE], highp uint index)
{
    highp uint big_index = index >> 2u;
    highp uint sub_index = index & 3u;
    highp uint packed_val = arr[big_index][sub_index];
    return packed_val;
}
