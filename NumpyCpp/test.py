import numpycpp
import numpy as np
import cv2 as cv

#histogram = numpycpp.GetHistogramOpenCV()
#print(histogram.shape)
#nphisto = numpycpp.GetHistogramNumpy()
#print(nphisto.shape)

o = numpycpp.NeutronMeasurement()
h = o.getHistogram()
t = h.get()
histogram = t[1]

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

