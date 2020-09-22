#  -*- coding: utf-8 -*-
#***************************************************************************
#* Copyright (C) 2020 by Andreas Langhoff *
#* <andreas.langhoff@frm2.tum.de> *
#* This program is free software; you can redistribute it and/or modify *
#* it under the terms of the GNU General Public License v3 as published *
#* by the Free Software Foundation; *
# **************************************************************************

import asyncio
import aiohttp
import websockets
import json
from concurrent.futures import TimeoutError as ConnectionTimeoutError
import base64
import os
import inspect
from os.path import expanduser

from entangle.core import states
import threading

from entangle.device.iseg import CC2xlib
from entangle.device.iseg.CC2xlib import json_data
 

instances = []

always_monitored = ['0_1000_','__']

lock = threading.Lock()

sessionid = ''
websocket = None
itemUpdated = {}
state = states.UNKNOWN



def StatusJson(channellist)->str:
    rv = ''
    tmp = {}
    lock.acquire()
    for ch in channellist:
        if ch in itemUpdated:
           tmp[ch] = itemUpdated[ch]
    rv = json.dumps(tmp)
    lock.release()
    return rv

async def heartbeat(connection):
    while True:
        try:
            await asyncio.sleep(15)
            await connection.send('ping')
        except websockets.exceptions.ConnectionClosed:
            print('Connection with server closed.')
            break
        except Exception as inst:
            print(type(inst))    # the exception instance
            print(inst.args)     # arguments stored in .args
            print(inst)  
            break
            
v_measure_received = 0    
async def listen(connection):
    global sessionid,lock,itemUpdated,websocket,always_monitored,instances
    global v_measure_received
    while True:
        try :
            response = await connection.recv()
            
            dictlist = json.loads(response)

            if "d" in dictlist:
                data  = dictlist["d"]
                if "file" in dictlist:
                    filename = dictlist["file"]
                    databytes = base64.b64decode(data)
                    scriptfile = inspect.getframeinfo(inspect.currentframe()).filename
                    writeablepath = os.path.dirname(os.path.abspath(scriptfile))
                    client = connection.remote_address
                    clientstr = ''
                    s = str(client[0]).replace('.','_').replace(':','_')
                    clientstr += s
                    clientstr += '_'
                    if not os.access(writeablepath, os.W_OK | os.X_OK):
                        writeablepath = expanduser("~")
                    
                    fullpath = os.path.join(writeablepath,clientstr+filename)

                    with open(fullpath, "wb") as file:
                        file.write(databytes)
                        print(fullpath)
                    continue

            for adict in dictlist:
                #print(adict)
                if "trigger" in dictlist:
                    if dictlist["trigger"] == "false":
                        #print(dictlist)
                        # this is our heartbeat, we don't need to print it
                        pass
                    continue
                
                
                if "t" in adict:
                    if adict["t"] == "info":
                        #print(adict)
                        pass
                    if adict["t"] == "response":
                        #print(adict)
                        pass
                    if "c" in adict:
                        contentlist = adict["c"]
                        for c in contentlist:
                            lac = CC2xlib.json_data.getshortlac(c["d"]["p"])
                            timestamp = c["d"]["t"]
                            someinstancesubscribed = 0
                            lock.acquire()
                            for inst in instances:
                                if lac in  inst.channels_handled:
                                    someinstancesubscribed = 1
                            lock.release()
                            if (someinstancesubscribed or (lac in always_monitored)):
                                command = c["d"]["i"]
                                if not ( command in ["Status.currentMeasure","Status.heartBeat","System.time","Status.temperature0","Status.temperature1"]): #,"Status.voltageMeasure"]):
                                    if command == "Status.voltageMeasure":
                                        v_measure_received = v_measure_received + 1
                                        if not (v_measure_received % 5) :
                                            print(c["d"])
                                    else:
                                        print(c["d"])
                                value = c["d"]["v"]
                                unit =  c["d"]["u"]
                                vu = {"v":value, "u": unit}
                                if lac == always_monitored[1]:
                                    maxconnections = 2
                                    if (command == "Status.connectedClients" and int(value) > maxconnections):
                                        print("only "+str(maxconnections)+ " client(s) connection allowed.")
                                        await connection.close()
                                    continue
                                lenrr = 0
                                ourdict = {}
                                lock.acquire()
                                if lac in itemUpdated:
                                    ourdict = itemUpdated[lac]
                                # this is a dict again, and that we will update
                                ourdict[command] = vu
                                itemUpdated[lac] = ourdict
                                lock.release()

                                if lac == always_monitored[0]:
                                    continue
                                lock.acquire()
                                for inst in instances:
                                    if inst.waitstring :
                                        if not inst.waitstringmintime:
                                            inst.waitstringmintime = timestamp
                                        obj = json.loads(inst.waitstring)
                                        allrequestedok = True
                                        for item in obj:
                                            if item =='GROUP':
                                                continue

                                            groupnames = obj['GROUP']
                                            groupname = groupnames[0]
                                            requestedvalues = obj[item]
                                            channels = inst.getChannels(groupname)
                                            k = 0
                                               
                                            for ch in channels:
                                                if ch in itemUpdated:
                                                    od2 = itemUpdated[ch]
                                                    if item in od2:
                                                        v = od2[item]
                                                        if v['v'].isalpha():
                                                            if v['v'] != str(requestedvalues[k]):
                                                                 allrequestedok = False
                                                        else:
                                                            if float(v['v']) != float(requestedvalues[k]):
                                                                 allrequestedok = False

                                                        if (float(timestamp) < float(inst.waitstringmintime)):
                                                            allrequestedok = False
                                                    else :
                                                        allrequestedok = False
                                                        pass
                                                else :
                                                    allrequestedok = False
                                                    pass
                                                k = k + 1
                                        if allrequestedok:
                                            inst.state= "FINISHED:"+inst.waitstring
                                            print(inst.state)
                                            inst.waitstring = ''
                                            inst.waitstringmintime = ''
                                lock.release()

                                



                  # fill out our log with the results
        except Exception as inst:
            print(type(inst))    # the exception instance
            print(inst.args)     # arguments stored in .args
            print(inst)          # __str__ allows args to be printed directly,
            break
   
    lock.acquire()
    websocket = None
    lock.release() 


