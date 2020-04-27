#!/usr/bin/env python3

"""Application for playing back recorded raw values through an I2C DAC."""

__author__ = "Raido Pahtma"
__copyright__ = "ProLab"

import datetime
import json
import math
import sys
import threading
import time

from smbus2 import SMBusWrapper

import ctypes
c_uint8 = ctypes.c_uint8
c_uint16 = ctypes.c_uint16
c_int16 = ctypes.c_int16


class DacFastWriteBits(ctypes.BigEndianStructure):
    _fields_ = [
            ("_15", c_uint8, 1),
            ("_14", c_uint8, 1),
            ("pd1", c_uint8, 1),
            ("pd2", c_uint8, 1),
            ("value", c_uint16, 12),
        ]

    def __str__(self):
        s = ""
        for field_name, field_type, l in self._fields_:
            if not field_name.startswith("_"):
                s += "%s: %d\n" % (field_name, getattr(self, field_name))
        return s


class DacFastWriteBytes(ctypes.BigEndianStructure):
    _fields_ = [
            ("h", c_uint8, 8),
            ("l", c_uint8, 8)
        ]


class DacFastWrite(ctypes.Union):
    _fields_ = [("bits", DacFastWriteBits),
                ("bytes", DacFastWriteBytes),
                ("sreg", c_uint16)]


class DacWriter(threading.Thread):

    def __init__(self, busnum, address, frequency, data):
        super(DacWriter, self).__init__()

        self._bus = busnum
        self._address = address
        self._frequency = frequency
        self._input = data

        self.interrupted = threading.Event()

    def join(self, timeout=0):
        self.interrupted.set()
        super(DacWriter, self).join(timeout)

    def run(self):
        configured = False

        data = []
        with open(self._input, 'r') as f:
            for l in f.readlines():
                try:
                    data.append(int(l))
                except ValueError:
                    print("bad line: {}".format(l))

        with SMBusWrapper(self._bus) as bus:
            sample = 0
            st = t = time.time()
            while not self.interrupted.is_set():
                while time.time() - t < 1.0 / self._frequency:
                    pass
                t = time.time()

                try:
                    dfw = DacFastWrite()
                    dfw.sreg = 0
                    dfw.bits.value = data[sample]

                    sample += 1
                    if sample >= len(data):
                        sample = 0
                        print("avg freq: {}".format(len(data)/(time.time()-st)))
                        st = time.time()
                        sample = 0

                    #bus.write_i2c_block_data(self._address, dfw.bytes.h, (dfw.bytes.l,))
                    bus.write_byte_data(self._address, dfw.bytes.h, dfw.bytes.l)

                except IOError as e:
                    print("-- %s -- I2C ERROR" % time.time())
                    print(e)
                    time.sleep(5)
                except Exception as e:
                    print(e)
                    time.sleep(5)


def main():
    from argparse import ArgumentParser
    parser = ArgumentParser(description="DacWriter")
    parser.add_argument("--bus", default="6", help="i2c bus")
    parser.add_argument("--address", default=0x62, help="dac i2c address")
    parser.add_argument("--frequency", type=int, default=3000, help="Update frequency, Hz")
    parser.add_argument("input", default="samples.txt", help="Input data")

    args = parser.parse_args()

    dac = DacWriter(args.bus, args.address, int(args.frequency), args.input)

    logfile = None

    dac.start()

    interrupted = False
    while not interrupted:
        try:
            time.sleep(1)
        except KeyboardInterrupt:
            print("interrupted")
            dac.join()
            interrupted = True

    if logfile is not None:
        logfile.close()
    sys.stdout.flush()


if __name__ == '__main__':
    main()
