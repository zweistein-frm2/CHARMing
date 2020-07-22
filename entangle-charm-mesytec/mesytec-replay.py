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

from entangle.device.charm import listmodereplay

charmsystem = None

class DeviceConnection(FdLogMixin,base.MLZDevice):
    commands = {
         'Log':
            Cmd('latest log messages.',None, listof(str), '', ''),

    }
    def init(self):
        self.init_fd_log('Replay')
        fd = self.get_log_fd()
       # print("charm-replay.py:DeviceConnection.init("+str(fd)+")")
        global charmsystem
        if charmsystem is None:
                charmsystem=listmodereplay.ReplayList(fd)

   # def __del__(self):
   #   print("charm-replay.py: DeviceConnection.__del__")

    def read_version(self):
        ver = super().read_version();
        if not charmsystem:
            return ver
        return ver + " "+charmsystem.version

    def Log(self):
        return charmsystem.log()

class PlayList(base.MLZDevice):
    commands = {
         'RemoveFile':
            Cmd('remove file from playlist.',str, listof(str), '', ''),
          'AddFile':
            Cmd('add file to playlist.', str, listof(str), '', ''),
          'FilesInDirectory':
            Cmd('return directory list of .mdat files.', str, listof(str), '', ''),
    }


    def RemoveFile(self,file):
        global charmsystem
        if charmsystem:
            return charmsystem.removefile(file)
        return False

    def AddFile(self,file):
        global charmsystem
        if charmsystem:
            return charmsystem.addfile(file)
        return False

    def FilesInDirectory(self,directory):
        print('FilesInDirectory('+str(directory)+')')

        global charmsystem
        if charmsystem:
            return charmsystem.files(directory)

    def read_version(self):
        ver = super().read_version();
        if not charmsystem:
            return ver
        return ver + "\r\n"+charmsystem.version




importlib.import_module('charming-core')
