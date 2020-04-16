from entangle import base
from entangle.core import states, Prop, uint16, Attr,Cmd
from entangle.core.defs import uint64, int32, boolean
from entangle.core.states import BUSY, UNKNOWN,FAULT
from entangle.core.errors import InvalidValue, InvalidOperation, \
    ConfigurationError
from entangle.lib.loggers import FdLogMixin
import signal, os
import numpy as np
import cv2 as cv

from entangle.device.charm import mesytecsystem

charmsystem = None

class DeviceConnection(FdLogMixin,base.MLZDevice):
    def init(self):
        self.init_fd_log('Charm')
        fd=self.get_log_fd()
        print("charm.py:DeviceConnection.init("+str(fd)+")")
        global charmsystem
        if charmsystem is None:
                charmsystem=mesytecsystem.NeutronMeasurement(fd)
       
    def __del__(self):
        print("charm.py: DeviceConnection.__del__")


class Simulator(base.MLZDevice):


    attributes = {
        'NucleoRate':   
            Attr(int32,'simulator data rate',
                 writable=True,memorized=False,  disallowed_read=(states.INIT, states.UNKNOWN,),
                 disallowed_write=( states.OFF, states.INIT, states.UNKNOWN,)),
    }

    def read_NucleoRate(self):
        global charmsystem
        if charmsystem:
            return charmsystem.simulatorRate
    def write_NucleoRate(self,value):
        global charmsystem
        if charmsystem:
            charmsystem.simulatorRate=value

    def get_NucleoRate_unit(self):
        return 'Events/second'



class Settings(base.MLZDevice):
    attributes = {
        'writelistmode':   
            Attr(boolean,'write mesytec listmode to file',
                 writable=True,memorized=False,  disallowed_read=(states.INIT, states.UNKNOWN,),
                 disallowed_write=( states.OFF, states.INIT, states.UNKNOWN,)),
    }

    def read_writelistmode(self):
        global charmsystem
        if charmsystem:
            return charmsystem.writelistmode
    def write_writelistmode(self,value):
        global charmsystem
        if charmsystem:
            charmsystem.writelistmode=value

    def get_writelistmode_unit(self):
        return ''



class MeasureCounts(base.CounterChannel):
    
   
   
    def state(self):
        global charmsystem
        if charmsystem:
            t=charmsystem.status()
            if not(t[2] &2): return (FAULT,t[3])
            if(t[2] & 1): return (BUSY,t[3])
            _state= base.ImageChannel.state(self)
            return (_state[0],t[3])
    
    def Start(self):
        global charmsystem
        if charmsystem:
            charmsystem.start()
    
    def Stop(self):
        global charmsystem
        if charmsystem:
            charmsystem.stop()
    
    def Prepare(self):
        pass
    def Resume(self):
        global charmsystem
        if charmsystem:
            charmsystem.resume()
    def read_value(self):
        global charmsystem
        if charmsystem:
            t=charmsystem.status()
            return t[0]
    def write_preselection(self, value):
        global charmsystem
        if charmsystem:
            t=charmsystem.status()
            charmsystem.stopafter(value,0)
        


class MeasureTime(base.TimerChannel):
      
    
    def state(self):
        global charmsystem
        if charmsystem:
            t=charmsystem.status()
            if not(t[2] &2): return (FAULT,t[3])
            if(t[2] & 1): return (BUSY,t[3])
            _state= base.ImageChannel.state(self)
            return (_state[0],t[3])
       
    def Start(self):
        global charmsystem
        if charmsystem:
            charmsystem.start()

    def Stop(self):
        global charmsystem
        if charmsystem:
            charmsystem.stop()
    
    def Prepare(self):
        pass
    def Resume(self):
        global charmsystem
        if charmsystem:
            charmsystem.resume()
    def read_value(self):
        global charmsystem
        if charmsystem:
            t=charmsystem.status()
            return t[1]
    def write_preselection(self, value):
        global charmsystem
        if charmsystem:
             t=charmsystem.status()
             charmsystem.stopafter(0,value)



class Histogram(base.ImageChannel):

    commands ={
        'setRoiWKT':
            Cmd('sends WKT string with POLYGON Roi.',
                str, None,'','', disallowed=(states.FAULT, states.OFF, states.BUSY, states.INIT,
                            states.UNKNOWN,)),
        
    }
    attributes = {
        'CountsInRoi': Attr(uint64, 'counts in Roi',dims=0,writable=False,memorized=False,unit=None),
         'RoiWKT':   Attr(str,'Well known text (WKT) representation of the region of interest.'),
    }


    def init(self):
        self.count = 0;
        self.mat = np.zeros((1,1,1),dtype="int32")
        self.histogram = None
         
    def Histogram(self):
        if self.histogram is None:
             if charmsystem:
                  self.histogram = charmsystem.getHistogram()
             else:
                 return None
        return self.histogram

    def read_CountsInRoi(self):
        return uint64(self.count)
    def get_CountsInRoi_unit(self):
        return 'cts'
    def Clear(self):
        return
    def read_detectorSize(self):
        return self.Histogram().Size
    def read_RoiWKT(self):
        return self.Histogram().getRoi(0)
    def setRoiWKT(self,value):
        self.Histogram().setRoi(value,0)
    def get_RoiWKT_unit(self):
        return ''
    def read_value(self):
        t=self.Histogram().update(self.mat)
        self.mat=t[1]
        self.count=t[0];
        return self.mat.flatten()

