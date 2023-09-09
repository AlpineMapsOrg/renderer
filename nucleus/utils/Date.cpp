#include <math.h>
#include <string.h>
#include "Date.h"

Date::Date() : year(0), month(0), day(0)
{
}

Date::Date(int y, int m, int d) : year(y), month(m), day(d)
{
}

Date::Date(const time_t& src)
{
    struct tm* buff = gmtime(&src);
    time_t t = src;

    year = buff->tm_year + 1900;
    month = buff->tm_mon + 1;
    day = buff->tm_mday;
}

Date::Date(double juliandate)
{
    double z = floor(juliandate + 0.5);
    double f = (juliandate + 0.5) - z;
    double A;
    if (z < 2299161)
    {
        A = z;
    }
    else
    {
        double alpha = floor((z - 1867216.25) / 36524.25);
        A = z + 1 + alpha - floor(alpha / 4);
    }
    double B = A + 1524;
    double C = floor((B - 122.1) / 365.25);
    double D = floor(365.25 * C);
    double E = floor((B - D) / 30.6001);
    day = B - D - floor(30.6001 * E) + f;
    month = (E < 14) ? E - 1 : E - 13;
    year = ((month > 2) ? C - 4716 : C - 4715);
}

double Date::toJulianDate() const
{
    double y = year;
    double m = month;
    double d = day;
    double A = floor(y / 100);
    double B = 2 - A + floor(A / 4);
    double JD = floor(365.25 * (y + 4716)) + floor(30.6001 * (m + 1)) + d + B - 1524.5;
    return JD;
}

time_t Date::julianDateToTimeT(double jd)
{
    return (jd - 2440587.5) * 86400.0;
}

time_t Date::ToTimeT(const Date& d, double time)
{
    struct tm tm;
    memset(&tm, 0, sizeof(struct tm));
    tm.tm_year  = d.year - 1900;
    tm.tm_mon   = d.month - 1;
    tm.tm_mday  = d.day;
    time_t date = mktime(&tm); // Make time creates the time assuming the current time, remove gmtoff later!

    int min = floor(time);
    double sec = time - floor(time);
    date += (min * 60) + (sec * 60.0);

    //date += tm.tm_gmtoff - (tm.tm_isdst * 3600);
    return date;
}