async def login(address,user,password):
    timeout = 5
    global websocket
    global sessionid
    global state
    try:
        websocket = await asyncio.wait_for(websockets.connect('ws://'+address+':8080'), timeout)
        cmd = CC2xlib.json_data.login(user,password)
        await websocket.send(cmd)
        response = await websocket.recv()
        adict = json.loads(response)
        lock.acquire()
        sessionid = adict["i"]
        state = states.ON
        lock.release()
        
        
    except  ConnectionTimeoutError:
        print("Connection timeout")
        
        lock.acquire()
        websocket = None
        state = states.FAULT
        lock.release()
   

async def fetch(session, url):
    async with session.get(url,timeout = 5) as response:
        assert response.status == 200
        return await response.read()



async def getItemsInfo(address):
    async with aiohttp.ClientSession() as session:
        url = 'http://'+address+'/api/getItemsInfo'
        html = await fetch(session, url)
        scriptfile = inspect.getframeinfo(inspect.currentframe()).filename
        writeablepath = os.path.dirname(os.path.abspath(scriptfile))
        pre = address.replace('.',"_")
        pre += "_"
        if not os.access(writeablepath, os.W_OK | os.X_OK):
            writeablepath = expanduser("~")
        fullpath = os.path.join(writeablepath, pre+"getItemsInfo.xml")
        with open(fullpath, "wb") as file:
            file.write(html)
            print(fullpath)

async def getConfig():
    global websocket, sessionid
    cmd = CC2xlib.json_data.getConfig(sessionid)
    await websocket.send(cmd)
               

async def execute_request(requestobjlist):
    global websocket, sessionid
    cmd = CC2xlib.json_data.request(sessionid, requestobjlist)
    await websocket.send(cmd)
    return True
        
       
monitored = []

def monitor(address,user,password):
    global websocket, state, loop
    asyncio.set_event_loop(loop)
    #ping.ping(address)
    loop.run_until_complete(login(address,user,password))
    tmpstate = states.UNKNOWN
    lock.acquire()
    tmpstate = state
    lock.release()
    if tmpstate != states.ON:
        return
    lock.acquire()
    monitored.append(address)
    lock.release()
    loop.run_until_complete(getItemsInfo(address))
    loop.run_until_complete(getConfig())
    try :
        future1 = asyncio.ensure_future(heartbeat(websocket))
        future2 = asyncio.ensure_future(listen(websocket))
        loop.run_until_complete(asyncio.gather(future1,future2))
    except:
        pass
    lock.acquire()
    monitored.remove(address)
    lock.release()

loop = None

def add_monitor(ipaddress,user,password):
    global loop
    alreadyrunning = False;
    lmon = 0
    lock.acquire()
    lmon = len(monitored)
    if ipaddress in monitored:
        alreadyrunning = True
    lock.release()
    if alreadyrunning: 
        return
    if lmon :
        raise Exception("Unsupported: multiple ip addresses")
    loop = asyncio.get_event_loop()
    
    t = threading.Thread(target=monitor, args=(ipaddress,user,password,))
    t.start()
   

def queue_request(rol):
    global sessionid, state, loop
    if len(rol) == 0: return
    sid =''
    tmpstate = states.UNKNOWN
    while sid == '':
        lock.acquire()
        tmpstate = state
        sid = sessionid
        lock.release()
        if tmpstate == states.FAULT:
            return # no action
    future = asyncio.run_coroutine_threadsafe(execute_request(rol), loop)
    timeout = 15
    try :
        result = future.result(timeout)
    except asyncio.TimeoutError:
        print('The coroutine took too long, cancelling the task...')
        future.cancel()
    except Exception as exc:
        print(f'The coroutine raised an exception: {exc!r}')
    else:
        return result



    
    
   
    