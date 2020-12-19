import os
import sys

def padded_hex(s,p):
    return '0x' + s[2:].zfill(p)

def start(patchname):
    path='patches/'+patchname+'.bin'
    size=os.stat(path).st_size 
    f=open(path,'r+b')
    contents = f.read()
    f.seek(0)
    padding = 'NEWPATCH' + padded_hex(hex(size),14)
    f.write(padding.encode())
    f.write(contents)

if __name__ == "__main__":
    start(sys.argv[1])