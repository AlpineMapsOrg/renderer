#include <stdio.h>

#include <math.h>
#include <time.h>
#include <string.h>
#include <iostream>

#include "NoaaSolarCalculator.h"

const float  SunCalc::sunrise          = 0.833;
const float  SunCalc::sunriseEnd       = 0.3;
const float  SunCalc::twilight         = 6.0;
const float  SunCalc::nauticalTwilight = 12.0;
const float  SunCalc::night            = 18.0;
const float  SunCalc::goldenHour       = -6.0;
const double SunCalc::PI               = 3.141592653589793;


SunCalc::SunCalc(const Date &date, double latitude, double longitude)
{
    _date = date;
    _latitude = latitude;
    _longitude = longitude;
    _julianDate = date.toJulianDate();
}

double SunCalc::timeAtAngle(float angle, bool rising)
{
    return calcSunriseSet(rising, angle, _julianDate, _date, _latitude, _longitude);
}

// rise = true for sunrise, false for sunset
time_t SunCalc::calcSunriseSet(bool rise, float angle, double JD, Date date, double latitude, double longitude)
{
    double timeUTC = calcSunriseSetUTC(rise, angle, JD, latitude, longitude);
    double newTimeUTC = calcSunriseSetUTC(rise, angle, JD + timeUTC / 1440.0, latitude, longitude);

    if (!isnan(newTimeUTC))
    {
        return Date::ToTimeT(_date, newTimeUTC);
    }
    // no sunrise/set found
    double doy = calcDoyFromJD(JD);
    double jdy;
    if (( (latitude >  66.4) &&  (doy > 79) && (doy < 267) ) ||
        ( (latitude < -66.4) && ((doy < 83) || (doy > 263))))
    {
        //previous sunrise/next sunset
        jdy = calcJDofNextPrevRiseSet(!rise, rise, angle, timeUTC);
    }
    else
    {
        //previous sunset/next sunrise
        jdy = calcJDofNextPrevRiseSet(rise, rise, angle, timeUTC);
    }
    timeUTC = calcSunriseSetUTC(rise, angle, jdy, latitude, longitude);
    newTimeUTC = calcSunriseSetUTC(rise, angle, jdy + timeUTC / 1440.0, latitude, longitude);
    return Date::ToTimeT(Date(jdy), newTimeUTC);
    //return JdToDate(jdy);
}

double SunCalc::calcSunriseSetUTC(bool rise, float angle, double JD, double latitude, double longitude)
{
    double t = calcTimeJulianCent(JD);
    double eqTime = calcEquationOfTime(t);
    double solarDec = calcSunDeclination(t);
    double hourAngle = calcHourAngle(angle, latitude, solarDec);
    //alert("HA = " + radToDeg(hourAngle));
    if (!rise)
        hourAngle = -hourAngle;
    double delta = longitude + radToDeg(hourAngle);
    double timeUTC = 720.0 - (4.0 * delta) - eqTime; // in minutes
    return timeUTC;
}

double SunCalc::calcTimeJulianCent(double jd)
{
    double d = (jd - 2451545.0);
    double T = d / 36525.0;
    return T;
}

double SunCalc::calcEquationOfTime(double t)
{
    double epsilon = calcObliquityCorrection(t);
    double l0 = calcGeomMeanLongSun(t);
    double e = calcEccentricityEarthOrbit(t);
    double m = calcGeomMeanAnomalySun(t);
    double tmp1 = degToRad(epsilon);
    double y = tan(tmp1 / 2.0);
    y *= y;

    double t_l0 = degToRad(l0);
    double t_m = degToRad(m);

    double sin2l0 = sin(2.0 * t_l0);
    double cos2l0 = cos(2.0 * t_l0);

    double sinm = sin(t_m);

    double tmp3 = 4.0 * t_l0;
    double sin4l0 = sin(tmp3);
    double sin2m = sin(2.0 * t_m);

    double Etime = y * sin2l0 - 2.0 * e * sinm + 4.0 * e * y * sinm * cos2l0 - 0.5 * y * y * sin4l0 - 1.25 * e * e * sin2m;
    return radToDeg(Etime) * 4.0; // in minutes of time
}

double SunCalc::calcSunDeclination(double t)
{
    double e = calcObliquityCorrection(t);
    double lambda = calcSunApparentLong(t);

    double sint = sin(degToRad(e)) * sin(degToRad(lambda));
    double theta = radToDeg(asin(sint));
    return theta; // in degrees
}

