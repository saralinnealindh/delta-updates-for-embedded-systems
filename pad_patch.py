import os

def padded_hex(s,p):
    return '0x' + s[2:].zfill(p)

path='patches/patch.bin'
size=os.stat(path).st_size 
f=open(path,'r+b')
f.seek(0)
padding = 'NEWPATCH' + padded_hex(hex(size),14)
f.write(padding.encode())