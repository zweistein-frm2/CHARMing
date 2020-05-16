/***************************************************************************
 *   Copyright (C) 2019 by Andreas Langhoff <andreas.langhoff@frm2.tum.de> *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation;                                         *
 ***************************************************************************/
#pragma once
#include "Zweistein.Binning.hpp"
#include <opencv2/core.hpp>
#include <opencv2/core/mat.hpp>

namespace Zweistein {
    namespace Binning{
    void Apply_OCC_Correction(cv::Mat mat, polygon_type& roi,unsigned long &count) {
        auto s = Zweistein::Binning::OCC.shape();
        for (int r = 0; r < s[1]; r++) {
            for (int c = 0; c < s[0]; c++) {
                short occ = Zweistein::Binning::OCC[c][r];
                if (occ <= 0) continue;
                if (occ == Zweistein::Binning::occmultiplier) continue; //  occ == 1
                if (occ < -Zweistein::Binning::occmultiplier || occ > Zweistein::Binning::occmultiplier) {
                    LOG_ERROR << "occ=" << occ << ", " << " [" << c << "," << r << "]" << std::endl;
                    continue;
                }
                if (r < s[1] - 1) {
                    // occ is stored as short, and float 0.94 is mapped to 9400 (=> multiplier 10000)
                        //point_type pb0(c, r);
                        //unsigned long nextpixelcount=histograms[0].histogram.at<int32_t>(p0.y(), p0.x());
                    short binnedY = Zweistein::Binning::BINNING[c][r];
                    //	LOG_DEBUG << "occ=" << occ << ", binnedY=" << binnedY << " [" << c << "," << r << "]" << std::endl;
                    if (binnedY < 0) continue;
                    point_type pb(c, binnedY);
                    if (pb.x() >= mat.cols) {
                        LOG_ERROR << pb.x() << " >= binned_occ_corrected.cols" << std::endl;
                        continue;
                    }
                    if (pb.y() >= mat.rows) {
                        LOG_ERROR << pb.y() << " >= binned_occ_corrected.rows" << std::endl;
                        continue;
                    }
                    unsigned long nextpixelcount = mat.at<int32_t>(pb.y(), pb.x()) * (Zweistein::Binning::occmultiplier - occ) / Zweistein::Binning::occmultiplier;
                    if (nextpixelcount != 0) {
                        //     LOG_DEBUG << "pixel corrected:" << pb.x() << ", " << pb.y() << " count -=" << nextpixelcount << std::endl;
                    }

                    mat.at<int32_t>(pb.y(), pb.x()) -= nextpixelcount;
                    point_type pbNext(c, binnedY + 1);
                    mat.at<int32_t>(pbNext.y(), pbNext.x()) += nextpixelcount;

                    bool origInside = false;
                    bool nextInside = false;

                    if (boost::geometry::covered_by(pb, roi)) {
                        origInside = true;
                    }
                    if (boost::geometry::covered_by(pbNext, roi)) {
                        nextInside = true;
                    }

                    if (origInside != nextInside) {
                        // we have to correct counts:
                        if (origInside) count -= nextpixelcount; // so next Y  is outside counting area, so we have to correct origY counts
                        if (nextInside) count += nextpixelcount; // original was not counted since outside
                    }


                }

            }
        }
    }
}
}