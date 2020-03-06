from entangle import base
from entangle.core import Prop, uint16
from entangle.core.states import BUSY, UNKNOWN
from entangle.core.errors import InvalidValue, InvalidOperation, \
    ConfigurationError

import numpy as np
import cv2 as cv

from entangle.device.charm import numpycpp

o = numpycpp.NeutronMeasurement()

class Histogram(base.ImageChannel):

    def init(self):
        self.count = 0;
        self.Roi = ""
        self.mat = np.zeros((1,1,1),dtype="int32")
        self.Histogram = o.getHistogram()

    def Clear(self):
        return
    def read_detectorSize(self):
        return self.Histogram.Size
    def read_value(self):
        print("read_value")
        t=self.Histogram.update(self.mat)
        self.mat=t[1]
        print(self.mat.shape)
        self.count=t[0];
        return self.mat.flatten()

    def Start(self):
        o.start()

    def Stop(self):
        o.stop()
