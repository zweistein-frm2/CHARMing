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
from entangle.core import Cmd
from entangle.core.defs import  listof

from entangle.lib.loggers import FdLogMixin

#import entangle.device.charming as charming
import entangle.device.charming.mesytecsystem as mesytecsystem
# pylint: disable=wildcard-import
from entangle.device.charming.core import *
from entangle.device.charming.settings import *
from entangle.device.charming.simulator import *


class DeviceConnection(FdLogMixin, base.MLZDevice):
    commands = {
         'Log':
            Cmd('latest log messages.', None, listof(str), '', ''),
    }

    def init(self):
        self.init_fd_log('Mesytec')
        fd = self.get_log_fd()
        #print("mesytec.py:DeviceConnection.init("+str(fd)+")")
        # pylint: disable=undefined-variable
        if msmtsystem.msmtsystem is None:
            # pylint: disable=undefined-variable
            msmtsystem.msmtsystem = mesytecsystem.NeutronMeasurement(fd)
            self.On()

    #def __del__(self):
        #print("charm.py: DeviceConnection.__del__")
    def On(self):
        # pylint: disable=undefined-variable
        if msmtsystem.msmtsystem:
            msmtsystem.msmtsystem.on()
    def Off(self):
        # pylint: disable=undefined-variable
        if msmtsystem.msmtsystem:
            msmtsystem.msmtsystem.off()
    def read_version(self):
        ver = super().read_version()
        # pylint: disable=undefined-variable
        if not msmtsystem.msmtsystem:
            return ver
        return ver + " "+msmtsystem.msmtsystem.version

    def Log(self):
        # pylint: disable=undefined-variable
        return msmtsystem.msmtsystem.log()
