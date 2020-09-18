#  -*- coding: utf-8 -*-
#***************************************************************************
#* Copyright (C) 2020 by Andreas Langhoff *
#* <andreas.langhoff@frm2.tum.de> *
#* This program is free software; you can redistribute it and/or modify *
#* it under the terms of the GNU General Public License v3 as published *
#* by the Free Software Foundation; *
# **************************************************************************
import time
import json

from entangle import base
from entangle.core import states , Prop, Cmd, pair,listof
from entangle.device.iseg.CC2xlib.globals import add_monitor, queue_request

from CC2xlib import globals, json_data
class PowerSupply(base.MLZDevice):
    """Controls an Iseg NHQ/EHQ high-voltage power supply via string I/O."""
    
    commands = {
        'setVoltage':
            Cmd('Sets Voltage for a multiple channels.',
                pair(listof(float), listof(str)) , None,
                '[channel1,channel2,...][voltage1,voltage2,..]',
                'None',
                disallowed= (states.BUSY, states.FAULT, states.INIT,
                            states.UNKNOWN,)),
         'getChannels':
            Cmd('gets channel list for a group.',
                str,listof(str),'group name','list of channels',
                disallowed = (states.BUSY, states.FAULT, states.INIT,
                              states.UNKNOWN,)),
         'applyTransition':
            Cmd('applies a transition like Off->On, On->Off etc.',
                str,None, 'transition namé', 'None',
                disallowed = (states.BUSY,)),


    }

    
    properties = {
        'address': Prop(str, 'ip address of device.'),
        'user': Prop(str, 'user.'),
        'password': Prop(str, 'pw.'),
        'groups': Prop(str, 'groups.'),
        'operatingstates': Prop(str, 'operatingstates.'),
      }

   
 
    def init(self):
        print("Init")
        self.jgroups = json.loads(self.groups)
        #checkchannels(groups)
        self.joperatingstates = json.loads(self.operatingstates)

        for group in self.jgroups:
            #groupname = list(group.keys())[0]
            print(group)

        #checkoperatingstates(operatingstates)
        add_monitor(self.address,self.user,self.password)
        state = states.ON
   
    def setItemValue(self,groupname,item,value):
        rol = [] # request object list 
        channels = self.jgroup[groupname]['channels']
        rampstyle = self.jgroup[groupname]['operatingstate']
        for rstyle in self.joperatingstates :
            stylename = list(rstyle.keys())[0]
            if stylename == rampstyle :
                for k, v in rstyle[stylename].items():
                    item = k
                    for channel in channels :
                            rol.append(json_data.make_requestobject("setItem",channel,item,v))
        queue_request(rol)

    def SetRampSpeed(self,arg):
        pass

    def SetVoltage(self, arg):
          if len(arg) != 2 :
               raise Exception('SetVoltage(arg)', 'is not a pair of objects (must be lists)')
          keys = arg[1]
          values = arg[0]

          if len(keys) != len(values) :
               raise Exception('len list of values ', 'not equal len list of keys')
          return "ok"
     
    def ApplyTransition(self,transition):
        pass
   
 #   def Stop(self):
 #     print("Stop")
 #     pass


