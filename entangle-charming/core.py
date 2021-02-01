#!/usr/bin/env python
#                           _              _                _
#	___  __ __ __  ___     (_)     ___    | |_     ___     (_)    _ _
#   |_ /  \ V  V / / -_)    | |    (_-<    |  _|   / -_)    | |   | ' \
#  _/__|   \_/\_/  \___|   _|_|_   /__/_   _\__|   \___|   _|_|_  |_||_|
#	   .
#	   |\       Copyright (C) 2019 - 2020 by Andreas Langhoff
#	 _/]_\_                            <andreas.langhoff@frm2.tum.de>
# ~~~"~~~~~^~~   This program is free software; you can redistribute it
# and/or modify it under the terms of the GNU General Public License v3
# as published by the Free Software Foundation;

import numpy as np
from entangle import base
from entangle.core import states, uint16, Attr
from entangle.core.defs import uint64
from entangle.core.states import FAULT



import  entangle.device.charming as charming
import entangle.device.charming.msmtsystem as msmtsystem


class MeasureCounts(base.CounterChannel):

    def init(self):
        self.preselected_value = 0
    # pylint: disable=inconsistent-return-statements
    def state(self):

        if charming.msmtsystem.msmtsystem:
            t = charming.msmtsystem.msmtsystem.status()
            if not t[2] &2:
                return (FAULT, t[3])
            #if(t[2] & 1): return (BUSY,t[3])
            _state = base.ImageChannel.state(self)
            if len(t) > 4:
                return (FAULT, t[4])

            return (_state[0], t[3])



    def Start(self):
        if charming.msmtsystem.msmtsystem:
            charming.msmtsystem.msmtsystem.start()

    def Stop(self):
        if charming.msmtsystem.msmtsystem:
            charming.msmtsystem.msmtsystem.stop()

    def Prepare(self):
        pass

    def Resume(self):
        if charming.msmtsystem.msmtsystem:
            charming.msmtsystem.msmtsystem.resume()
    # pylint: disable=inconsistent-return-statements
    def read_value(self):
        if charming.msmtsystem.msmtsystem:
            t = charming.msmtsystem.msmtsystem.status()
            return t[0]
    # pylint: disable=unused-variable
    def write_preselection(self, value):
        if charming.msmtsystem:
            t = charming.msmtsystem.msmtsystem.status()
            charming.msmtsystem.msmtsystem.stopafter(value, 0)
            self.preselected_value = value

    def read_preselection(self):
        return self.preselected_value

    def read_version(self):
        ver = super().read_version()
        if not charming.msmtsystem.msmtsystem:
            return ver
        return ver + " "+charming.msmtsystem.msmtsystem.version


class MeasureTime(base.TimerChannel):

    def init(self):
        self.preselected_value = 0
    # pylint: disable=inconsistent-return-statements
    def state(self):
        if charming.msmtsystem.msmtsystem:
            t = charming.msmtsystem.msmtsystem.status()
            if not t[2] &2:
                return (FAULT, t[3])
            #if(t[2] & 1): return (BUSY,t[3])
            _state = base.ImageChannel.state(self)
            if len(t) > 4:
                return (FAULT, t[4])

            return (_state[0], t[3])

    def On(self):
        if charming.msmtsystem.msmtsystem:
            charming.msmtsystem.msmtsystem.on()
    def Off(self):
        if charming.msmtsystem.msmtsystem:
            charming.msmtsystem.msmtsystem.off()

    def Start(self):
        if charming.msmtsystem.msmtsystem:
            charming.msmtsystem.msmtsystem.start()

    def Stop(self):
        if charming.msmtsystem.msmtsystem:
            charming.msmtsystem.msmtsystem.stop()

    def Prepare(self):
        pass
    # pylint: disable=inconsistent-return-statements
    def Resume(self):
        if charming.msmtsystem.msmtsystem:
            charming.msmtsystem.msmtsystem.resume()
    # pylint: disable=inconsistent-return-statements
    def read_value(self):
        if charming.msmtsystem.msmtsystem:
            t = charming.msmtsystem.msmtsystem.status()
            return t[1]

    def write_preselection(self, value):
        if charming.msmtsystem.msmtsystem:
            #t = msmtsystem.msmtsystem.status()
            charming.msmtsystem.msmtsystem.stopafter(0, value)
            self.preselected_value = value

    def read_preselection(self):
        return self.preselected_value

    def read_version(self):
        ver = super().read_version()
        if not charming.msmtsystem.msmtsystem:
            return ver
        return ver + " "+charming.msmtsystem.msmtsystem.version


class Monitor0(base.CounterChannel):
    # pylint: disable=inconsistent-return-statements
    def read_value(self):
        if charming.msmtsystem.msmtsystem:
            t = charming.msmtsystem.msmtsystem.monitors_status()
            return t[0][1]

