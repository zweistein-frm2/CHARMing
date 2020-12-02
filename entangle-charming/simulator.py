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
from entangle.core import states, Attr
from entangle.core.defs import  int32

import entangle.device.charming as charming
import entangle.device.charming.msmtsystem as msmtsystem


class Simulator(base.MLZDevice):


    attributes = {
        'NucleoRate':
            Attr(int32,'simulator data rate',
                 writable=True,memorized=False,  disallowed_read=(states.INIT, states.UNKNOWN,),
                 disallowed_write=( states.OFF, states.INIT, states.UNKNOWN,)),
    }

    def read_NucleoRate(self):
        if charming.msmtsystem.msmtsystem:
            return charming.msmtsystem.msmtsystem.simulatorRate

    def write_NucleoRate(self,value):
        if msmtsystem.msmtsystem:
            msmtsystem.msmtsystem.simulatorRate=value

    def get_NucleoRate_unit(self):
        return 'Events/second'

    def read_version(self):
        ver = super().read_version()
        if not msmtsystem.msmtsystem:
            return ver
        return ver + " "+msmtsystem.msmtsystem.version


