"""Static compatibility audit. Reads only the executable, never account data."""
import hashlib
from pathlib import Path
import struct
import sys

EXPECTED = {'d8fc8b43006118b9e51572581dad00db498acef100b45a75d01d0e3791eb63e7', '3d019e9ee7f220b5fd90ba33e538130a19774159e4cda0cbfd65563618e388d5'}


def audit(path):
    data = Path(path).read_bytes()
    assert hashlib.sha256(data).hexdigest() in EXPECTED, 'Unsupported executable hash'
    nt = struct.unpack_from('<I', data, 0x3C)[0]
    assert data[nt:nt + 4] == b'PE\0\0'
    machine, count, timestamp = struct.unpack_from('<HHI', data, nt + 4)
    assert machine == 0x14C and timestamp == 0x4510B6DB
    optional = nt + 24
    assert struct.unpack_from('<I', data, optional + 28)[0] == 0x400000
    assert struct.unpack_from('<I', data, optional + 56)[0] == 0x92B000
    table = optional + struct.unpack_from('<H', data, nt + 20)[0]
    sections = [struct.unpack_from('<IIII', data, table + i * 40 + 8) for i in range(count)]

    def offset(address):
        rva = address - 0x400000
        for virtual_size, start, raw_size, raw in sections:
            if start <= rva < start + max(virtual_size, raw_size):
                return raw + rva - start
        raise AssertionError(f'Unmapped address {address:x}')

    def signature(address, hex_bytes):
        expected = bytes.fromhex(hex_bytes)
        start = offset(address)
        assert data[start:start + len(expected)] == expected, f'Changed instruction at {address:x}'

    def call(address, destination):
        start = offset(address)
        assert data[start] == 0xE8
        assert address + 5 + struct.unpack_from('<i', data, start + 1)[0] == destination

    signature(0x5379A0, '55 8b ec 56 8b f1 83 7e 70 06 57 75 78')
    signature(0x537AA0, '55 8b ec 8b 15 6c d4 c0 00 53 8b 5d 0c 56 57 8d')
    signature(0x704120, '56 57 8b f9 e8 a7 ff ff ff 8b f0 6a 00 8b ce')
    signature(0x4E7686, '8b 80 68 0e 00 00 8a 98 2a 10 00 00')
    # 0x129 fills a login-only seed. Live slot getters and the local setter
    # instead use 0xBC6980. Setter arguments are (slot, raw, upload, suppress).
    signature(0x5E6689, 'be b4 c2 c4 00 bf 78 00 00 00 56 8b cb')
    signature(0x4E6DD3, '8b 04 b5 80 69 bc 00')
    signature(0x4E5D60, '55 8b ec 56 8b f1 85 f6 7c 40 83 fe 78 73 3b 85 d2 89 14 b5 80 69 bc 00')
    signature(0x4E5D9C, '8b 4d 0c 8b 55 08 51 8b ce')
    call(0x4E5DA5, 0x4E58E0)
    signature(0x4E5908, '3b df 89 04 b5 60 6b bc 00 8b 45 fc 89 04 b5 a0 67 bc 00 74 6a')
    signature(0x4E5987, '39 7d 08 75 14 46 56 68 54 51 83 00 68 d5 00 00 00')
    signature(0x5DEAF7, '8b 16 8b 4d fc 6a 01 6a 00')
    call(0x5DEB00, 0x4E5D60)
    signature(0x5DEB14, '6a 00 68 54 51 83 00 68 d5 00 00 00')
    call(0x5DEB20, 0x703F50)
    signature(0x703F50, '55 8b ec 8b 15 68 ef ce 00')
    # The stock setter composes four low bits then sends opcode 0x2BF.
    signature(0x4E771D, '68 bf 02 00 00')
    # Login UI executes a fixed Lua chunk in the caller's glue state.
    signature(0x6F5690, '55 8b ec 83 ec 08 8b 45 08 83 f8 03 72 17 80 3a')
    signature(0x6F41A0, '55 8b ec 83 ec 08 8b 45 0c 85 c0 56 57 75 04 33')
    # Account-data initialization consumes eight 16-byte MD5 entries.
    signature(0x5AF901, '6a 10 8d 4d f0 51 8b cb e8')
    signature(0x5AF920, '83 c6 2c 47 81 fe 50 de 85 00')
    signature(0x5AF940, '55 8b ec 81 ec 58 04 00 00 56 57 8b 7d 08 33 c0')
    # ReloadUI's client loop runs native teardown and complete initialization.
    call(0x495664, 0x490BD0)
    call(0x495669, 0x48FBF0)
    # Teardown saves macros, destroys macro objects, and destroys bindings.
    call(0x490C5A, 0x4F0C60)
    call(0x490C5F, 0x4EFFB0)
    call(0x490CB5, 0x4B6100)
    # Account/character binding and macro loaders use the disk/cache reader.
    call(0x4B62EF, 0x5AFE50)
    call(0x4B6300, 0x5AFE50)
    call(0x4F0A7C, 0x5AFE50)
    call(0x4F0A8D, 0x5AFE50)
    signature(0x5AFE87, '57 6a 00')  # reader opens the first file for reading
    # SaveBindings accepts 1/2 and updates the active binding set before
    # saving. Setup switches use set 2 to preserve shared account bindings.
    signature(0x4B8275, '83 fe 01 74 18 83 fe 02 74 13')
    signature(0x4B829D, '89 b7 d0 00 00 00')
    call(0x4B82A3, 0x4B6310)
    print('PASS: pinned client hash, PE metadata, native hook ABI signatures, multibar field, ReloadUI, and character binding-set save behavior.')


if __name__ == '__main__':
    if len(sys.argv) != 2:
        raise SystemExit('Usage: python tests/audit_client.py PATH/TO/WoW.exe')
    audit(sys.argv[1])
