import sys
import time
from os import path
import toml
import json
# Add import path for inplace usage
sys.path.insert(0, path.abspath(path.join(path.dirname(__file__), '../../..')))
import CC2xlib.globals
import CC2xlib.json_data as json_data
from entangle.core import states 


class PowerSupply():

      def __init__(self, *args, **kwargs):
           #with open('../../../example/Erwin-small-HV.res') as fd:
           with open('CC2xlib/example/Erwin-small-HV.res') as fd:
               data = toml.load(fd)
               tango_name = "test/Erwin/HV-Powersupply"
               self.transitions = data[tango_name]["transitions"]
               self.groups = data[tango_name]["groups"]
               self.operatingstyles = data[tango_name]["operatingstyles"]
               self.address = data[tango_name]["address"]
               self.user = data[tango_name]["user"]
               self.password = data[tango_name]["password"]

           return super().__init__(*args, **kwargs)

      def init(self):
          self.jtransitions = json.loads(self.transitions)
          self.jgroups = json.loads(self.groups)
          #checkchannels(groups)
          self.joperatingstyles = json.loads(self.operatingstyles)
          #checkoperatingstates(operatingstates)
          CC2xlib.globals.add_monitor(self.address,self.user,self.password)
          self.state = states.ON

     

           
      def setGroupItemValues(self,groupname,cmditem,channelvalues):
        rol = []
        channels = self.jgroup[groupname]['CHANNEL']
        rampstyle = self.jgroup[groupname]['OPERATINGSTYLE']
        for rstyle in self.joperatingstates :
            stylename = list(rstyle.keys())[0]
            if stylename == rampstyle :
                for k, v in rstyle[stylename].items():
                    item = k
                    if len(channels) != len(channelvalues):
                         raise Exception('len list of channelvalues ', 'not equal len list of channels in group '+groupname)
                    for channel in channels :
                            rol.append(json_data.make_requestobject("setItem",channel,item,v))
        i = 0
        for channel in channels :
            rol.append(json_data.make_requestobject("setItem",channel,cmditem,channelvalues[i]))
            i = i + 1
        return rol
        


      def rolAddOperatingStyle(self,groupname):
        rol = [] # request object list 
        channels = self.jgroup[groupname]['CHANNEL']
        rampstyle = self.jgroup[groupname]['OPERATINGSTYLE']
        for rstyle in self.joperatingstates :
            stylename = list(rstyle.keys())[0]
            if stylename == rampstyle :
                for k, v in rstyle[stylename].items():
                    item = k
                    for channel in channels :
                        rol.append(json_data.make_requestobject("setItem",channel,item,v))
        return rol
      
      def getChannels(self,groupname):
           groups = self.jgroups['GROUP']
           for group in groups:
               for key,val in group.items():
                   if key == groupname:
                       channels = val["CHANNEL"]
                       return channels


              
              
          

      def getTransitions(self):
          return self.jtransitions['TRANSITION']

           
      def rolSetVoltage(self, arg):
          if len(arg) != 2 :
               raise Exception('SetVoltage(arg)', 'is not a pair of objects (must be lists)')
          keys = arg[1]
          values = arg[0]
          # check channel is one of groups
          if len(keys) != len(values) :
               raise Exception('len list of values ', 'not equal len list of keys')

          rol = []

          for i in range(len(keys)):
             rol.append( json_data.make_requestobject("setItem",keys[i],"Control.voltageSet",values[i]))
     
          return rol
      def ApplyTransition(self,transition):
        pass
     
   


a = PowerSupply()

a.init()

CC2xlib.globals.monitored_channels = a.getChannels("Anodes")
CC2xlib.globals.monitored_channels.append("__") # for all device related
CC2xlib.globals.monitored_channels.append("0_1000_") # for Power On ??

rol = []

#rol = 

rol.append(json_data.make_requestobject("getItem","0_1000","Control.power",''))

rol.append( json_data.make_requestobject("setItem","0_1000","Control.power",1))
CC2xlib.globals.queue_request(rol)

rol = []
rol.append(a.rolSetVoltage(([30.0],["0_0_0"])))
CC2xlib.globals.queue_request(rol)



for i in range(10):
    time.sleep(1)


