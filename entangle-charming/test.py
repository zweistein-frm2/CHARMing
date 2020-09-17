#!/usr/bin/env python
#                           _              _                _
#	___  __ __ __  ___     (_)     ___    | |_     ___     (_)    _ _
#   |_ /  \ V  V / / -_)    | |    (_-<    |  _|   / -_)    | |   | ' \
#  _/__|   \_/\_/  \___|   _|_|_   /__/_   _\__|   \___|   _|_|_  |_||_|
#	   .
#	   |\       Copyright (C) 2019 - 2020 by Andreas Langhoff
#	 _/]_\_                            <andreas.langhoff@frm2.tum.de>
# ~~~"~~~~~^~~   This program is free software; you can redistribute it
# and/or modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation;

import mesytecsystem
import numpy as np
import cv2 as cv
import sys

def makeRoiWkt( parameter):

    wkt="POLYGON(("
    for xy in parameter:
        for ve in xy:
            wkt+=str(ve)
            wkt+=" "
        wkt+=","
    wkt=wkt[:-1]
    wkt+="),())"

o = mesytecsystem.NeutronMeasurement(sys.stdout.fileno())
v = o.version
print(v)
o.start()
h = o.getHistogram()
mat = np.zeros((1,1,1),dtype="int32")
t= h.update(mat)
print("h.Size="+str(h.Size))
print("h.getRoi(0)="+str(h.getRoi(0)))
detsize=[0,0],[64,1024]
makeRoiWkt(detsize)


print(t[0])
histogram = t[1]
print(histogram)
img = cv.normalize(histogram, None, 0,255, norm_type=cv.NORM_MINMAX, dtype=cv.CV_8U)
img=cv.applyColorMap(img,cv.COLORMAP_JET)
cv.imshow("Window", img)
cv.waitKey(0)
cv.destroyAllWindows()






