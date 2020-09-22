#  -*- coding: utf-8 -*-
#***************************************************************************
#* Copyright (C) 2020 by Andreas Langhoff *
#* <andreas.langhoff@frm2.tum.de> *
#* This program is free software; you can redistribute it and/or modify *
#* it under the terms of the GNU General Public License v3 as published *
#* by the Free Software Foundation; *
# **************************************************************************

import json
import threading
import time
from typing import List

from entangle import base
from entangle.core.defs import boolean
from entangle.core import states , Prop, Cmd, pair,listof
from  entangle.device.iseg import CC2xlib
import entangle.device.iseg.CC2xlib.globals
import entangle.device.iseg.CC2xlib.json_data
import entangle.device.iseg.CC2xlib.CC2xjsonhandling



class PowerSupply(base.MLZDevice):
    """Controls an Iseg CC2x high-voltage power supply via websocket."""
    
    commands = {
        'setVoltage':
            Cmd('Sets Voltage for a multiple channels.',
                pair(listof(float), listof(str)) , None,
                '[channel1,channel2,...][voltage1,voltage2,..]',
                'None',
                disallowed= (states.BUSY, states.FAULT, states.INIT,
                            states.UNKNOWN,)),
        'getGroupNames':
            Cmd('gets group names.',
                None,listof(str),'group names',None,
                disallowed = (states.BUSY, states.FAULT, states.INIT,
                              states.UNKNOWN,)),

        'getChannels':
            Cmd('gets channel list for a group.',
                str,listof(str),'group name','list of channels',
                disallowed = (states.BUSY, states.FAULT, states.INIT,
                              states.UNKNOWN,)),
        'getTransitionNames':
            Cmd('gets list of transition names.',
                None,listof(str),'None','transition names',
                disallowed = (states.BUSY, states.FAULT, states.INIT,
                              states.UNKNOWN,)),
       
        'applyTransition':
            Cmd('applies a transition like Off->On, On->Off etc.',
                str,None, 'transition namé', 'None',
                disallowed = (states.BUSY,)),

        'getstatusJson':
            Cmd('gets status for everything in a json string.',
                None,str,'','status for everything',
                disallowed = (states.UNKNOWN,)),

    }

    
    properties = {
        'address': Prop(str, 'ip address of device.'),
        'user': Prop(str, 'user.'),
        'password': Prop(str, 'pw.'),
        'transitions': Prop(str, 'transitions.'),
        'groups': Prop(str, 'groups.'),
        'operatingstyles': Prop(str, 'operatingstyles.'),
      }

   
 
    def init(self):

        #checkoperatingstates(operatingstates)
        self.channels_handled = self.checkchannels()
        self.waitstring =''
        self.waitstringmintime = ''
        self.tw = None
        # accessed also from other thread, so do the proper locking:
        # self.channels_handled
        # self.waitstring
        # self.waitstringmintime
        CC2xlib.globals.lock.acquire()
        CC2xlib.globals.instances.append(self)
        CC2xlib.globals.lock.release()
        CC2xlib.globals.add_monitor(self.address,self.user,self.password)

    def __delete__(self):
        self.delete()

    def delete(self):
        CC2xlib.globals.lock.acquire()
        for i in CC2xlib.globals.instances:
            if i == self :
                CC2xlib.globals.instances.remove(i)
        CC2xlib.globals.lock.release()

    def checkchannels(self):
            rv = []
            jobjgroups = json.loads(self.groups)
            groups = jobjgroups['GROUP']
            for group in groups:
                for groupname in group:
                    channels = CC2xlib.CC2xjsonhandling.getChannels(self.groups,groupname)
                    for ch in channels:
                        if ch in rv:
                            raise Exception("Duplicate channel '"+ch + "' in 'GROUP':"+group)

                        isusedbyother = False;
                        CC2xlib.globals.lock.acquire()
                        for i in CC2xlib.globals.instances:
                            if i != self:
                                if ch in i.channels_handled:
                                    isusedbyother = True

                        CC2xlib.globals.lock.release()

                        if isusedbyother:
                            raise Exception("Channel '"+ch + "' used by another instance")
                        rv.append(ch)
            return rv

    def getChannels(self,groupname):
        return CC2xlib.CC2xjsonhandling.getChannels(self.groups,groupname)
        

    def On(self):
        self.power(True)

    def Off(self):
        self.power(False)

    def power(self, value: bool) -> None:
        rol = []
        rol.append( CC2xlib.json_data.make_requestobject("setItem",CC2xlib.globals.always_monitored[0],"Control.power",str(int(value))))
        rol.append( CC2xlib.json_data.make_requestobject("getItem",CC2xlib.globals.always_monitored[0],"Control.power",''))
        CC2xlib.globals.queue_request(rol)

    def getGroupNames(self)->List[str]:
        return CC2xlib.CC2xjsonhandling.getGroupNames(self.groups)
       


    def getTransitionNames(self):
        tmp =''
        CC2xlib.globals.lock.acquire()
        tmp = self.transitions[:]
        CC2xlib.globals.lock.release()
        return CC2xlib.CC2xjsonhandling.getTransitionNames(tmp)

    def getTransitions(self):

        tmp =''
        CC2xlib.globals.lock.acquire()
        tmp = self.transitions[:]
        CC2xlib.globals.lock.release()
        return CC2xlib.CC2xjsonhandling.getTransitions(tmp)
        

    def setVoltage(self, arg):
        if len(arg) != 2 :
            raise Exception('SetVoltage(arg)', 'is not a pair of objects (must be lists)')
        keys = arg[1]
        values = arg[0]
        
        # check channel is one of groups
        if len(keys) != len(values) :
            raise Exception('len list of values ', 'not equal len list of keys')

        rol = []

        for j in range(len(keys)):
            print("key="+ str(keys[j]) + ", value="+str(values[j]) )
            rol.append( CC2xlib.json_data.make_requestobject("getItem",keys[j],"Control.voltageSet",''))
            rol.append( CC2xlib.json_data.make_requestobject("setItem",keys[j],"Control.voltageSet",values[j]))
     
        CC2xlib.globals.queue_request(rol) 
        

    def getstatusJson(self):
        ours = CC2xlib.globals.StatusJson(self.channels_handled)
        return ours

    def applyTransition(self,toapply):
        if (self.tw) and (self.tw.is_alive()):
            self.tw.join()

        self.tw = threading.Thread(target=self.applytransitionworker, args=(toapply,))
        self.tw.start()

    def applytransitionworker(self,toapply):
        CC2xlib.globals.lock.acquire()
        self.state = "START:"+toapply
        print(self.state)
        CC2xlib.globals.lock.release()
        transitions = self.getTransitions()
        for tr in transitions:
            if toapply in tr:
                workqueue = tr[toapply]
                #here we actually elaborate the workjobs
                for nextjob in workqueue:
                    
                    for item in nextjob:
                        if(str(item) == 'GROUP') :
                            continue
                        getrol = []
                        # we wait for response, but by sending a "getItem" we force a response which would not come if condition already reached
                        groups = nextjob['GROUP']
                        for groupname in groups:
                            channels = CC2xlib.CC2xjsonhandling.getChannels(self.groups,groupname)
                            j = 0
                            for channel in channels:
                                getrol.append(CC2xlib.json_data.make_requestobject("getItem",channel,item,''))
                                j = j + 1
                        
                            
                        CC2xlib.globals.lock.acquire()
                        self.waitstring =json.dumps(nextjob)
                        self.waitstringmintime = ''
                        if not (str(item).startswith("Control.") or str(item).startswith("Setup.")):
                            self.state = "WAITFOR_CONDITION:"+str(nextjob)
                            print(self.state)
                        CC2xlib.globals.lock.release()

                        CC2xlib.globals.queue_request(getrol) 

                        if (str(item).startswith("Control.") or str(item).startswith("Setup.")):
                            
                            values = nextjob[item]
                            groups = nextjob['GROUP']
                            for groupname in groups:
                                channels = CC2xlib.CC2xjsonhandling.getChannels(self.groups,groupname)
                                j = 0
                                rol = []
                                for channel in channels:
                                    rol.append(CC2xlib.json_data.make_requestobject("setItem",channel,item,values[j]))
                                    j = j + 1
                                CC2xlib.globals.lock.acquire()
                                self.state = "QUEUE_REQUEST:"+str(nextjob)
                                print(self.state)
                                CC2xlib.globals.lock.release()
                                CC2xlib.globals.queue_request(rol) 
                        rrlen = 1
                        while rrlen:
                            time.sleep(2)
                            CC2xlib.globals.lock.acquire()
                            if not self.waitstring:
                                rrlen = 0
                            CC2xlib.globals.lock.release()
                               
                        pass
                
                CC2xlib.globals.lock.acquire()
                self.state = "FINISHED:"+toapply
                print(self.state)
                CC2xlib.globals.lock.release()
                return
        
     
   


