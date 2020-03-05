from entangle.device.charm import numpycpp
import numpy as np
import cv2 as cv

from entangle import base
from entangle.core import Prop, uint16
from entangle.core.states import BUSY, UNKNOWN
from entangle.core.errors import InvalidValue, InvalidOperation, \
    ConfigurationError

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
        t=self.Histogram.update(self.mat)
        self.count=t[0];
        return self.mat.flatten()

    def Start(self):
        o.start()

    def Stop(self):
        o.stop()