double SunCalc::calcHourAngle(float angle, double lat, double solarDec)
{
    double latRad = degToRad(lat);
    double sdRad = degToRad(solarDec);
    double HAarg = (cos(degToRad(90.0 + angle)) / (cos(latRad) * cos(sdRad)) - tan(latRad) * tan(sdRad));
    double HA = acos(HAarg);
    return HA; // in radians (for sunset, use -HA)
}

double SunCalc::calcObliquityCorrection(double t)
{
    double e0 = calcMeanObliquityOfEcliptic(t);
    double omega = 125.04 - 1934.136 * t;
    double e = e0 + 0.00256 * cos(degToRad(omega));
    return e; // in degrees
}

double SunCalc::calcGeomMeanLongSun(double t)
{
    double L0 = 280.46646 + t * (36000.76983 + t * (0.0003032));
    while (L0 > 360.0)
    {
        L0 -= 360.0;
    }
    while (L0 < 0.0)
    {
        L0 += 360.0;
    }
    return L0; // in degrees
}
double SunCalc::calcEccentricityEarthOrbit(double t)
{
    double e = 0.016708634 - t * (0.000042037 + 0.0000001267 * t);
    return e; // unitless
}

double SunCalc::calcGeomMeanAnomalySun(double t)
{
    double M = 357.52911 + t * (35999.05029 - 0.0001537 * t);
    return M; // in degrees
}

double SunCalc::radToDeg(double angleRad)
{
    return (180.0 * angleRad / PI);
}

double SunCalc::degToRad(double angleDeg)
{
    return (PI * angleDeg / 180.0);
}
double SunCalc::calcSunApparentLong(double t)
{
    double o = calcSunTrueLong(t);
    double omega = 125.04 - 1934.136 * t;
    double lambda = o - 0.00569 - 0.00478 * sin(degToRad(omega));
    return lambda; // in degrees
}

double SunCalc::calcMeanObliquityOfEcliptic(double t)
{
    double seconds = 21.448 - t * (46.8150 + t * (0.00059 - t * (0.001813)));
    double e0 = 23.0 + (26.0 + (seconds / 60.0)) / 60.0;
    return e0; // in degrees
}

double SunCalc::calcSunTrueLong(double t)
{
    double l0 = calcGeomMeanLongSun(t);
    double c = calcSunEqOfCenter(t);
    double O = l0 + c;
    return O; // in degrees
}

double SunCalc::calcSunEqOfCenter(double t)
{
    double m = calcGeomMeanAnomalySun(t);
    double mrad = degToRad(m);
    double sinm = sin(mrad);
    double sin2m = sin(mrad + mrad);
    double sin3m = sin(mrad + mrad + mrad);
    double C = sinm * (1.914602 - t * (0.004817 + 0.000014 * t)) + sin2m * (0.019993 - 0.000101 * t) + sin3m * 0.000289;
    return C; // in degrees
}

double SunCalc::calcDoyFromJD(double jd)
{
    double z = floor(jd + 0.5);
    double f = (jd + 0.5) - z;
    double A;
    if (z < 2299161)
    {
        A = z;
    }
    else
    {
        double alpha = floor((z - 1867216.25) / 36524.25);
        A = z + 1 + alpha - floor(alpha / 4.0);
    }
    double B = A + 1524.0;
    int C = floor((B - 122.1) / 365.25);
    double D = floor(365.25 * C);
    double E = floor((B - D) / 30.6001);
    double day = B - D - floor(30.6001 * E) + f;
    double month = (E < 14) ? E - 1 : E - 13;
    int year = (month > 2) ? C - 4716.0 : C - 4715.0;

    double k = (isLeapYear(year) ? 1.0 : 2.0);
    double doy = floor((275 * month) / 9) - k * floor((month + 9) / 12) + day - 30;
    return doy;
}

bool SunCalc::isLeapYear(int yr)
{
    return ((yr % 4 == 0 && yr % 100 != 0) || yr % 400 == 0);
}

double SunCalc::calcJDofNextPrevRiseSet(bool next, bool rise, float type, double time)
{
    double julianday = _julianDate;
    double increment = ((next) ? 1.0 : -1.0);
    while (isnan(time))
    {
        julianday += increment;
        time = calcSunriseSetUTC(rise, type, julianday, _latitude, _longitude);
    }
    return julianday;
}
