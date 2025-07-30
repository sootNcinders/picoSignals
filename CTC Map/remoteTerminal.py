import threading
import serial
import serial.tools.list_ports
import datetime
import time
import sys
import platform

#list all available ports
available_ports = list(serial.tools.list_ports.comports())
serial_ports = []
for port in available_ports:
    serial_ports.append(port.device)

if len(serial_ports) == 0:
    print("NO SERIAL DEVICES")
    sys.exit("NO SERIAL DEVICES")

print("Available serial ports:")
for i, port in enumerate(serial_ports):
    print(f"{i}: {port}")

# Select a port
port_index = int(input("Select a port by index: "))

if port_index < 0 or port_index >= len(serial_ports):
    sys.exit("Invalid port index")

term = serial.Serial(serial_ports[port_index], 115200, timeout=1)

print(f"Connected to {serial_ports[port_index]}")

dest = 0
printHeader = True

def read_serial():
    global dest
    global term
    global printHeader
    lnin = ""
    src = 0

    while True:
        try:
            lnin = term.readline()
            lnin = lnin.decode("ASCII")

            if "~" in lnin:
                src = int(lnin[1:3], 16)
                lnin = lnin[3:]
                
                if lnin[0] != '\x00':
                    if printHeader:
                        printHeader = False
                        print("\r\n", end="")
                        print(src, end="")
                        print(": ", end="")

                    for i in range(0, len(lnin)):
                        if lnin[i] == '\x1A':
                            printHeader = True

                        elif lnin[i] != "\n" and lnin[i] != "\r":
                            print(lnin[i], end="")

                else:
                    print("\r\n\r\n", end="")

            elif dest == 0:
                if len(lnin) > 0 and "[" not in lnin and lnin[0] != ":" and lnin[0] != ";":
                    print(lnin, end="")

        except:
            pass

        time.sleep(0.05)

def read_term():
    global dest
    global term
    global printHeader
    lnin = ""
    destStr = ""
    while True:
        try:
            lnin = input()
            if "exit" in lnin:
                break
            elif "open" in lnin:
                dest = int(lnin.split(" ")[1])
                destStr = f'{dest:0>2X}'
            elif len(lnin) > 0:
                printHeader = True

                if dest == 0:
                    lnout = lnin
                    lnout = lnout + "\n"
                    lnout = lnout.encode("ASCII")
                    term.write(lnout)
                else:
                    lnout = "~"
                    lnout = lnout + destStr
                    lnout = lnout + lnin
                    lnout = lnout + "\n"
                    lnout = lnout.encode("ASCII")
                    term.write(lnout)

        except:
            pass

        time.sleep(0.05)

term_thread = threading.Thread(target=read_term)
serial_thread = threading.Thread(target=read_serial)

term_thread.start()
serial_thread.start()
