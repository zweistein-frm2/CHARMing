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

from entangle import base
from entangle.core import  Cmd
from entangle.core.defs import  listof
from entangle.lib.loggers import FdLogMixin

import entangle.device.charming as charming
import entangle.device.charming.charmsystem as charmsystem
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
        if charming.msmtsystem.msmtsystem is None:
            charming.msmtsystem.msmtsystem=charmsystem.NeutronMeasurement(fd)

    #def __del__(self):
        #print("charm.py: DeviceConnection.__del__")

    def On(self):
        if charming.msmtsystem.msmtsystem:
            charming.msmtsystem.msmtsystem.on()

    def Off(self):
        if charming.msmtsystem.msmtsystem:
            charming.msmtsystem.msmtsystem.off()

    def read_version(self):
        ver = super().read_version()
        if not charming.msmtsystem.msmtsystem:
            return ver
        return ver + " "+charming.msmtsystem.msmtsystem.version

    def Log(self):
        return charming.msmtsystem.msmtsystem.log()



