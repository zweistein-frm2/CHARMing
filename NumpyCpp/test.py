import numpycpp
import numpy as np
import cv2 as cv


def makeRoiWkt( parameter):

    wkt="POLYGON(("
    for xy in parameter:
        for v in xy:
            wkt+=str(v)
            wkt+=" "
        wkt+=","
    wkt=wkt[:-1]
    wkt+="),())"

o = numpycpp.NeutronMeasurement()
h = o.getHistogram()
mat = np.zeros((1,1,1),dtype="int32")
t= h.update(mat)
print(h.Roi)
detsize=[0,0],[64,1024]
e=makeRoiWkt(detsize)


print("count="+str(t[0]))
histogram = t[1]
print(histogram)
#with np.printoptions(threshold=np.inf):
#   print(histogram)
#minVal, maxVal, minLoc, maxLoc=cv.minMaxLoc(histogram)
#print(minVal,maxVal,minLoc,maxLoc)
#img= (histogram*(255.0/maxVal)).astype(np.uint8)
img = cv.normalize(histogram, None, 0,255, norm_type=cv.NORM_MINMAX, dtype=cv.CV_8U)
img=cv.applyColorMap(img,cv.COLORMAP_JET)
cv.imshow("Window", img)
cv.waitKey(0)
cv.destroyAllWindows()






