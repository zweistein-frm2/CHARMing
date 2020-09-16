["test/Erwin/HV-Powersupply"]
type = "iseg.CC2x.PowerSupply"

address = '172.25.25.56'
user = 'admin'
password = 'password'
absmin = 0
absmax = 0

transitions="""
{
"TRANSITION" :[
{"Off->On": {"GROUP":["Anodes"],"Control.voltageSet": [2075,2100,2085] }}
]
}
"""

groups ="""
{
 "GROUP": [
   {"Anodes": { "CHANNEL": ["0_0_0","0_0_1","0_0_2"]  ,"OPERATINGSTYLE": "slow" }},
   {"CathodeStripes": { "CHANNEL": ["0_0_4","0_0_5"],  "Control.voltageSet": [75,80],  "OPERATINGSTYLE": "normal" }},
   {"Window": { "CHANNEL": ["0_0_7"],  "Control.voltageSet": [-1000] , "OPERATINGSTYLE": "slow" }}
 ]
}
"""
operatingstyles = """
{
  "OPERATNGSTYLE":
  [ 
    {"normal": { "Control.voltageRampspeedUp" : 5, 
                "Control.voltageRampspeedDown" :10,
                "Control.currentSet" : 1,
                "Setup.delayedTripTime" : 100 

     }},
	  {"slow": {    "Control.voltageRampspeedUp" : 2,
                    "Control.voltageRampspeedDown" : 5,
                    "Control_currentSet" : 1,
                    "Setup_delayedTripTime" : 100
	   }}
   ]
}
"""

