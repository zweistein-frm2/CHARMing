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
from entangle.core import states, Prop, Attr, Cmd, pair, listof, uint32, boolean

from entangle.lib.loggers import FdLogMixin

import entangle.device.charming as charming

import entangle.device.charming.listmodereplay as listmodereplay
import entangle.device.charming.msmtsystem as msmtsystem
from  entangle.device.charming.core import *
import ctypes
import json

class DeviceConnection(FdLogMixin,base.MLZDevice):
    commands = {
         'Log':
            Cmd('latest log messages.',None, listof(str), '', ''),

    }
    def init(self):
        self.init_fd_log('Replay')
        fd = self.get_log_fd()
       # print("charm-replay.py:DeviceConnection.init("+str(fd)+")")
        if charming.msmtsystem.msmtsystem is None:
            charming.msmtsystem.msmtsystem=listmodereplay.ReplayList(fd)


   # def __del__(self):
   #   print("charm-replay.py: DeviceConnection.__del__")

    def read_version(self):
        ver = super().read_version()
        if not charming.msmtsystem.msmtsystem:
            return ver
        return ver + " "+charming.msmtsystem.msmtsystem.version

    def Log(self):
        return charming.msmtsystem.msmtsystem.log()

__ALL__ = ['RemoveFile','AddFile','FilesInDirectory']

#call format is 'functionName:[arg1,arg2,...,argn]'
#return is a json string

class CmdProcessor(object):
    lastcmd = ''
    funcstr = ''
    def read_availableLines(self):
        roilist = self.AddFile('')
        if not roilist:
            return 0
        return len(roilist)

    def read_availableChars(self):
        rv = ctypes.c_ulong(-1)
        return rv.value

    def Write(self, msg:str)->uint32:
        self.lastcmd = msg.rstrip()
        self.funcstr = ''
        #print('CmdProcessor.Write('+self.lastcmd + ')\n')

        tok = self.lastcmd.split(':')
        if not tok:
            return 0
        if len(tok) < 2:
            return 0
        #print("tok[0]="+str(tok[0]))
        for cmd in __ALL__:
            if tok[0] ==  cmd:
                roi = self.lastcmd[len(tok[0])+1:]
                if roi:
                    args = json.loads(roi)
                    self.funcstr = 'self.'+ cmd
                    self.funcstr += '('
                    i = 0
                    for a in args:
                        if i > 0:
                            self.funcstr += ','
                        if type(a) == type(""):
                            print("is a str")
                            self.funcstr +="\""
                        self.funcstr += str(a)
                        if type(a) == type(""):
                            self.funcstr +="\""
                        i = i + 1
                    self.funcstr += ')'
                    return len(msg)

        self.funcstr = ''
        self.lastcmd = ''
        raise Exception("cmd : "+tok[0] + " not found.")

        return len(msg)

    def ReadLine(self):
        print('CmdProcessor.ReadLine() lastcmd == ' + self.lastcmd)
        if not self.lastcmd:
            return ''
        if self.lastcmd == '?':
            return self._state[1]

        if self.funcstr:
            print('CmdProcessor.ReadLine() funcstr == ' + self.funcstr)
            try:
                value = eval(self.funcstr)
                print(value)
                self.funcstr = ''
                return json.dumps(value)
            except Exception as inst:
                print(type(inst))    # the exception instance
                print(inst.args)     # arguments stored in .args
                print(inst)          # __str__ allows args to be printed directly,
        return ''

class PlayList(CmdProcessor,base.StringIO):
    commands = {
         'RemoveFile':
            Cmd('remove file from playlist.',str, listof(str), '', ''),
          'AddFile':
            Cmd('add file to playlist.', str, listof(str), '', ''),
          'FilesInDirectory':
            Cmd('return directory list of .mdat files.', str, listof(str), '', ''),
    }

    def state(self):
        _state = base.StringIO.state(self)
        roil =  self.AddFile('')
        msg = ''

        if roil:
            return json.dumps(roil)
            #msg += '['
            #i = 0
            #for tup in roil:
            #    if i:
            #      msg += ','
            #    msg += tup[0]
            #    i = i + 1
            #msg += ']'


        return (_state[0],msg)

    def RemoveFile(self,file):
        if charming.msmtsystem.msmtsystem:
            return charming.msmtsystem.msmtsystem.removefile(file)
        return False

    def AddFile(self,file):
        #print("\n\rAddFile("+str(file)+")")
        if charming.msmtsystem.msmtsystem:
            return charming.msmtsystem.msmtsystem.addfile(file)
        return False

    def FilesInDirectory(self,directory):
        #print('FilesInDirectory('+str(directory)+')')
        if charming.msmtsystem.msmtsystem:
            files = charming.msmtsystem.msmtsystem.files(directory)
            return files
        return False


    def read_version(self):
        ver = super().read_version()
        if not charming.msmtsystem.msmtsystem:
            return ver
        return ver + "\r\n"+charming.msmtsystem.msmtsystem.version






