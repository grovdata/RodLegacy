# This is a generated file! Please edit source .ksy file and use kaitai-struct-compiler to rebuild

import kaitaistruct
from kaitaistruct import KaitaiStruct, KaitaiStream, BytesIO


if getattr(kaitaistruct, 'API_VERSION', (0, 9)) < (0, 9):
    raise Exception("Incompatible Kaitai Struct Python API: 0.9 or later is required, but you have %s" % (kaitaistruct.__version__))

class SrLibpAmiga(KaitaiStruct):
    def __init__(self, _io, _parent=None, _root=None):
        self._io = _io
        self._parent = _parent
        self._root = _root if _root else self
        self._read()

    def _read(self):
        self.num_sprites = self._io.read_u2be()
        self.sprites = []
        for i in range(self.num_sprites):
            self.sprites.append(SrLibpAmiga.SpriteDescriptor(self._io, self, self._root))


    class SpriteDescriptor(KaitaiStruct):
        def __init__(self, _io, _parent=None, _root=None):
            self._io = _io
            self._parent = _parent
            self._root = _root if _root else self
            self._read()

        def _read(self):
            self.width = self._io.read_u2be()
            self.height = self._io.read_u2be()
            self.size_decoded = self._io.read_u2be()
            self.type = self._io.read_u1()
            self.directory = self._io.read_bytes(16)
            self.zero = self._io.read_u1()
            self.offset = self._io.read_u2be()
            self.size_encoded = self._io.read_u2be()


import sys
ks = KaitaiStream(BytesIO(open(sys.argv[1],'rb').read()))
print(ks)

lib = SrLibpAmiga(ks)
size_all = 0
types = set()
zeros = set()
for s in lib.sprites:
    size_all += s.size_encoded
    zeros.add(s.zero)
    types.add(s.type)

print(f'total size: {size_all}, types: {types} zeros: {zeros}')
