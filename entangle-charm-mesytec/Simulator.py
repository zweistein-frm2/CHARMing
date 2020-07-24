from entangle import base
from entangle.core import states, Prop, uint16, Attr,Cmd
from entangle.core.defs import uint64, int32, boolean, listof
from entangle.core.errors import InvalidValue, InvalidOperation, \
    ConfigurationError


class Simulator(base.MLZDevice):


    attributes = {
        'NucleoRate':
            Attr(int32,'simulator data rate',
                 writable=True,memorized=False,  disallowed_read=(states.INIT, states.UNKNOWN,),
                 disallowed_write=( states.OFF, states.INIT, states.UNKNOWN,)),
    }

    def read_NucleoRate(self):
        if msmtsystem.msmtsystem:
            return msmtsystem.msmtsystem.simulatorRate

    def write_NucleoRate(self,value):
        if msmtsystem.msmtsystem:
            msmtsystem.msmtsystem.simulatorRate=value

    def get_NucleoRate_unit(self):
        return 'Events/second'

    def read_version(self):
        ver = super().read_version();
        if not msmtsystem.msmtsystem:
            return ver
        return ver + " "+msmtsystem.msmtsystem.version


