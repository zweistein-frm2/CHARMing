import ctypes
import json
from entangle import base
from entangle.core import uint32
import entangle.device.charming.msmtsystem as msmtsystem



class CmdProcessor(object):
    lastcmd = ''
    def read_availableLines(self):
        roilist = self.get_roidata()
        if not roilist:
            return 0
        return len(roilist)

    def read_availableChars(self):
        rv = ctypes.c_ulong(-1)
        return rv.value

    def Write(self, msg: str)->uint32:
        self.lastcmd = msg.rstrip()
        #print('CmdProcessor.Write('+self.lastcmd + ')')

        tok = self.lastcmd.split(':')
        if not tok:
            return 0
        if len(tok) < 2:
            return 0
        #print("tok[0]="+str(tok[0]))
        if tok[0].startswith('ROI'):
            istr = tok[0][3:].strip()
            if not istr:
                return 0
            itr = int(istr)

        roi = self.lastcmd[len(tok[0])+1:]
        self.write_roi(roi, itr)
        return len(msg)

    def ReadLine(self):
        #print('CmdProcessor.ReadLine() lastcmd == ' + self.lastcmd)
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



class RoiManager(CmdProcessor, base.StringIO):

    def init(self):
        pass

    def state(self):
        _state = base.StringIO.state(self)
        roil = self.get_roidata()
        msg = ''

        if roil:
            msg += '['
            i = 0
            for tup in roil:
                if i:
                    msg += ','
                msg += tup[0]
                i = i + 1
            msg += ']'


        return (_state[0], msg)

    def get_roidata(self):
        if not msmtsystem.msmtsystem:
            return None
        l = msmtsystem.msmtsystem.getHistogram().getRoiData()
        return l

    # pylint: disable=inconsistent-return-statements
    def write_roi(self, wkt, selecteditem):
        if msmtsystem.msmtsystem:
            #print(write_roi)
            roilist = self.get_roidata()
            if len(roilist) > selecteditem + 1:
                print("index "+str(selecteditem) + " above maximum")
                return None
            if len(roilist) == selecteditem:
                msmtsystem.msmtsystem.getHistogram().getRoi(selecteditem) # creates new Roi
            msmtsystem.msmtsystem.getHistogram().setRoi(wkt, selecteditem)
