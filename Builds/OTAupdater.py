import serial
import serial.tools.list_ports
import time
import sys
import binascii

VERSION = "V4R0"

#Always include 0 for unprogrammed nodes
UPDATE_NODES = {0, 2, 222}
UPDATE_FILE = f"/Users/tleavitt/pico/projects/picoSignals/Builds/{VERSION}/picoSignals-{VERSION}.hex"
UPDATE_VERSION = VERSION

start = time.perf_counter()

f = open(UPDATE_FILE, "r")
lines = [l.rstrip("\r\n") for l in f.readlines()]
f.close()

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

#Select a port
port_index = int(input("Select a port by index: "))

if port_index < 0 or port_index >= len(serial_ports):
    sys.exit("Invalid port index")

term = serial.Serial(serial_ports[port_index], 115200, timeout=0.01)

print(f"Connected to {serial_ports[port_index]}")

anyNodeReady = False

for node in UPDATE_NODES:
    print(f"Starting update for node {node}")

    nodeReady = False
    if node == 255:
        i = 9
    else:
        i = 0

    while not nodeReady and i < 10:
        #Send Update Command
        lnOut = "~"
        lnOut += f'{node:0>2X}'
        lnOut += "UPDATE\n"
        lnOut = lnOut.encode("ASCII")
        term.write(lnOut)

        #Wait for device to reboot
        time.sleep(2)

        lnOut = "*E"
        lnOut += f'{node:0>2X}'
        lnOut += "\n"
        lnOut = lnOut.encode("ASCII")

        term.write(lnOut)

        t = time.perf_counter()

        lnin = term.readline()
        
        while not nodeReady and time.perf_counter() - t < 10:
            if len(lnin) > 0:
                lnin = lnin.decode("ASCII")

                try:
                    addr = (int(lnin[2], 16) << 4) + int(lnin[3], 16)
                except (ValueError, IndexError):
                    addr = 0
                
                #*A means node is ready
                if lnin[0] == '*' and lnin[1] == 'A' and addr == node:
                    anyNodeReady = True
                    nodeReady = True
                    print(f"Node {node} is ready for update")

            lnin = term.readline()
        i += 1

if not anyNodeReady:
    print(f"Nodes did not respond, skipping update")
else:
    print(f"Updating nodes with file {UPDATE_FILE}")

    fileChecksum = 0
    recNum = 1
    numLines = len(lines)
    lastPerc = -1
    i = 0
    while i < numLines:
        recNum = i + 1

        # line[0] preserved (record start char like ':') and rest is hex pairs
        if len(lines[i]) < 2:
            payload = b''
        else:
            # convert hex payload (from char index 1 to end) to bytes
            try:
                payload = binascii.unhexlify(lines[i][1:])
            except (binascii.Error, TypeError):
                payload = b''

        bOut = bytearray()
        bOut.extend(b'*D')
        bOut.append(ord(lines[i][0]) if lines[i] else 0)
        bOut.extend(payload)
        bOut.append((recNum >> 8) & 0xFF)
        bOut.append(recNum & 0xFF)
        bOut.append(0x0A)  # newline

        # update checksum (same semantics as original: sum of bytes after "*D")
        fileChecksum += sum(bOut[2:-3]) if len(bOut) > 3 else 0

        #term.write(bytes(bOut))

        lnOut = "*D"
        lnOut += lines[i]
        lnOut += f'{((recNum >> 8) & 0xFF):0>2X}'
        lnOut += f'{((recNum) & 0xFF):0>2X}'
        lnOut += "\n"
        lnOut = lnOut.encode("ASCII")

        term.write(lnOut)

        perc = int((recNum / max(1, numLines)) * 100)
        percTenth = int((recNum / max(1, numLines)) * 1000) - (perc * 10)
        if perc != lastPerc:
            lastPerc = perc
            print("\r" + f"{perc}.{percTenth}%", end=" ")

        # small pause kept minimal; adjust or remove if device can handle faster
        t = time.perf_counter()
        delay = 0.15  # seconds
        if lines[i][5] == 'F' and lines[i][6] == '0':  #delay at increments of 0x100 to allow device to write to flash
            delay = 0.25 

        while time.perf_counter() - t < delay:
            gotRewind = False
            # read any pending responses (bytes-based) and act on resend requests
            lnin = term.readline()

            lnin = lnin.decode("ASCII", errors='ignore')
            # expect responses like b'*N' + two-byte record number
            if  len(lnin) > 0 and lnin[0] == '*':  # b'*'
                if len(lnin) >= 4 and lnin[1] == 'N':
                    try:
                        lastRecNum = (int(lnin[4], 16) << 12) + (int(lnin[5], 16) << 8) + (int(lnin[6], 16) << 4) + int(lnin[7], 16)

                    except (ValueError, IndexError):
                        lastRecNum = 0

                    if not gotRewind or lastRecNum < recNum:
                        if lastRecNum != 0:
                            recNum = lastRecNum - 1
                            i = recNum - 1
                        else:
                            recNum = 0 
                            i = 0

                        # jump to that record in our list
                        print(f"\nResending from record {recNum}\n")
    
        i += 1

    # send final checksum packet: "~C" + two-byte checksum + "\n"
    chk = fileChecksum & 0xFFFF

    for i in range(0, 10):
        lnOut = "*C"
        lnOut += f'{((chk >> 8) & 0xFF):0>2X}'
        lnOut += f'{((chk) & 0xFF):0>2X}'
        lnOut += "\n"
        lnOut = lnOut.encode("ASCII")

        term.write(lnOut)
        time.sleep(1)

    versions = []

    for node in UPDATE_NODES:
        verReceived = False
        i = 0
        while not verReceived and i < 10:
            #Send ERR CLR Command
            lnOut = "~"
            lnOut += f'{node:0>2X}'
            lnOut += "ERR CLR\n"
            lnOut = lnOut.encode("ASCII")
            term.write(lnOut)

            #Send VER Command
            lnOut = "~"
            lnOut += f'{node:0>2X}'
            lnOut += "VER\n"
            lnOut = lnOut.encode("ASCII")
            term.write(lnOut)

            time.sleep(5)

            lnin = term.readline()

            while len(lnin) > 0 and not verReceived:
                lnin = lnin.decode("ASCII")

                try:
                    addr = (int(lnin[1], 16) << 4) + int(lnin[2], 16)
                except (ValueError, IndexError):
                    addr = 0
                
                #*A means node is ready
                if lnin[3] == '>' and lnin[4] == ' ' and lnin[5] == 'V' and addr == node:
                    verReceived = True
                    ver = lnin[4:].strip()
                    versions.append(ver)

                lnin = term.readline()
            i += 1
        
        if(not verReceived):
            versions.append("V0R0")

    for node, ver in zip(UPDATE_NODES, versions):
        print(f"Node {node} version: {ver}")


    print(f"\nUpdate complete\n")

    end = time.perf_counter()

    #print elapsed time in hours:minutes:seconds
    elapsed = end - start
    hours = int(elapsed // 3600)
    minutes = int((elapsed % 3600) // 60)
    seconds = int(elapsed % 60)
    print(f"Elapsed time: {hours:02d}:{minutes:02d}:{seconds:02d}")
