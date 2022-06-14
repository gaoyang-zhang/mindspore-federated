# automatically generated by the FlatBuffers compiler, do not modify

# namespace: schema

import flatbuffers
from flatbuffers.compat import import_numpy
np = import_numpy()

class ReturnExchangeKeys(object):
    __slots__ = ['_tab']

    @classmethod
    def GetRootAs(cls, buf, offset=0):
        n = flatbuffers.encode.Get(flatbuffers.packer.uoffset, buf, offset)
        x = ReturnExchangeKeys()
        x.Init(buf, n + offset)
        return x

    @classmethod
    def GetRootAsReturnExchangeKeys(cls, buf, offset=0):
        """This method is deprecated. Please switch to GetRootAs."""
        return cls.GetRootAs(buf, offset)
    # ReturnExchangeKeys
    def Init(self, buf, pos):
        self._tab = flatbuffers.table.Table(buf, pos)

    # ReturnExchangeKeys
    def Retcode(self):
        o = flatbuffers.number_types.UOffsetTFlags.py_type(self._tab.Offset(4))
        if o != 0:
            return self._tab.Get(flatbuffers.number_types.Int32Flags, o + self._tab.Pos)
        return 0

    # ReturnExchangeKeys
    def Iteration(self):
        o = flatbuffers.number_types.UOffsetTFlags.py_type(self._tab.Offset(6))
        if o != 0:
            return self._tab.Get(flatbuffers.number_types.Int32Flags, o + self._tab.Pos)
        return 0

    # ReturnExchangeKeys
    def RemotePublickeys(self, j):
        o = flatbuffers.number_types.UOffsetTFlags.py_type(self._tab.Offset(8))
        if o != 0:
            x = self._tab.Vector(o)
            x += flatbuffers.number_types.UOffsetTFlags.py_type(j) * 4
            x = self._tab.Indirect(x)
            from mindspore_fl.schema.ClientPublicKeys import ClientPublicKeys
            obj = ClientPublicKeys()
            obj.Init(self._tab.Bytes, x)
            return obj
        return None

    # ReturnExchangeKeys
    def RemotePublickeysLength(self):
        o = flatbuffers.number_types.UOffsetTFlags.py_type(self._tab.Offset(8))
        if o != 0:
            return self._tab.VectorLen(o)
        return 0

    # ReturnExchangeKeys
    def RemotePublickeysIsNone(self):
        o = flatbuffers.number_types.UOffsetTFlags.py_type(self._tab.Offset(8))
        return o == 0

    # ReturnExchangeKeys
    def NextReqTime(self):
        o = flatbuffers.number_types.UOffsetTFlags.py_type(self._tab.Offset(10))
        if o != 0:
            return self._tab.String(o + self._tab.Pos)
        return None

def Start(builder): builder.StartObject(4)
def ReturnExchangeKeysStart(builder):
    """This method is deprecated. Please switch to Start."""
    return Start(builder)
def AddRetcode(builder, retcode): builder.PrependInt32Slot(0, retcode, 0)
def ReturnExchangeKeysAddRetcode(builder, retcode):
    """This method is deprecated. Please switch to AddRetcode."""
    return AddRetcode(builder, retcode)
def AddIteration(builder, iteration): builder.PrependInt32Slot(1, iteration, 0)
def ReturnExchangeKeysAddIteration(builder, iteration):
    """This method is deprecated. Please switch to AddIteration."""
    return AddIteration(builder, iteration)
def AddRemotePublickeys(builder, remotePublickeys): builder.PrependUOffsetTRelativeSlot(2, flatbuffers.number_types.UOffsetTFlags.py_type(remotePublickeys), 0)
def ReturnExchangeKeysAddRemotePublickeys(builder, remotePublickeys):
    """This method is deprecated. Please switch to AddRemotePublickeys."""
    return AddRemotePublickeys(builder, remotePublickeys)
def StartRemotePublickeysVector(builder, numElems): return builder.StartVector(4, numElems, 4)
def ReturnExchangeKeysStartRemotePublickeysVector(builder, numElems):
    """This method is deprecated. Please switch to Start."""
    return StartRemotePublickeysVector(builder, numElems)
def AddNextReqTime(builder, nextReqTime): builder.PrependUOffsetTRelativeSlot(3, flatbuffers.number_types.UOffsetTFlags.py_type(nextReqTime), 0)
def ReturnExchangeKeysAddNextReqTime(builder, nextReqTime):
    """This method is deprecated. Please switch to AddNextReqTime."""
    return AddNextReqTime(builder, nextReqTime)
def End(builder): return builder.EndObject()
def ReturnExchangeKeysEnd(builder):
    """This method is deprecated. Please switch to End."""
    return End(builder)