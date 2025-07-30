/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Joerg Christian Reiher
 * Copyright (C) 2024 Johannes Eschner
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

layout (std140) uniform eaws_reports {
    // length of array must be the same as in nucleus::avalanche::uboEawsReports on host side
    ivec4 reports[1000];
} eaws;

// Color for areas where no report is available
vec3 color_no_report_available = vec3(.5f,.5f,.5f);

vec3 color_from_eaws_danger_rating(int rating)
{
    if(1 == rating) return vec3(0.f,1.f,0.f);      // green        for 1 = low
    if(2 == rating) return vec3(1.f,1.f,0.f);      // yellow       for 2 = moderate
    if(3 == rating) return vec3(1.f,0.53f,0.f);    // orange       for 3 = considerable
    if(4 == rating) return vec3(1.f,0.f,0.f);      // red          for 4 = high
    if(5 == rating) return vec3(0.5333f,0.f,0.f);  // dark red     for 5 = extreme
    return(color_no_report_available);             // grey         for undefined cases
}

vec3 snowCardLevel[6] = vec3[6](
    vec3(1.f    , 1.f   , 1.f   ),   // level 0 = white
    vec3(0.9961 , 0.8000, 0.3608),   // level 1 = yellow
    vec3(0.9922 , 0.5530, 0.2353),   // level 2 = orange
    vec3(0.9412 , 0.2314, 0.1255),   // level 3 = red
    vec3(0.4588 , 0.0510, 0.1333),   // level 4 = dark red
    vec3(0.f    ,0.f    ,0.f    )    // level 5 = black
);


vec3 slopeAngleColorFromNormal(vec3 notNormalizedNormal)
{
    // Calculte slope angle
    vec3 normal = normalize(notNormalizedNormal);
    float slope_in_rad = acos(normal.z);
    float slope_in_deg = degrees(slope_in_rad);

    // Get color for slope angle
    vec3 slopeColor;
    if(slope_in_deg < 30) // white
        slopeColor= vec3(1,1,1);
    else if (30 <= slope_in_deg && slope_in_deg < 35) //yellow
        slopeColor = vec3(0.9490196078431372f, 0.8980392156862745f, 0.0392156862745098f);
    else if (35 <= slope_in_deg && slope_in_deg < 40) //orange
        slopeColor = vec3(0.95686274f, 0.43529411764705883f,0.1411764705882353f);
    else if (40 <= slope_in_deg && slope_in_deg < 45) //red
        slopeColor = vec3(0.8705882352941177f, 0.0196078431372549f, 0.3568627450980392f);
    else // purple if > 45
        slopeColor = vec3(0.7843137254901961f, 0.5372549019607843f, 0.7333333333333333f);

    // Return color
    return slopeColor;
}

// SnowCard Risk overlay:
// adapts eaws rating according to slope angle and favorable/unfavorable position
// E.g. favorable1[0] contains snowcard rating for ewas rating level 1 at favorable position for 27deg slope angle,
// favorable1[1]for eaws rating 1 at 28deg etc. up to 45deg
// --------------------------------------------
//                              27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45
int   favorable1[19] =  int[19]( 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 5); // 1 = danger rating, favorable
int   favorable2[19] =  int[19]( 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 5); // 2 = danger rating, favorable
int   favorable3[19] =  int[19]( 0, 0, 0, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 4, 4, 5); // 3 = danger rating, favorable
int   favorable4[19] =  int[19]( 1, 2, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5); // 4 = danger rating, favorable
int   favorable5[19] =  int[19]( 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5); // 5 = danger rating, favorable
int unfavorable1[19] =  int[19]( 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 5); // 1 = danger rating, unfavorable
int unfavorable2[19] =  int[19]( 0, 0, 0, 1, 1, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 5); // 2 = danger rating, unfavorable
int unfavorable3[19] =  int[19]( 0, 0, 0, 1, 2, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5); // 3 = danger rating, unfavorable
int unfavorable4[19] =  int[19]( 1, 2, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5); // 4 = danger rating, unfavorable
int unfavorable5[19] =  int[19]( 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5); // 5 = danger rating, unfavorable

