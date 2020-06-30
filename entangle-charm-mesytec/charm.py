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
    commands = {
         'Log':
            Cmd('latest log messages.',None, listof(str), '', ''),
    }

    def init(self):
        self.init_fd_log('Charm')
        fd = self.get_log_fd()
      #  print("charm.py:DeviceConnection.init("+str(fd)+")")
        global charmsystem
        if charmsystem is None:
                charmsystem=mesytecsystem.NeutronMeasurement(fd)
       
    #def __del__(self):
       # print("charm.py: DeviceConnection.__del__")
    def read_version(self):
        ver = super().read_version();
        if not charmsystem:
            return ver
        return ver + " "+charmsystem.version
    
    def Log(self):
        #el = logging.getLoggerClass()
        #super(entangle.core.device.DeviceWorker,self).
        return ['not yet implemented']
    


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

    def read_version(self):
        ver = super().read_version();
        if not charmsystem:
            return ver
        return ver + " "+charmsystem.version

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

    def read_version(self):
        ver = super().read_version();
        if not charmsystem:
            return ver
        return ver + " "+charmsystem.version

class MeasureCounts(base.CounterChannel):
    
    def state(self):
        global charmsystem
        if charmsystem:
            t = charmsystem.status()
            if not(t[2] &2): return (FAULT,t[3])
            #if(t[2] & 1): return (BUSY,t[3])
            _state = base.ImageChannel.state(self)
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
            t = charmsystem.status()
            return t[0]
    def write_preselection(self, value):
        global charmsystem
        if charmsystem:
            t = charmsystem.status()
            charmsystem.stopafter(value,0)
    
    def read_version(self):
        ver = super().read_version();
        if not charmsystem:
            return ver
        return ver + " "+charmsystem.version


class MeasureTime(base.TimerChannel):
      
    
    def state(self):
        global charmsystem
        if charmsystem:
            t = charmsystem.status()
            if not(t[2] &2): return (FAULT,t[3])
            #if(t[2] & 1): return (BUSY,t[3])
            _state = base.ImageChannel.state(self)
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
            t = charmsystem.status()
            return t[1]
    def write_preselection(self, value):
        global charmsystem
        if charmsystem:
             t = charmsystem.status()
             charmsystem.stopafter(0,value)
    
    def read_version(self):
        ver = super().read_version();
        if not charmsystem:
            return ver
        return ver + " "+charmsystem.version


class Monitor0(base.DiscreteOutput):
    def read_value(self):
        global charmsystem
        if charmsystem:
            t = charmsystem.monitors_status()
            return t[0][1]

class Monitor1(base.DiscreteOutput):
    def read_value(self):
        global charmsystem
        if charmsystem:
            t = charmsystem.monitors_status()
            return t[1][1]

class Monitor2(base.DiscreteOutput):
    def read_value(self):
        global charmsystem
        if charmsystem:
            t = charmsystem.monitors_status()
            return t[2][1]

class Monitor3(base.DiscreteOutput):
    def read_value(self):
        global charmsystem
        if charmsystem:
            t = charmsystem.monitors_status()
            return t[3][1]


class Histogram(base.ImageChannel):

    attributes = {
         'CountsInRoi': Attr(uint64, 'counts in Roi',dims = 0,writable=False,memorized = False,unit=None),
         'maxindexroi': Attr(uint16, 'max index of roi (0 ..n-1)',dims = 0,writable=False,memorized = False,unit=None),
         'selectedRoi': Attr(uint16, 'selected Roi',dims = 0,writable=True,memorized = True,unit = None,disallowed_read = (states.BUSY, states.INIT, states.UNKNOWN,),
                            disallowed_write = (states.FAULT, states.OFF, states.BUSY,states.INIT, states.UNKNOWN,)),
         'RoiWKT':   Attr(str,'Well known text (WKT) representation of the region of interest.',writable = True,memorized = False,
                            disallowed_read = (states.BUSY, states.INIT, states.UNKNOWN,),
                            disallowed_write = (states.FAULT, states.OFF, states.BUSY,states.INIT, states.UNKNOWN,)),
                         
    }
    
    

    def init(self):
        self.count  =  0;
        self.selectedRoi = 0
        self.maxindexroi = 0
        self.mat = np.zeros((1,1,1),dtype = "int32")
        self.histogram = None
         
    def Histogram(self):
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
        #print("read_RoiWKT("+str(self.selectedRoi)+")")
        wkt = self.Histogram().getRoi(self.selectedRoi)
        return wkt
        
    def write_RoiWKT(self,value):
        print("write_RoiWKT = "+str(value))
        print("selectedRoi = "+str(self.selectedRoi))
        self.Histogram().setRoi(value,self.selectedRoi)
        
    def get_RoiWKT_unit(self):
        return ''
    
    def read_selectedRoi(self):
        return self.selectedRoi
    def write_selectedRoi(self,value):
        #print("write_selectedRoi("+str(value)+")")
        if value<0 :
            print("error, selectedRoi = "+str(value))
            return
        self.selectedRoi = value
       
    def get_selectedRoi_unit(self):
        return ''
    def read_maxindexroi(self):
        return self.maxindexroi
    def get_maxindexroi_unit(self):
        return ''
    def read_value(self):
        t = self.Histogram().update(self.mat)
        #t[0] is list of pair(wkt,count)
        self.mat = t[1]
        #print(len(t[0]))
        self.maxindexroi = len(t[0])-1
        if self.selectedRoi> self.maxindexroi:
            self.selectedRoi = self.maxindexroi
        #print(t[0][self.selectedRoi]);  #should be pair wkt/counts
        self.count=t[0][self.selectedRoi][1]
        #t[2] is list of Monitors
        return self.mat.flatten()
    def read_version(self):
        ver = super().read_version();
        if not charmsystem:
            return ver
        return ver + " "+charmsystem.version


