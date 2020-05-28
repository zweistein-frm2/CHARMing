import mesytecsystem
import numpy as np
import cv2 as cv
import sys

def makeRoiWkt( parameter):

    wkt="POLYGON(("
    for xy in parameter:
        for v in xy:
            wkt+=str(v)
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
e=makeRoiWkt(detsize)


print(t[0])
histogram = t[1]
print(histogram)
img = cv.normalize(histogram, None, 0,255, norm_type=cv.NORM_MINMAX, dtype=cv.CV_8U)
img=cv.applyColorMap(img,cv.COLORMAP_JET)
cv.imshow("Window", img)
cv.waitKey(0)
cv.destroyAllWindows()






