#!/usr/bin/env python
#                           _              _                _
#	___  __ __ __  ___     (_)     ___    | |_     ___     (_)    _ _
#   |_ /  \ V  V / / -_)    | |    (_-<    |  _|   / -_)    | |   | ' \
#  _/__|   \_/\_/  \___|   _|_|_   /__/_   _\__|   \___|   _|_|_  |_||_|
#	   .
#	   |\       Copyright (C) 2019 - 2020 by Andreas Langhoff
#	 _/]_\_                            <andreas.langhoff@frm2.tum.de>
# ~~~"~~~~~^~~   This program is free software; you can redistribute it
# and/or modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation;

import importlib
from entangle import base
from entangle.core import states, Prop, uint16, Attr,Cmd
from entangle.core.defs import uint64, int32, boolean, listof
from entangle.core.states import BUSY, UNKNOWN,FAULT
from entangle.core.errors import InvalidValue, InvalidOperation, \
    ConfigurationError
from entangle.lib.loggers import FdLogMixin

import signal, os

from entangle.device.charming import listmodereplay
import entangle.device.charming.msmtsystem as msmtsystem

from entangle.device.charming.core import *

class DeviceConnection(FdLogMixin,base.MLZDevice):
    commands = {
         'Log':
            Cmd('latest log messages.',None, listof(str), '', ''),

    }
    def init(self):
        self.init_fd_log('Replay')
        fd = self.get_log_fd()
       # print("charm-replay.py:DeviceConnection.init("+str(fd)+")")
        if msmtsystem.msmtsystem is None:
                msmtsystem.msmtsystem=listmodereplay.ReplayList(fd)


   # def __del__(self):
   #   print("charm-replay.py: DeviceConnection.__del__")

    def read_version(self):
        ver = super().read_version();
        if not msmtsystem.msmtsystem:
            return ver
        return ver + " "+msmtsystem.msmtsystem.version

    def Log(self):
        return msmtsystem.msmtsystem.log()

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
        if msmtsystem.msmtsystem:
            return msmtsystem.msmtsystem.removefile(file)
        return False

    def AddFile(self,file):
        if msmtsystem.msmtsystem:
            return msmtsystem.msmtsystem.addfile(file)
        return False

    def FilesInDirectory(self,directory):
        print('FilesInDirectory('+str(directory)+')')

        if msmtsystem.msmtsystem:
            return msmtsystem.msmtsystem.files(directory)

    def read_version(self):
        ver = super().read_version();
        if not msmtsystem.msmtsystem:
            return ver
        return ver + "\r\n"+msmtsystem.msmtsystem.version






