# automatically generated by the FlatBuffers compiler, do not modify

# namespace: schema

import flatbuffers
from flatbuffers.compat import import_numpy
np = import_numpy()

class RequestAsyncUpdateModel(object):
    __slots__ = ['_tab']

    @classmethod
    def GetRootAs(cls, buf, offset=0):
        n = flatbuffers.encode.Get(flatbuffers.packer.uoffset, buf, offset)
        x = RequestAsyncUpdateModel()
        x.Init(buf, n + offset)
        return x

    @classmethod
    def GetRootAsRequestAsyncUpdateModel(cls, buf, offset=0):
        """This method is deprecated. Please switch to GetRootAs."""
        return cls.GetRootAs(buf, offset)
    @classmethod
    def RequestAsyncUpdateModelBufferHasIdentifier(cls, buf, offset, size_prefixed=False):
        return flatbuffers.util.BufferHasIdentifier(buf, offset, b"\x46\x4C\x4A\x30", size_prefixed=size_prefixed)

    # RequestAsyncUpdateModel
    def Init(self, buf, pos):
        self._tab = flatbuffers.table.Table(buf, pos)

    # RequestAsyncUpdateModel
    def FlName(self):
        o = flatbuffers.number_types.UOffsetTFlags.py_type(self._tab.Offset(4))
        if o != 0:
            return self._tab.String(o + self._tab.Pos)
        return None

    # RequestAsyncUpdateModel
    def FlId(self):
        o = flatbuffers.number_types.UOffsetTFlags.py_type(self._tab.Offset(6))
        if o != 0:
            return self._tab.String(o + self._tab.Pos)
        return None

    # RequestAsyncUpdateModel
    def Iteration(self):
        o = flatbuffers.number_types.UOffsetTFlags.py_type(self._tab.Offset(8))
        if o != 0:
            return self._tab.Get(flatbuffers.number_types.Int32Flags, o + self._tab.Pos)
        return 0

    # RequestAsyncUpdateModel
    def DataSize(self):
        o = flatbuffers.number_types.UOffsetTFlags.py_type(self._tab.Offset(10))
        if o != 0:
            return self._tab.Get(flatbuffers.number_types.Int32Flags, o + self._tab.Pos)
        return 0

    # RequestAsyncUpdateModel
    def FeatureMap(self, j):
        o = flatbuffers.number_types.UOffsetTFlags.py_type(self._tab.Offset(12))
        if o != 0:
            x = self._tab.Vector(o)
            x += flatbuffers.number_types.UOffsetTFlags.py_type(j) * 4
            x = self._tab.Indirect(x)
            from mindspore_fl.schema.FeatureMap import FeatureMap
            obj = FeatureMap()
            obj.Init(self._tab.Bytes, x)
            return obj
        return None

    # RequestAsyncUpdateModel
    def FeatureMapLength(self):
        o = flatbuffers.number_types.UOffsetTFlags.py_type(self._tab.Offset(12))
        if o != 0:
            return self._tab.VectorLen(o)
        return 0

    # RequestAsyncUpdateModel
    def FeatureMapIsNone(self):
        o = flatbuffers.number_types.UOffsetTFlags.py_type(self._tab.Offset(12))
        return o == 0

def Start(builder): builder.StartObject(5)
def RequestAsyncUpdateModelStart(builder):
    """This method is deprecated. Please switch to Start."""
    return Start(builder)
def AddFlName(builder, flName): builder.PrependUOffsetTRelativeSlot(0, flatbuffers.number_types.UOffsetTFlags.py_type(flName), 0)
def RequestAsyncUpdateModelAddFlName(builder, flName):
    """This method is deprecated. Please switch to AddFlName."""
    return AddFlName(builder, flName)
def AddFlId(builder, flId): builder.PrependUOffsetTRelativeSlot(1, flatbuffers.number_types.UOffsetTFlags.py_type(flId), 0)
def RequestAsyncUpdateModelAddFlId(builder, flId):
    """This method is deprecated. Please switch to AddFlId."""
    return AddFlId(builder, flId)
def AddIteration(builder, iteration): builder.PrependInt32Slot(2, iteration, 0)
def RequestAsyncUpdateModelAddIteration(builder, iteration):
    """This method is deprecated. Please switch to AddIteration."""
    return AddIteration(builder, iteration)
def AddDataSize(builder, dataSize): builder.PrependInt32Slot(3, dataSize, 0)
def RequestAsyncUpdateModelAddDataSize(builder, dataSize):
    """This method is deprecated. Please switch to AddDataSize."""
    return AddDataSize(builder, dataSize)
def AddFeatureMap(builder, featureMap): builder.PrependUOffsetTRelativeSlot(4, flatbuffers.number_types.UOffsetTFlags.py_type(featureMap), 0)
def RequestAsyncUpdateModelAddFeatureMap(builder, featureMap):
    """This method is deprecated. Please switch to AddFeatureMap."""
    return AddFeatureMap(builder, featureMap)
def StartFeatureMapVector(builder, numElems): return builder.StartVector(4, numElems, 4)
def RequestAsyncUpdateModelStartFeatureMapVector(builder, numElems):
    """This method is deprecated. Please switch to Start."""
    return StartFeatureMapVector(builder, numElems)
def End(builder): return builder.EndObject()
def RequestAsyncUpdateModelEnd(builder):
    """This method is deprecated. Please switch to End."""
    return End(builder)