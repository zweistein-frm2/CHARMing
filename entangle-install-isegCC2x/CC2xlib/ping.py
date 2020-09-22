#  -*- coding: utf-8 -*-
#***************************************************************************
#* Copyright (C) 2020 by Andreas Langhoff *
#* <andreas.langhoff@frm2.tum.de> *
#* This program is free software; you can redistribute it and/or modify *
#* it under the terms of the GNU General Public License v3 as published *
#* by the Free Software Foundation; *
# **************************************************************************

import subprocess
import sys


def ping(ipaddress):
    retry = 2
    cmdline = ["ping"]
    if sys.platform.startswith('win32'):
        cmdline.append('-n')
        cmdline.append(str(retry))
    else :
        cmdline.append('-c'+str(retry))
    cmdline.append(ipaddress)
    p = subprocess.Popen(cmdline,
            stdout = subprocess.PIPE,
    stderr = subprocess.PIPE
    )
    out, error = p.communicate()
    lines = out.split(b'\r\n')
    undesired = []
    desired = []
    if sys.platform.startswith('win32'):
        undesired.append(b'Request timed out')
        desired.append(b'Reply from')
    else :
        undesired.append(b'100 % packet loss')
        desired.append(b'%d' % retry + b' received')
        desired.append(b'64 bytes from')
    if(len(error)  != 0):
        return False
    for l in lines:
        for u in undesired:
            if l.find(u) != -1:
                return False
        for d in desired:
            if l.find(d) > -1:
                return True
    return True



#print("ping " +str(ping("172.25.2.24")))

