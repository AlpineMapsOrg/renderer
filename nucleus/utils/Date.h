#pragma once
#define _GNU_SOURCE /* for tm_gmtoff and tm_zone */
#include <time.h>

class Date
{
public:
    int year;  //!< Year in format YYYY e.g 2000
    int month; //!< Month of the year 1-12.
    int day;   //!< Day of the month 1-32.
    Date();
    Date(int year, int month, int day);
    Date(double); //!< Juliandate
    Date(const time_t& time);
    double toJulianDate() const;
    static time_t julianDateToTimeT(double jd); //!< Converst julian date to time_t
    static time_t ToTimeT(const Date& d, double time); //!< Combine julian timestamp and date to create a new time_t
};
