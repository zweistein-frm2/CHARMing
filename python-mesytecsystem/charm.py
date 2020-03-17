from entangle import base
from entangle.core import states, Prop, uint16, Attr,Cmd
from entangle.core.defs import uint64
from entangle.core.states import BUSY, UNKNOWN
from entangle.core.errors import InvalidValue, InvalidOperation, \
    ConfigurationError

import numpy as np
import cv2 as cv

from entangle.device.charm import mesytecsystem

__mesytecsystem = None
def getMesytecSystem():
    global __mesytecsystem
    if __mesytecsystem is None:
        __mesytecsystem = mesytecsystem.NeutronMeasurement()
    return __mesytecsystem




class MeasureCounts(base.CounterChannel):
    
   
    def init(self):
        self.o=getMesytecSystem()
    
        
    def Start(self):
        self.o.start()

    def Stop(self):
        self.o.stop()
    
    def Prepare(self):
        pass
    def Resume(self):
        self.o.start()
    def read_value(self):
        t=self.o.status()
        return t[0]
    def write_preselection(self, value):
        t=self.o.status()
        self.o.stopafter(value,t[1])
        


class MeasureTime(base.TimerChannel):
      
    def init(self):
        self.o=getMesytecSystem()
    
    def Start(self):
        self.o.start()

    def Stop(self):
        self.o.stop()
    
    def Prepare(self):
        pass
    def Resume(self):
        self.o.start()
    def read_value(self):
        t=self.o.status()
        return t[1]
    def write_preselection(self, value):
        t=self.o.status()
        self.o.stopafter(t[0],value)



class Histogram(base.ImageChannel):

    attributes = {
        'CountsInRoi': Attr(uint64, 'counts in Roi',dims=0,writable=False,memorized=False,unit=None),
         'RoiWKT':   
            Attr(str,'Well known text (WKT) representation of the region of interest.',
                 writable=True, memorized=True,
                 disallowed_read=(states.BUSY, states.INIT, states.UNKNOWN,),
                 disallowed_write=(states.FAULT, states.OFF, states.BUSY,
                                   states.INIT, states.UNKNOWN,)),
    }

    def init(self):
        self.count = 0;
        self.mat = np.zeros((1,1,1),dtype="int32")
        self.Histogram = getMesytecSystem().getHistogram()

    def read_CountsInRoi(self):
        return uint64(self.count)
    def get_CountsInRoi_unit(self):
        return 'cts'
    def Clear(self):
        return
    def read_detectorSize(self):
        return self.Histogram.Size
    def read_RoiWKT(self):
        return self.Histogram.Roi
    def write_RoiWKT(self,value):
        self.Histogram.Roi=value
    def get_RoiWKT_unit(self):
        return ''
    def read_value(self):
        t=self.Histogram.update(self.mat)
        self.mat=t[1]
        #print(self.mat.shape)
        self.count=t[0];
        return self.mat.flatten()

