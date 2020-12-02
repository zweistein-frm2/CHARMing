import json
from entangle import base
from entangle.core import states, Prop, Attr, Cmd, pair, listof, uint32, boolean

import entangle.device.charming.msmtsystem as msmtsystem


class CmdProcessor(object):
    lastcmd = ''
    def read_availableLines(self):
        roilist = self.get_roidata()
        if not roilist:
            return 0
        return len(roilist)

    def read_availableChars(self):
        return -1

    def Write(self, msg:str)->uint32:
        self.lastcmd = msg.rstrip()
        print('CmdProcessor.Write('+self.lastcmd + ')')

        tok = self.lastcmd.split(':')
        if not tok:
            return 0
        if len(tok) < 2:
            return 0
        print("tok[0]="+str(tok[0]))
        if tok[0].startswith('ROI'):
            istr = tok[0][3:].strip()
            if not istr:
                return 0
            itr = int(istr)

        roi = self.lastcmd[len(tok[0])+1:]
        if roi:
            self.write_roi(roi,itr)
        return len(msg)

    def ReadLine(self):
        print('CmdProcessor.ReadLine() lastcmd == ' + self.lastcmd)
        if not self.lastcmd:
            return ''
        if self.lastcmd == '?':
            return self._state[1]
        tmp = self.lastcmd.rstrip()
        cmd = 'ROI'
        index = tmp.find(cmd)
        if index < 0:
            return ''

        tok = (tmp[index+len(cmd):].strip()).split(':')
        itr = int(tok[0])
        roilist = self.get_roidata()

        if not roilist:
            return ''
        if len(roilist) <= itr:
            return ''
        return json.dumps(roilist[itr])



class RoiManager(CmdProcessor,base.StringIO):

    def init(self):
        pass

    def get_roidata(self):
        if not msmtsystem.msmtsystem:
            return None
        l = msmtsystem.msmtsystem.getHistogram().getRoiData()
        return l

    def write_roi(self, wkt, selecteditem):
        if msmtsystem.msmtsystem:
            #print(write_roi)
            roilist = self.get_roidata()
            if len(roilist) > selecteditem:
                print("index "+str(selecteditem) + " above maximum")
                return None
            if len(roilist) == selecteditem:
                msmtsystem.msmtsystem.getHistogram().getRoi(selecteditem) # creates new Roi
            msmtsystem.msmtsystem.getHistogram().setRoi(wkt,selecteditem)


