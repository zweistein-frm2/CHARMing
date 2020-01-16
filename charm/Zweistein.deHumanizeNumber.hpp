/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/

#pragma once
#include <iostream>
#include <iomanip>
#include <cctype>
#include <string>
#include <sstream>
namespace Zweistein {
    long long Dehumanize(std::string& s) {

        long long ret = 0;
        bool isscientific = false;
        long long ibefore = 0;
        long long iafter = 0;
        char Abbr[] = { 'K','M','G','T' };
        // there may be only 1 Abbr char in a string and no other chars
        // after the Abbr char there may be at max 3 digits

        int isafter = -1;
        int pos = 0;
        long long factor = 1;
        for (const char c : s) {
            if (isafter >= 0) {
                if (!std::isdigit(c)) {
                    std::string se = "Unexpected char ";
                    se += s.substr(0, pos);
                    se += "->";
                    se += c;
                    se += "<-";
                    se += s.substr(pos + 1);
                    throw std::invalid_argument(se); // after no non numbers allowed
                }
            }
            long long fac = 1;
            for (const char& a : Abbr) {
                fac *= 1000;
                if (std::tolower(a) == std::tolower(c)) {
                    isafter = 0;
                    ibefore = ibefore * fac;
                    factor = fac;
                }
            }
            if (isafter < 0) {
                if (std::isdigit(c)) ibefore = 10LL * ibefore + (c - 48);
                else {
                    if (isscientific == false && std::tolower('E') == std::tolower(c)) {
                        isafter = 0;
                        isscientific = true;
                    }
                    else {
                        std::string se = "Unrecognized char ";
                        se += s.substr(0, pos);
                        se += "->";
                        se += c;
                        se += "<-";
                        se += s.substr(pos + 1);
                        throw std::invalid_argument(se); // after no non numbers allowed
                    }
                }
            }
            else {
                // after Abbr
                if (std::isdigit(c)) {
                    int fac2 = factor / 1000;
                    for (int j = 2 - isafter; j > 0; j--) fac2 *= 10;
                    if (isscientific) {
                        iafter = 10 * iafter + (c - 48);
                    }
                    else iafter = fac2 * (c - 48) + iafter;
                    isafter++;
                    if (isafter > 2) {
                        std::string se = "Inconsistent use of ";
                        se += s.substr(0, pos);
                        se += "->";
                        se += c;
                        se += "<-";
                        se += s.substr(pos + 1);
                        throw std::invalid_argument(se);
                    }
                }
            }
            pos++;
        }
        if (isscientific) {
            long exp = 1;
            for (int i = 0; i < iafter; i++) exp *= 10;
            return ibefore * exp;
        }
        return ibefore + iafter;
    }

}