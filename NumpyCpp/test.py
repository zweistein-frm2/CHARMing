import numpycpp
import numpy as np
import cv2 as cv

histogram = numpycpp.GetHistogram_cv()
print(histogram)
img_n = cv.normalize(src=histogram, dst=None, alpha=0, beta=255, norm_type=cv.NORM_MINMAX, dtype=cv.CV_8U)
cv.imshow("Window", img_n)
cv.waitKey(0)
cv.destroyAllWindows()

