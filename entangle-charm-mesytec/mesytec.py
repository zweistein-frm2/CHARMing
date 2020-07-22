import importlib

from entangle import base
from entangle.core import states, Prop, uint16, Attr,Cmd
from entangle.core.defs import uint64, int32, boolean, listof
from entangle.core.states import BUSY, UNKNOWN,FAULT
from entangle.core.errors import InvalidValue, InvalidOperation, \
    ConfigurationError
from entangle.lib.loggers import FdLogMixin
import signal, os
import numpy as np
import cv2 as cv

from entangle.device.charm import mesytecsystem


msmtsystem = None

class DeviceConnection(FdLogMixin,base.MLZDevice):
    commands = {
         'Log':
            Cmd('latest log messages.',None, listof(str), '', ''),
    }

    def init(self):
        self.init_fd_log('Charm')
        fd = self.get_log_fd()
        #print("charm.py:DeviceConnection.init("+str(fd)+")")
        global msmtsystem
        if msmtsystem is None:
                msmtsystem=mesytecsystem.NeutronMeasurement(fd)

    #def __del__(self):
        #print("charm.py: DeviceConnection.__del__")
    def read_version(self):
        ver = super().read_version();
        if not msmtsystem:
            return ver
        return ver + " "+msmtsystem.version

    def Log(self):
        return msmtsystem.log()

importlib.import_module('Settings')
importlib.import_module('Simulator')
importlib.import_module('charming-core')


