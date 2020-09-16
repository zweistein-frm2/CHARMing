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
from urllib.request import urlopen
import time
from concurrent.futures import TimeoutError as ConnectionTimeoutError
import base64
import os
import inspect
from os.path import expanduser

import entangle.device.iseg.CC2xlib.json_data as json_data
import entangle.device.iseg.CC2xlib.ping as ping
from entangle.core import states
import threading
 
lock = threading.Lock()

sessionid = ''
websocket = None

itemUpdated = {}
state = states.UNKNOWN

monitored_channels = []

channelfolders = []




async def heartbeat(connection):
    while True:
            try:
                await connection.send('ping')
                await asyncio.sleep(15)
            except websockets.exceptions.ConnectionClosed:
                print('Connection with server closed')
                break
            except Exception as inst:
                print(type(inst))    # the exception instance
                print(inst.args)     # arguments stored in .args
                print(inst)  
                break
            
    
async def listen(connection):
    global sessionid,lock,itemUpdated
    while True:
        try :
            response = await connection.recv()
            
            dictlist = json.loads(response)

            if "d" in dictlist:
                    data  = dictlist["d"]
                    if "file" in dictlist:
                        filename = dictlist["file"]
                        bytes = base64.b64decode(data)
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
                            file.write(bytes)
                            print(fullpath)
                        continue

            for dict in dictlist:
                #print(dict)
                if "trigger" in dictlist:
                    if dictlist["trigger"] == "false":
                        #print(dictlist)
                        pass
                    continue
                
                
                if "t" in dict:
                       if dict["t"] == "info":
                           #print(dict)
                           pass
                       if dict["t"] == "response":
                           #print(dict)
                           pass
                       if "c" in dict:
                           contentlist = dict["c"]
                           for c in contentlist:
                               lac = json_data.getshortlac(c["d"]["p"])
                               if lac in monitored_channels:
                                   print(c["d"])
                                   command = c["d"]["i"]
                                   value = c["d"]["v"]
                                   unit =  c["d"]["u"]

                                   vu = {"v":value, "u": unit}
                                  

                                   if lac == "__":
                                       if (command == "Status.connectedClients" and int(value) > 1):
                                           print("only one client connection allowed.")
                                           await connection.close()
                                       continue
                                   lock.acquire()

                                   ourdict = {}
                                   if lac in itemUpdated:
                                       ourdict = itemUpdated[lac]
                                   # this is a dict again, and that we will update
                                   ourdict[command] = vu
                                   itemUpdated[lac] = ourdict
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
        cmd = json_data.login(user,password)
        await websocket.send(cmd)
        response = await websocket.recv()
        dict = json.loads(response)
        lock.acquire()
        sessionid = dict["i"]
        state = states.ON
        lock.release()
        
        
    except  ConnectionTimeoutError as e:
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
        cmd = json_data.getConfig(sessionid)
        await websocket.send(cmd)
               

async def execute_request(requestobjlist):
        global websocket, sessionid
        cmd = json_data.request(sessionid, requestobjlist)
        await websocket.send(cmd)
        
       
monitored = []

def monitor(address,user,password,loop):
    global websocket, state
    asyncio.set_event_loop(loop)
    #ping.ping(address)
    loop.run_until_complete(login(address,user,password))
    tmpstate = states.UNKNOWN
    lock.acquire()
    tmpstate = state
    lock.release()
    if tmpstate != states.ON:
        return

    monitored.append(address)
    
    #loop.run_until_complete(getItemsInfo(address))
    #loop.run_until_complete(getConfig())

    future1 = asyncio.ensure_future(heartbeat(websocket))
    future2 = asyncio.ensure_future(listen(websocket))
    loop.run_until_complete(asyncio.gather(future1,future2))
    
    monitored.remove(address)

loop = None

def add_monitor(ipaddress,user,password):
    global loop, add_monitor_init_done
    if ipaddress in monitored:
        return
    if len(monitored) :
        raise Exception("Unsupported: multiple ip addresses")
    loop = asyncio.get_event_loop()
    
    t = threading.Thread(target=monitor, args=(ipaddress,user,password,loop,))
    t.start()
   

def queue_request(rol):
    global sessionid, state
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
  
    
   
    