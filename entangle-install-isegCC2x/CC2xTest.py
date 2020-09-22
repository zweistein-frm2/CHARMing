import sys
import time
from os import path
import toml
import json
# Add import path for inplace usage
sys.path.insert(0, path.abspath(path.join(path.dirname(__file__), '../../..')))
import CC2xlib.globals
import CC2xlib.json_data
import CC2xlib.CC2xjsonhandling
import CC2x



from entangle.core import states ,Prop


class PowerSupply(CC2x.PowerSupply):

    _props = {}
    def __init__(self,logger=None):
        self.state = states.INIT
        with open('CC2xlib/example/Erwin-small-HV.res') as fd:
            data = toml.load(fd)
            tango_name = 'test/Erwin/HV-Powersupply'
            self.address = data[tango_name]['address']
            self.user = data[tango_name]['user']
            self.password = data[tango_name]['password']
            self.transitions = data[tango_name]['transitions']
            self.groups = data[tango_name]['groups']
            self.operatingstyles = data[tango_name]['operatingstyles']
        self.init()

    def __delete__(self):
        self.delete()
        
             
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
                        rol.append(CC2xlib.json_data.make_requestobject("setItem",channel,item,v))
        j = 0
        for channel in channels :
            rol.append(CC2xlib.json_data.make_requestobject("setItem",channel,cmditem,channelvalues[j]))
            j = j + 1
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
                        rol.append(CC2xlib.json_data.make_requestobject("setItem",channel,item,v))
        return rol


a = PowerSupply()

a.power(True)

time.sleep(1)
t = a.getTransitionNames()
a.setVoltage(([29],['0_0_0']))

a.applyTransition("Off->On")
for i in range(10):
    time.sleep(1)

statusjsonstr = a.getstatusJson()
voltage004 = CC2xlib.CC2xjsonhandling.getStatusValue("0_0_4","Status.voltageMeasure",statusjsonstr)
if voltage004:
    print("\r\n"+"0_0_4"+ " : "+ "Status.voltageMeasure" +"="+ str(voltage004)+"\r\n")
for i in range(15):
    time.sleep(1)

a.applyTransition("On->Off")



