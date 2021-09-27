import os
import sys

def padded_hex(s,p):
    return '0x' + s[2:].zfill(p)

def start(path,max_size,input_header_size):
    header_size = max(input_header_size,8)
    size=os.stat(path).st_size 
    f=open(path,'r+b')
    contents = f.read()
    f.seek(0)
    f.write('NEWP'.encode()) 
    f.write(size.to_bytes(header_size-4,byteorder='little'))
    f.write(contents)

    print("Patch size: " + hex(size) + " + " + hex(header_size) + " (header)")

    if((max_size-header_size)<size):
        print("WARNING: Patch too large for patch partition!")

if __name__ == "__main__":
    start(sys.argv[1],int(sys.argv[2],0),int(sys.argv[3],0))