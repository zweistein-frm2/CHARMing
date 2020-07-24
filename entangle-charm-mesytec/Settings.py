from entangle import base
from entangle.core import states, Prop, uint16, Attr,Cmd
from entangle.core.defs import uint64, int32, boolean, listof
from entangle.core.errors import InvalidValue, InvalidOperation, \
    ConfigurationError


from entangle.device.charming import msmtsystem

class Settings(base.MLZDevice):
    attributes = {
        'writelistmode':
            Attr(boolean,'write mesytec listmode to file',
                 writable=True,memorized=False,  disallowed_read=(states.INIT, states.UNKNOWN,),
                 disallowed_write=( states.OFF, states.INIT, states.UNKNOWN,)),
    }

    def read_writelistmode(self):
        if msmtsystem.msmtsystem:
            return msmtsystem.msmtsystem.writelistmode

    def write_writelistmode(self,value):
        if msmtsystem.msmtsystem:
            msmtsystem.msmtsystem.writelistmode=value

    def get_writelistmode_unit(self):
        return ''

    def read_version(self):
        ver = super().read_version();
        if not msmtsystem.msmtsystem:
            return ver
        return ver + " "+msmtsystem.msmtsystem.version