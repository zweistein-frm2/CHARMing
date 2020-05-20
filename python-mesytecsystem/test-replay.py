import listmodereplay
import numpy as np
import cv2 as cv
import sys
import time

def makeRoiWkt( parameter):

    wkt="POLYGON(("
    for xy in parameter:
        for v in xy:
            wkt+=str(v)
            wkt+=" "
        wkt+=","
    wkt=wkt[:-1]
    wkt+="),())"

o = listmodereplay.ReplayList(sys.stdout.fileno())
files = o.files('~')
print("Number of files in PlayList:"+str(len(files)))

o.addfile(files[0])
#input("start now")
o.start()

time.sleep(60)

#print(files)
#for file in files:
#o.addfile(files[0])
#o.addfile(files[1])
#o.start()
#time.sleep(25)


#h = o.getHistogram()
#mat = np.zeros((1,1,1),dtype="int32")
#t= h.update(mat)
#print(h.getRoi(0))
#detsize=[0,0],[64,1024]
#e=makeRoiWkt(detsize)


#print("polygon/count="+str(t[0]))
#histogram = t[1]
#print(histogram)
#with np.printoptions(threshold=np.inf):
#   print(histogram)
#minVal, maxVal, minLoc, maxLoc=cv.minMaxLoc(histogram)
#print(minVal,maxVal,minLoc,maxLoc)
#img= (histogram*(255.0/maxVal)).astype(np.uint8)
#img = cv.normalize(histogram, None, 0,255, norm_type=cv.NORM_MINMAX, dtype=cv.CV_8U)
#img=cv.applyColorMap(img,cv.COLORMAP_JET)
#cv.imshow("Window", img)
#cv.waitKey(0)
#cv.destroyAllWindows()






