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

import ctypes
import json

from entangle import base
from entangle.core import Cmd, listof, uint32

from entangle.lib.loggers import FdLogMixin

import entangle.device.charming.listmodereplay as listmodereplay
# pylint: disable=wildcard-import
from  entangle.device.charming.core import *


class DeviceConnection(FdLogMixin, base.MLZDevice):
    commands = {
         'Log':
            Cmd('latest log messages.', None, listof(str), '', ''),

    }
    def init(self):
        self.init_fd_log('Replay')
        fd = self.get_log_fd()
        # print("charm-replay.py:DeviceConnection.init("+str(fd)+")")
        # pylint: disable=undefined-variable
        if msmtsystem.msmtsystem is None:
            msmtsystem.msmtsystem = listmodereplay.ReplayList(fd)


   # def __del__(self):
   #   print("charm-replay.py: DeviceConnection.__del__")

    def read_version(self):
        ver = super().read_version()
        # pylint: disable=undefined-variable
        if not msmtsystem.msmtsystem:
            return ver
        return ver + " " + msmtsystem.msmtsystem.version

    def Log(self):
        # pylint: disable=undefined-variable
        return msmtsystem.msmtsystem.log()

__ALL__ = ['RemoveFile', 'AddFile', 'FilesInDirectory']

__ALL_ATTRIB__ = ['speedmultiplier']

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

    def Write(self, msg: str)->uint32:
        self.lastcmd = msg.rstrip()
        self.funcstr = ''
        #print('CmdProcessor.Write('+self.lastcmd + ')\n')

        tok = self.lastcmd.split(':')
        if not tok:
            return 0
        if len(tok) < 2:
            return 0
        #print("tok[0]="+str(tok[0]))




        for setting in __ALL_ATTRIB__:
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

        for cmd in __ALL__:
            if tok[0] == cmd:
                roi = self.lastcmd[len(tok[0])+1:]
                if roi:
                    args = json.loads(roi)
                    self.funcstr = 'self.'+ cmd
                    self.funcstr += '('
                    i = 0
                    for a in args:
                        if i > 0:
                            self.funcstr += ','
                        # pylint: disable=unidiomatic-typecheck
                        if type(a) == type(""):
                            print("is a str")
                            self.funcstr += "\""
                        self.funcstr += str(a)
                        if type(a) == type(""):
                            self.funcstr += "\""
                        i = i + 1
                    self.funcstr += ')'
                    return len(msg)

        self.funcstr = ''
        self.lastcmd = ''
        raise Exception("cmd : "+tok[0] + " not found.")


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
        try:
            if len(tok[0]) <= 2:  # 0 to 99
                index = int(tok[0])
                # pylint: disable=chained-comparison
                if index >= 0 and index < len(__ALL__):
                    # pylint: disable=undefined-variable
                    tok[0] = __ALL_ATTRIB___[index]
        # pylint: disable=bare-except
        except:
            pass


        for setting in __ALL_ATTRIB__:
            if tok[0] == setting:
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


        if self.funcstr:
            #print('CmdProcessor.ReadLine() funcstr == ' + self.funcstr)
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

class PlayList(CmdProcessor, base.StringIO):
    commands = {
         __ALL__[0]:
            Cmd('remove file from playlist.', str, listof(str), '', ''),
          __ALL__[1]:
            Cmd('add file to playlist.', str, listof(str), '', ''),
          __ALL__[2]:
            Cmd('return directory list of .mdat files.', str, listof(str), '', ''),
    }

    attributes = {
        __ALL_ATTRIB__[0]:
            # pylint: disable=undefined-variable
            Attr(int, 'replay speedmultiplier',
                 writable=True, memorized=False, disallowed_read=(states.INIT, states.UNKNOWN,),
                 disallowed_write=(states.OFF, states.INIT, states.UNKNOWN,)),
    }

    def init(self):
        print("PlayList::init")
        files = self.FilesInDirectory("~")
        for f in files:
            self.AddFile(f)

    def state(self):
        _state = base.StringIO.state(self)
        roil = self.AddFile('')
        msg = ''

        if roil:
            return (_state[0], json.dumps(roil))
            #msg += '['
            #i = 0
            #for tup in roil:
            #    if i:
            #      msg += ','
            #    msg += tup[0]
            #    i = i + 1
            #msg += ']'


        return (_state[0], msg)

    def RemoveFile(self, file):
        # pylint: disable=undefined-variable
        if msmtsystem.msmtsystem:
            return msmtsystem.msmtsystem.removefile(file)
        return []

    def AddFile(self, file):
        #print("\n\rAddFile("+str(file)+")")
        # pylint: disable=undefined-variable
        if msmtsystem.msmtsystem:
            return msmtsystem.msmtsystem.addfile(file)
        return []

    def FilesInDirectory(self, directory):
        #print('FilesInDirectory('+str(directory)+')')
        # pylint: disable=undefined-variable
        if msmtsystem.msmtsystem:
            files = msmtsystem.msmtsystem.files(directory)
            return files
        return []


    def read_version(self):
        ver = super().read_version()
        # pylint: disable=undefined-variable
        if not msmtsystem.msmtsystem:
            return ver
        return ver + "\r\n" + msmtsystem.msmtsystem.version

        # pylint: disable=inconsistent-return-statements
    def read_speedmultiplier(self):
        # pylint: disable=undefined-variable
        if msmtsystem.msmtsystem:
            # pylint: disable=undefined-variable
            return msmtsystem.msmtsystem.speedmultiplier
    # pylint: disable=inconsistent-return-statements
    def write_speedmultiplier(self, value):
        #print('write_writelistmode')
        # pylint: disable=undefined-variable
        if msmtsystem.msmtsystem:
            msmtsystem.msmtsystem.speedmultiplier = value

    def get_speedmultiplier_unit(self):
        return ''