class Monitor1(base.CounterChannel):
    # pylint: disable=inconsistent-return-statements
    def read_value(self):
        if charming.msmtsystem.msmtsystem:
            t = charming.msmtsystem.msmtsystem.monitors_status()
            return t[1][1]

class Monitor2(base.CounterChannel):
    # pylint: disable=inconsistent-return-statements
    def read_value(self):
        if charming.msmtsystem.msmtsystem:
            t = charming.msmtsystem.msmtsystem.monitors_status()
            return t[2][1]

class Monitor3(base.CounterChannel):
    # pylint: disable=inconsistent-return-statements
    def read_value(self):
        if charming.msmtsystem.msmtsystem:
            t = charming.msmtsystem.msmtsystem.monitors_status()
            return t[3][1]

class HistogramBase(base.ImageChannel):

    attributes = {
         'CountsInRoi': Attr(uint64, 'counts in Roi', dims=0, writable=False, memorized=False, unit=None),
         'maxindexroi': Attr(uint16, 'max index of roi (0 ..n-1)', dims=0, writable=False, memorized=False, unit=None),
         'selectedRoi': Attr(uint16, 'selected Roi', dims=0, writable=True, memorized=True, unit=None, disallowed_read=(states.BUSY, states.INIT, states.UNKNOWN,),
                            disallowed_write=(states.FAULT, states.OFF, states.BUSY, states.INIT, states.UNKNOWN,)),
         'RoiWKT':   Attr(str, 'Well known text (WKT) representation of the region of interest.', writable=True, memorized=False,
                            disallowed_read=(states.BUSY, states.INIT, states.UNKNOWN,),
                            disallowed_write=(states.FAULT, states.OFF, states.BUSY, states.INIT, states.UNKNOWN,)),

    }



    def init(self):
        self.count = 0
        self.selectedRoi = 0
        self.maxindexroi = 0
        self.mat = np.zeros((1, 1, 1), dtype="int32")
        self.histogram = None

    def Histogram(self):
        raise NotImplementedError()


    def read_CountsInRoi(self):
        return uint64(self.count)

    def get_CountsInRoi_unit(self):
        return 'cts'

    def Clear(self):
        return

    def read_detectorSize(self):
        return self.Histogram().Size

    def read_roiSize(self):
        return self.read_detectorSize()

    def read_RoiWKT(self):
        #print("read_RoiWKT("+str(self.selectedRoi)+")")
        wkt = self.Histogram().getRoi(self.selectedRoi)
        return wkt

    def write_RoiWKT(self, value):
        #print("write_RoiWKT = "+str(value))
        #print("selectedRoi = "+str(self.selectedRoi))
        self.Histogram().setRoi(value, self.selectedRoi)

    def get_RoiWKT_unit(self):
        return ''

    def read_selectedRoi(self):
        return self.selectedRoi

    def write_selectedRoi(self, value):
        #print("write_selectedRoi("+str(value)+")")
        if value < 0:
            print("error, selectedRoi = "+str(value))
            return
        self.selectedRoi = value

    def get_selectedRoi_unit(self):
        return ''

    def read_maxindexroi(self):
        return self.maxindexroi

    def get_maxindexroi_unit(self):
        return ''

    def read_value(self):
        t = self.Histogram().update(self.mat)
        #t[0] is list of pair(wkt,count)
        self.mat = t[1]
        #print(len(t[0]))
        self.maxindexroi = len(t[0])-1
        if self.selectedRoi > self.maxindexroi:
            self.selectedRoi = self.maxindexroi
        #print(t[0][self.selectedRoi]);  #should be pair wkt/counts
        self.count = t[0][self.selectedRoi][1]
        #t[2] is list of Monitors
        return self.mat.flatten()

    def read_version(self):
        ver = super().read_version()
        if not charming.msmtsystem.msmtsystem:
            return ver
        return ver + " "+charming.msmtsystem.msmtsystem.version


class Histogram(HistogramBase):

    def Histogram(self):
        if msmtsystem.msmtsystem:
            self.histogram = charming.msmtsystem.msmtsystem.getHistogram()
        else:
            return None
        return self.histogram


class HistogramRaw(HistogramBase):

    def Histogram(self):
        RAWHISTOGRAM = 1
        if msmtsystem.msmtsystem:
            self.histogram = charming.msmtsystem.msmtsystem.getHistogram(RAWHISTOGRAM)
        else:
            return None
        return self.histogram

    def read_value(self):
        self.Histogram().nextRAW = True
        return super().read_value()

    def read_detectorSize(self):
        self.Histogram().nextRAW = True
        return super().read_detectorSize()

    def read_RoiWKT(self):
        #print("read_RoiWKT("+str(self.selectedRoi)+")")
        self.Histogram().nextRAW = True
        return super().read_RoiWKT()


    def write_RoiWKT(self, value):
        self.Histogram().nextRAW = True
        return super().write_RoiWKT(value)
