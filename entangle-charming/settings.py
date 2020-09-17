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
from entangle.core.defs import  boolean

import entangle.device.charming.msmtsystem as msmtsystem

class Settings(base.MLZDevice):
    attributes = {
        'writelistmode':
            Attr(boolean,'write mesytec listmode to file',
                 writable=True,memorized=False,  disallowed_read=(states.INIT, states.UNKNOWN,),
                 disallowed_write=( states.OFF, states.INIT, states.UNKNOWN,)),
    }

    def read_writelistmode(self):
        if msmtsystem.msmtsystem:
            return msmtsystem.msmtsystem.writelistmode

    def write_writelistmode(self,value):
        if msmtsystem.msmtsystem:
            msmtsystem.msmtsystem.writelistmode=value

    def get_writelistmode_unit(self):
        return ''

    def read_version(self):
        ver = super().read_version()
        if not msmtsystem.msmtsystem:
            return ver
        return ver + " "+msmtsystem.msmtsystem.version