// returns warning level according to SnowCard for a given eaws reprot on a slope angle at favorable/unfavorable exposition
// returns 0 if exposition contains something else than 1(favorable) or -1(unfavorable)
vec3 color_from_snowCard_risk_parameters(int eaws_danger_rating, vec3 notNormalizedNormal, bool unfavorable)
{
    // Calculate slope angle and return black if steeper than 45 deg
    vec3 normal = normalize(notNormalizedNormal);
    float slope_in_rad = acos(normal.z);
    int slopeAngleAsInt = int(degrees(slope_in_rad));

    // Truncate slope angle and calculate corresponding index for accessing array with danger ratings
    int angle = min(max(slopeAngleAsInt,27), 45); // angle below 27deg is treated like 27deg (same for above 45deg)
    int idx = angle-27; // array[0] contains rating for 27 degree, see above

    // Calculate mixing factor for smooth transition over borders
    float a = degrees(slope_in_rad) - float(slopeAngleAsInt);

    //Avoid out index overflow
    int nextIdx = min(idx+1,18);
    if(true == unfavorable)
    {
        // pick unfavorable array according to eaws danger rating
        switch(eaws_danger_rating)
        {
            case 1: return (1-a) * snowCardLevel[unfavorable1[idx]] + a * snowCardLevel[unfavorable1[nextIdx]];
            case 2: return (1-a) * snowCardLevel[unfavorable2[idx]] + a * snowCardLevel[unfavorable2[nextIdx]];
            case 3: return (1-a) * snowCardLevel[unfavorable3[idx]] + a * snowCardLevel[unfavorable3[nextIdx]];
            case 4: return (1-a) * snowCardLevel[unfavorable4[idx]] + a * snowCardLevel[unfavorable4[nextIdx]];
            case 5: return (1-a) * snowCardLevel[unfavorable5[idx]] + a * snowCardLevel[unfavorable5[nextIdx]];
        }
    }
    else // favorable direction
    {
        // pick favorable array according to eaws danger rating
        switch(eaws_danger_rating)
        {
            case 1: return (1-a) * snowCardLevel[favorable1[idx]] + a * snowCardLevel[favorable1[nextIdx]];
            case 2: return (1-a) * snowCardLevel[favorable2[idx]] + a * snowCardLevel[favorable2[nextIdx]];
            case 3: return (1-a) * snowCardLevel[favorable3[idx]] + a * snowCardLevel[favorable3[nextIdx]];
            case 4: return (1-a) * snowCardLevel[favorable4[idx]] + a * snowCardLevel[favorable4[nextIdx]];
            case 5: return (1-a) * snowCardLevel[favorable5[idx]] + a * snowCardLevel[favorable5[nextIdx]];
        }
    }
}


// Colors for stop or go map
bool go0to30[5]  = bool[5](true, true,  true,  true,  false);
bool go30to35[5] = bool[5](true, true,  true,  false, false);
bool go35to40[5] = bool[5](true, true,  false, false, false);
bool goOver40[5] = bool[5](true, false, false, false, false);
vec3 color_from_stop_or_go(vec3 notNormalizedNormal, int eaws_danger_rating)
{
    // danger rating must be in [1,5]
    if(eaws_danger_rating < 1 || 5 < eaws_danger_rating) return vec3(0,0,0);
    int idx = eaws_danger_rating -1;

    // Calculte slope angle
    vec3 normal = normalize(notNormalizedNormal);
    float slope_in_rad = acos(normal.z);
    float slope_in_deg = degrees(slope_in_rad);
    bool go = false;
    if(slope_in_deg <= 30) go = go0to30[idx];
    else if (slope_in_deg <= 35) go = go30to35[idx];
    else if (slope_in_deg <= 40) go = go35to40[idx];
    else go = goOver40[idx];

    if(go) return vec3(0,0,0); // GO : retunr 0 0 0 so overlay is trnasparent
    return vec3(1.f,0.f,0.f); // STOP: return red;
}

// converts a 3d normal vector into a bit encoded direction N, NE, E etc
// n must be a unitvector !!!
int direction(vec3 n)
{
    //Ensure n has length = 1
    if(n.x*n.x +n.y*n.y +n.z*n.z >1.0f) normalize(n);

    // calculate direction of fragment (North, South etc
    float angle = sign(n.y)*degrees(acos(n.x));
    if(112.5 <= angle && angle < 157.5) return 1;                       // Encodes NW = 00000001
    else if(157.5 <= abs(angle) && abs(angle) <=180)    return (1<<1);  // Encodes  W = 00000010
    else if(-157.5 <= angle && angle < -112.5)          return (1<<2);  // Encodes SW = 00000100
    else if(-112.5 <= angle && angle < -67.5)           return (1<<3);  // Encodes  S = 00001000
    else if(-112.5 <= angle && angle < -67.5)           return (1<<4);  // Encodes SE = 00010000
    else if(0 <= abs(angle) && abs(angle) < 22.5)       return (1<<5);  // Encodes  E = 00100000
    else if(22.5 <= angle && angle < 67.5)              return (1<<6);  // Encode  NE = 01000000
    else                                                return (1<<7);  // Enncodes N = 10000000
}



