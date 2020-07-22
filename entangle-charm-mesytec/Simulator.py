class Simulator(base.MLZDevice):


    attributes = {
        'NucleoRate':
            Attr(int32,'simulator data rate',
                 writable=True,memorized=False,  disallowed_read=(states.INIT, states.UNKNOWN,),
                 disallowed_write=( states.OFF, states.INIT, states.UNKNOWN,)),
    }

    def read_NucleoRate(self):
        global msmtsystem
        if msmtsystem:
            return msmtsystem.simulatorRate

    def write_NucleoRate(self,value):
        global msmtsystem
        if msmtsystem:
            msmtsystem.simulatorRate=value

    def get_NucleoRate_unit(self):
        return 'Events/second'

    def read_version(self):
        ver = super().read_version();
        if not msmtsystem:
            return ver
        return ver + " "+msmtsystem.version


