import importlib
from entangle import base
from entangle.core import states, Prop, uint16, Attr,Cmd
from entangle.core.defs import uint64, int32, boolean, listof
from entangle.core.states import BUSY, UNKNOWN,FAULT
from entangle.core.errors import InvalidValue, InvalidOperation, \
    ConfigurationError
from entangle.lib.loggers import FdLogMixin
import signal, os

from entangle.device.charming import charmsystem
import entangle.device.charming.msmtsystem as msmtsystem

from entangle.device.charming.core import *
from entangle.device.charming.settings import *
from entangle.device.charming.simulator import *

class DeviceConnection(FdLogMixin,base.MLZDevice):
    commands = {
         'Log':
            Cmd('latest log messages.',None, listof(str), '', ''),
    }

    def init(self):
        self.init_fd_log('Charm')
        fd = self.get_log_fd()
        #print("charm.py:DeviceConnection.init("+str(fd)+")")
        if msmtsystem.msmtsystem is None:
                msmtsystem.msmtsystem=charmsystem.NeutronMeasurement(fd)

    #def __del__(self):
        #print("charm.py: DeviceConnection.__del__")

    def On(self):
      if msmtsystem.msmtsystem:
            msmtsystem.msmtsystem.on()

    def Off(self):
      if msmtsystem.msmtsystem:
            msmtsystem.msmtsystem.off()

    def read_version(self):
        ver = super().read_version();
        if not msmtsystem.msmtsystem:
            return ver
        return ver + " "+msmtsystem.msmtsystem.version

    def Log(self):
        return msmtsystem.msmtsystem.log()



