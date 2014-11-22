#!/usr/bin/python
"""functions to generate a random number in any base, C-style output"""
import random

def to_base(num, base):
    alphabet = '0123456789abcdefghijklmnopqrstuvwxyz'
    retstr = ""
    while num > 0:
        mod = num % base
        retstr = alphabet[mod] + retstr
        num /= base
    return retstr

def to_c_format(string, chunksize):
    chunks = [ string[start:(start+chunksize)] for start in range(0, len(string), chunksize)]
    return "\t\"" + "\"\n\t  \"".join(chunks) + "\""

if __name__ == "__main__":
    import sys
    base = 10
    if len(sys.argv) > 1:
        base = int(sys.argv[1])
    num = random.getrandbits(2048)
    s = to_base(num, base)
    print to_c_format(s, 80)
