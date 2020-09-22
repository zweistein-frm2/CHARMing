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
{"Off->On":  [
              {"GROUP":["Window"],"Control.voltageSet": [-130]},
              {"GROUP":["Window"],"Control.on": [1] },
              {"GROUP":["Window"],"Status.ramping": [0] },
              {"GROUP":["Anodes"],"Control.voltageSet": [13,182,11]},
              {"GROUP":["Anodes"],"Control.on": [1,1,1] },
              {"GROUP":["CathodeStripes"],"Control.voltageSet": [173,12]},
              {"GROUP":["CathodeStripes"],"Control.on": [1,1]},
              {"GROUP":["CathodeStripes"],"Status.ramping": [0,0]},
              {"GROUP":["Anodes"],"Status.ramping": [0,0,0] }
             ]
},
{"On->Off":  [
             {"GROUP":["Anodes"],"Control.on": [0,0,0] } ,
             {"GROUP":["Anodes"],"Status.ramping": [0,0,0] },
             {"GROUP":["CathodeStripes"],"Control.on": [0,0]},
             {"GROUP":["Window"],"Control.on": [0] },
             {"GROUP":["Window"],"Status.ramping": [0] },
             {"GROUP":["CathodeStripes"],"Status.ramping": [0,0]}
            ]
}
]
}
"""

groups="""
{
 "GROUP": [
   {"Anodes": { "CHANNEL": ["0_0_0","0_0_1","0_0_2"]  ,"OPERATINGSTYLE": "slow" }},
   {"CathodeStripes": { "CHANNEL": ["0_0_4","0_0_5"],  "OPERATINGSTYLE": "normal" }},
   {"Window": { "CHANNEL": ["0_0_7"], "OPERATINGSTYLE": "slow" }}
 ]
}
"""
operatingstyles="""
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

