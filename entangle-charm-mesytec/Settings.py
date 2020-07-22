class Settings(base.MLZDevice):
    attributes = {
        'writelistmode':
            Attr(boolean,'write mesytec listmode to file',
                 writable=True,memorized=False,  disallowed_read=(states.INIT, states.UNKNOWN,),
                 disallowed_write=( states.OFF, states.INIT, states.UNKNOWN,)),
    }

    def read_writelistmode(self):
        global msmtsystem
        if msmtsystem:
            return msmtsystem.writelistmode

    def write_writelistmode(self,value):
        global msmtsystem
        if msmtsystem:
            msmtsystem.writelistmode=value

    def get_writelistmode_unit(self):
        return ''

    def read_version(self):
        ver = super().read_version();
        if not msmtsystem:
            return ver
        return ver + " "+msmtsystem.version