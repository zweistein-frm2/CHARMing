import json
from typing import List

def getTransitions(transitions:str)->List[str]:
    jobjtransitions = json.loads(transitions)
    return jobjtransitions['TRANSITION']

def getTransitionNames(transitions:str)->List[str]:
    rv = []
    tr = getTransitions(transitions)
    for t in tr:
        for name in t:
            rv.append(name)
    return rv

def getStatusValue(channel:str,item:str,statusjsonstr:str):
    all = json.loads(statusjsonstr)
    for it in all:
        if it==channel:
            objects = all[channel]
            for cmd in objects:
                if cmd == item:
                    vu = objects[cmd]
                    return vu['v']
      
def getGroupNames(groups:str)->List[str]:
        rv = []
        jobjgroups = json.loads(groups)
        groups = jobjgroups['GROUP']
        for group in groups:
            for key,val in group.items():
                rv.append(key)
        return rv
    
def getChannels(groups:str,groupname:str)->List[str]:
        rv = []
        jobjgroups = json.loads(groups)
        ggroups = jobjgroups['GROUP']
        
        for group in ggroups:
            for key,val in group.items():
                if key == groupname:
                    channels = val["CHANNEL"]
                    for ch in channels:
                        rv.append(ch)
        return rv



