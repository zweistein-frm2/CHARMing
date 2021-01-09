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
import json
import copy
from entangle import base
from entangle.core import states, Attr
from entangle.core import states, Prop, Attr, Cmd, pair, listof, uint32, boolean
import entangle.device.charming.msmtsystem as msmtsystem
import ctypes

__ALL__ = ['writelistmode']


# IMPROVE:
# values t write expect python format f.e. True  while values returned are in json format (hence true)
# should become more consistent,
# maybe enough to fix the true ->True case? numbers should be ok.

# we must implement the attribute and member functions in class Settings
# for exampe :
# class Settings(CmdProcessor,base.StringIO):
#
#   def read_writelistmode(self):
#   def write_writelistmode(self,value):
#   setting values are returned as json strings
class CmdProcessor(object):
    lastcmd = ''
    def read_availableLines(self):
        return len(__ALL__)

    def read_availableChars(self):
        rv = ctypes.c_ulong(-1)
        return rv.value
  
    def Write(self, msg:str)->uint32:
        self.lastcmd = msg.rstrip()
        #print('CmdProcessor.Write('+self.lastcmd + ')')

        tok = self.lastcmd.split(':')
        if not tok:
            return 0
        if len(tok) < 2:

            # so tok should contain a number which is index to setting value we want to read out
            #
            return 0
        #print("tok[0]="+str(tok[0]))

        for setting in __ALL__:
            if tok[0] == setting:
                value = self.lastcmd[len(setting)+1:].strip()
                #now we generate a member function signature of the form
                # write_setting(self,value)
                f_str = 'self.write_'+setting
                try:
                    tmpvalue = eval(value)
                    eval(f_str)(tmpvalue)
                except Exception as inst:
                    print(type(inst))    # the exception instance
                    print(inst.args)     # arguments stored in .args
                    print(inst)          # __str__ allows args to be printed directly,
                             # but may be overridden in exception subclasses
            return len(msg)

    def ReadLine(self):
        #print('CmdProcessor.ReadLine() lastcmd == ' + self.lastcmd)
        if not self.lastcmd:
            return ''
        if self.lastcmd == '?':
            return self._state[1]
        tmp = self.lastcmd.rstrip()
        tok = tmp.split(':')
        # if tok[0] is a number then it is the index to the setting we want to read out
        #otherwise it is the setting
        try :
            if len(tok[0]) <= 2:  # 0 to 99
                index =  int(tok[0])
                if index >= 0 and index < len(__ALL__):
                    tok[0] = __ALL__[index]
        except:
            pass

        for setting in __ALL__:
            if tok[0] ==setting:
                 r_str = 'self.read_'+setting+'()'
                 try:
                     rv = {}
                     value = eval(r_str)
                     rv[setting] = value
                     return json.dumps(rv)
                 except Exception as inst:
                     print(type(inst))    # the exception instance
                     print(inst.args)     # arguments stored in .args
                     print(inst)          # __str__ allows args to be printed directly,

        return ''

class Settings(CmdProcessor,base.StringIO):
    attributes = {
        __ALL__[0]:
            Attr(boolean,'write mesytec listmode to file',
                 writable=True,memorized=False,  disallowed_read=(states.INIT, states.UNKNOWN,),
                 disallowed_write=( states.OFF, states.INIT, states.UNKNOWN,)),
    }

    def read_writelistmode(self):
        if msmtsystem.msmtsystem:
            return msmtsystem.msmtsystem.writelistmode

    def write_writelistmode(self,value):
        print('write_writelistmode')
        if msmtsystem.msmtsystem:
            msmtsystem.msmtsystem.writelistmode=value

    def get_writelistmode_unit(self):
        return ''

    def read_version(self):
        ver = super().read_version()
        if not msmtsystem.msmtsystem:
            return ver
        return ver + " "+msmtsystem.msmtsystem.version