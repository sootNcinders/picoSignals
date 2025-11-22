import serial
import serial.tools.list_ports
import time
import sys
import binascii

UPDATE_NODES = {1}
UPDATE_FILE = "/Users/tleavitt/pico/projects/picoSignals/Builds/V4R0/picoSignals-V4R0.hex"

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

for node in UPDATE_NODES:
    print(f"Starting update for node {node}")

    #Send Update Command
    lnOut = "~"
    lnOut += f'{node:0>2X}'
    lnOut += "UPDATE\n"
    lnOut = lnOut.encode("ASCII")
    term.write(lnOut)

    #Wait for device to reboot
    time.sleep(5.1)

    nodeReady = True#False
    i = 0
    while not nodeReady and i < 10:
        lnOut = "*E\n"
        lnOut = lnOut.encode("ASCII")
        term.write(lnOut)

        time.sleep(0.05)

        lnin = term.readline()
        
        while len(lnin) > 0 and not nodeReady:
            lnin = lnin.decode("ASCII")
            
            #*A means node is ready
            if lnin[0] == '*':
                if lnin[1] == 'A':
                    nodeReady = True
                    print(f"Node {node} is ready for update")

            lnin = term.readline()
        i += 1

    if not nodeReady:
        print(f"Node {node} did not respond, skipping update")
        continue
    else:
        print(f"Updating node {node} with file {UPDATE_FILE}")

        fileChecksum = 0
        recNum = 1
        numLines = len(lines)
        lastPerc = -1
        t1 = t0 = time.perf_counter()
        for idx, line in enumerate(lines):
            t0 = t1
            t1 = time.perf_counter()
            tDiff = t1 - t0
            # line[0] preserved (record start char like ':') and rest is hex pairs
            if len(line) < 2:
                payload = b''
            else:
                # convert hex payload (from char index 1 to end) to bytes
                try:
                    payload = binascii.unhexlify(line[1:])
                except (binascii.Error, TypeError):
                    payload = b''

            bOut = bytearray()
            bOut.extend(b'*D')
            bOut.append(ord(line[0]) if line else 0)
            bOut.extend(payload)
            bOut.append((recNum >> 8) & 0xFF)
            bOut.append(recNum & 0xFF)
            bOut.append(0x0A)  # newline

            # update checksum (same semantics as original: sum of bytes after "*D")
            fileChecksum += sum(bOut[2:-3]) if len(bOut) > 3 else 0

            term.write(bytes(bOut))

            recNum += 1

            perc = int((recNum / max(1, numLines)) * 100)
            if perc != lastPerc:
                lastPerc = perc
                print(f"{perc}%", end="\r")

            # small pause kept minimal; adjust or remove if device can handle faster
            time.sleep(0.01)

            # read any pending responses (bytes-based) and act on resend requests
            lnin = term.readline()
            while lnin:
                # expect responses like b'*N' + two-byte record number
                if lnin[0] == 0x2A:  # b'*'
                    if len(lnin) >= 4 and lnin[1] == ord('N'):
                        lastRecNum = (lnin[2] << 8) + lnin[3]
                        recNum = lastRecNum
                        # jump to that record in our list
                        print(f"\nResending from record {recNum}\n")
                        # set loop index to recNum-1 for next iteration
                        # (we'll continue the for-loop by converting to while if needed)
                        # Simplest: slice remaining lines and reset iteration
                        lines = lines[recNum-1:]
                        numLines = len(lines)
                        recNum = 1
                        lastPerc = -1
                        break
                lnin = term.readline()

        # send final checksum packet: "~C" + two-byte checksum + "\n"
        chk = fileChecksum & 0xFFFF
        final = bytearray(b'~C')
        final.append((chk >> 8) & 0xFF)
        final.append(chk & 0xFF)
        final.append(0x0A)
        #term.write(bytes(final))
        print(f"\nUpdate for node {node} complete")
