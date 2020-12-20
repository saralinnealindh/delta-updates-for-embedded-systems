import os
import sys
from shutil import copyfile

def main(flashed_path,source_path):
    print("\n-----------------------------")
    print("Press \'y\' to set latest built image as source/currently running image.")
    print("Press any other key to keep old source file.")
    reply=input().lower().strip()

    if(reply=="y"):
        print('Updating source.\n')
        copyfile(flashed_path,source_path)
    else:
        print('Source is not updated.\n')

if __name__ == "__main__":
    main(sys.argv[1],sys.argv[2])