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

# useful for mixed mode debugging (Python and C++)
#import debugpy
#debugpy.listen(('0.0.0.0',5678))
#debugpy.wait_for_client()
#breakpoint()

import sys
import time
import threading
import numpy as np
import cv2 as cv


import mesytecsystem

assert sys.version_info >= (3, 4)



seconds = 20
# pylint: disable=unused-variable
# pylint: disable=redefined-outer-name
NM = None
def initMeasurement():
    global NM, seconds
    #breakpoint()
    NM = mesytecsystem.NeutronMeasurement(sys.stdout.fileno())
    v = NM.version
    print(v)
    NM.on()
    time.sleep(4)
    NM.simulatorRate = 25
    NM.start()
    for i in range(0, seconds):
        time.sleep(1)


t = threading.Thread(target=initMeasurement, args=())
t.start()

time.sleep(7) #should be connected by now


h = NM.getHistogram()
mat = np.zeros((1, 1, 1), dtype="int32")

for i in range(0, seconds):
    t = h.update(mat)
    print("h.Size="+str(h.Size))
    print("h.getRoi(0)="+str(h.getRoi(0)))
    print(t[0])
    histogram = t[1]
    #print(histogram)
    img = cv.normalize(np.int32(histogram), None, 0, 255, norm_type=cv.NORM_MINMAX, dtype=cv.CV_8U)

    img = cv.resize(img, (256, 256))
    img = cv.applyColorMap(img, cv.COLORMAP_JET)

    if i == 0:
        cv.namedWindow("Output", cv.WINDOW_NORMAL)
    cv.imshow("Output", img)
    cv.waitKey(1000)


cv.destroyAllWindows()
NM.stop()
NM = None
