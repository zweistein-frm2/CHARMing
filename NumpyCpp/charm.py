from entangle.device.charm import numpycpp
import numpy as np
import cv2 as cv

from entangle import base
from entangle.core import Prop, uint16
from entangle.core.states import BUSY, UNKNOWN
from entangle.core.errors import InvalidValue, InvalidOperation, \
    ConfigurationError

Measurement = numpycpp.NeutronMeasurement()

class Histogram(base.ImageChannel):

    def init(self):
        self.histogram = Measurement.getHistogram()

    def Clear(self):
        return
    def read_detectorSize(self):
        return [64,1024]
    def read_value(self):
        return self.histogram.get()[1].flatten()

    def Start(self):
        Measurement.start()

    def Stop(self):
        Measurement.stop()
