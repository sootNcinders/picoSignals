import threading
import serial
import serial.tools.list_ports
import datetime
import time
import sys
import platform
import re

display = 255

def sendCmd(addr: int, cmd: int):
    global ser
    """
    Sends a command to a destination node via radio.

    Args:
        addr (int): The destination node address, in the range 0-255.
        cmd (int): The command to send, in the range 0-255.

    Returns:
        None

    Notes:
        - The command is sent in a serial format, with the following structure:
            - Start Character: ':'
            - Destination node: encoded as a two-digit hexadecimal number
            - Command: encoded as a two-digit hexadecimal number
        - The following commands are supported:
            - 00 - ACK
            - 01 - Ping
            - 02 - Wake
            - 03 - Capture 1
            - 04 - Capture 2
            - 05 - Capture 3
            - 06 - Capture 4
            - 07 - Release 1
            - 08 - Release 2
            - 09 - Release 3
            - 0A - Release 4
            - 0B - Clear Flash
            - 0C - Reboot
    """

    out = ":"
    out = out + f'{addr:0>2x}'
    out = out + f'{cmd:0>2x}'

    out = out.encode("ASCII")

    ser.write(out)

def printResult():
    global ser
    global display

    #Serial input for map:
    #: - Start Chararcter
    #00 - Node Address, 00-FF
    #O - Head 1 - O, G, A, R, L
    #O - Head 2
    #O - Head 3
    #O - Head 4
    #0 - Captures, 0-F
    #0 - Releases, 0-F
    #0 - Turnouts, 0-F
    #00 - absolute value of average RSSI to primary partner, 0-FF
    #00 - Voltage x10, 0-FF
    #00 - Version, 0-FF
    #00 - Revision, 0-FF

    while True:
        if ser.in_waiting > 0:
            lnIn = ser.readline()
            lnIn = lnIn.decode("ASCII")

            if(lnIn[0] == ':'):
                node = int(lnIn[1] + lnIn[2], 16)
                rssi = -1 * int(lnIn[10] + lnIn[11], 16)
                voltage = (int(lnIn[12] + lnIn[13], 16)/10.0)  
                version = int(lnIn[18] + lnIn[19], 16)
                revision = int(lnIn[20] + lnIn[21], 16)

                if node == display or display == 255:
                    print("Node: " + str(node))
                    print("Head 1: " + lnIn[3])
                    print("Head 2: " + lnIn[4])
                    print("Head 3: " + lnIn[5])
                    print("Head 4: " + lnIn[6])
                    print("Captures: " + lnIn[7])
                    print("Releases: " + lnIn[8])
                    print("Turnouts: " + lnIn[9])
                    print("RSSI: " + str(rssi))
                    print("Voltage: " + str(voltage))
                    print("Version: " + str(version))
                    print("Revision: " + str(revision))
                    print(">")


def sendCmdThread():
    global ser
    global display 

    while True:
        cmd = input(">")

        print(">"+cmd)

        cmd = re.findall(r'\w+', cmd)

        if(len(cmd) == 2):
            if(cmd[0] == "ack"):
                sendCmd(int(cmd[1]), 0)
            elif(cmd[0] == "ping"):
                sendCmd(int(cmd[1]), 1)
            elif(cmd[0] == "wake"):
                sendCmd(int(cmd[1]), 2)
            elif(cmd[0] == "capture1"):
                sendCmd(int(cmd[1]), 3)
            elif(cmd[0] == "capture2"):
                sendCmd(int(cmd[1]), 4)
            elif(cmd[0] == "capture3"):
                sendCmd(int(cmd[1]), 5)
            elif(cmd[0] == "capture4"):
                sendCmd(int(cmd[1]), 6)
            elif(cmd[0] == "release1"):
                sendCmd(int(cmd[1]), 7)
            elif(cmd[0] == "release2"):
                sendCmd(int(cmd[1]), 8)
            elif(cmd[0] == "release3"):
                sendCmd(int(cmd[1]), 9)
            elif(cmd[0] == "release4"):
                sendCmd(int(cmd[1]), 10)
            elif(cmd[0] == "erase"):
                sendCmd(int(cmd[1]), 11)
            elif(cmd[0] == "reboot"):
                sendCmd(int(cmd[1]), 12)
            elif(cmd[0] == "display"):
                display = int(cmd[1])
            else:
                print("Help:")
                print("ack <addr>")
                print("ping <addr>")
                print("wake <addr>")
                print("capture1 <addr>")
                print("capture2 <addr>")
                print("capture3 <addr>")
                print("capture4 <addr>")
                print("release1 <addr>")
                print("release2 <addr>")
                print("release3 <addr>")
                print("release4 <addr>")
                print("erase <addr>")
                print("reboot <addr>")
        else:
            print("Help:")
            print("ack <addr>")
            print("ping <addr>")
            print("wake <addr>")
            print("capture1 <addr>")
            print("capture2 <addr>")
            print("capture3 <addr>")
            print("capture4 <addr>")
            print("release1 <addr>")
            print("release2 <addr>")
            print("release3 <addr>")
            print("release4 <addr>")
            print("erase <addr>")
            print("reboot <addr>")
        

print("Pico Signal CLI v1\n")

available_ports = list(serial.tools.list_ports.comports())

print("Select port:")

for port in available_ports:
    print(port.device)

port = input("\n:")

ser = serial.Serial(port, 115200, timeout=1)

rxThread = threading.Thread(target=printResult)
rxThread.daemon = True

txThread = threading.Thread(target=sendCmdThread)
txThread.daemon = False

rxThread.start()
txThread.start()