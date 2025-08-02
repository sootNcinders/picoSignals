import threading
from tkinter import *
import serial
import serial.tools.list_ports
import datetime
import time
import sys
import platform

GRAY = "#555"

DEVH = 1080
DEVW = 1920

HOUR = 60*60 
SIXMIN = 6*60

LABELSIZE = 26
DIAGSIZE = 20

if "Darwin" in platform.system():
    LABELSIZE = 20
    DIAGSIZE = 14

scale = 1

#guis
gui = Tk()
gui.attributes('-fullscreen', True)
gui.resizable(False, False)

screenW = gui.winfo_screenwidth()
screenH = gui.winfo_screenheight()

c = Canvas(gui, bg="black", height=screenH, width=screenW)
m = Menu(gui, tearoff = 0) 

serialSelect = Toplevel(gui)
serialSelect.attributes('-topmost', True)
serialSelect.title("Select Serial Device")

#signal heads 256 possible nodes each with up to 4 heads
head = [[0 for i in range (4)] for j in range(256)]

#switches 
wyeE = 0
wyeW = 0
uWyeE = 0
uWyeW = 0
serviceSwitch1s = 0
serviceSwitch1m = 0
serviceSwitch2s = 0
serviceSwitch2m = 0
serviceSwitch2s = 0
serviceSwitch2m = 0

#radio object
radio = serial.Serial()
value_inside = StringVar(serialSelect) 

f = open("sig.log","a+")
f2 = open("radioData.log", "a+")

dest = 255
headNo = 0

class node:
    def __init__(self) -> None:
        self.heads = ['O' for i in range(0,4)]
        self.captures = [False for i in range(0,4)]
        self.releases = [False for i in range(0,4)]
        self.turnouts = [False for i in range(0,4)]
        self.rssi = 0
        self.voltage = 0.0
        self.avgVoltage = 0.0
        self.prevVoltages = [0 for i in range(0,10)]
        self.ledError = [False for i in range(0,16)]
        self.version = 0
        self.revision = 0

nodes = [node() for i in range(0,256)]
rssis = [0 for i in range(0,256)]
volts = [0 for i in range(0,256)]
caps  = [[0 for i in range(0,4)] for j in range(0,256)]
rels  = [[0 for i in range(0,4)] for j in range(0,256)]
vers  = [0 for i in range(0,256)]
leds  = [0 for i in range(0,256)]
lastMsg = [0 for i in range(0,256)]

diagVisible = False
advDiagVisible = False

changed = False

trainCount = 0
counter = 0
lastParrish = 'G'

def do_rclick(event):
    global dest
    global headNo
    global scale
    
    x = event.x_root/scale
    y = event.y_root/scale

    if "Darwin" in platform.system():
        y = y - 40 #offset 40 pixels for mac with notch

    print("RCLICK X:" + str(x) + " Y:" + str(y))

    try:
        if x >= 785 and x <= 805 and y >= 670 and y <= 690:
            dest = 1
            headNo = 0
            m.tk_popup(event.x_root, event.y_root)
        elif x >= 220 and x <= 240 and y >= 710 and y <= 730:
            dest = 2
            headNo = 0
            m.tk_popup(event.x_root, event.y_root)
        elif x >= 1075 and x <= 1095 and y >= 715 and y <= 755:
            dest = 17
            if y >= 735:
                headNo = 0
            else:
                headNo = 1
            m.tk_popup(event.x_root, event.y_root)
        elif x >= 1850 and x <= 1870 and y >= 740 and y <= 760:
            dest = 18
            headNo = 0
            m.tk_popup(event.x_root, event.y_root)
        elif x>= 1570 and x <= 1590 and y >= 1000 and y <= 1020:
            dest = 19
            headNo = 0
            m.tk_popup(event.x_root, event.y_root)
        elif x >= 1455 and x <= 1475 and y >= 730 and y <= 750:
            dest = 21
            headNo = 0
            m.tk_popup(event.x_root, event.y_root)
        else:
            dest = 255
            headNo = 0
            m.tk_popup(event.x_root, event.y_root)
            
    finally:
        m.grab_release()
        
c.bind("<Button-3>", do_rclick)
c.bind("<Button-2>", do_rclick)

def sendCmd(addr: int, cmd: int):
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
    """

    out = ":"
    out = out + f'{addr:0>2x}'
    out = out + f'{cmd:0>2x}'

    out = out.encode("ASCII")

    radio.write(out)

def ping():
    global dest

    print("Pinging " + str(dest) + "\n")

    sendCmd(dest, 1)

    dest = 255

def wake():
    global dest
    
    print("Waking " + str(dest) + "\n")

    sendCmd(dest, 2)
    
    dest = 255
    
def capture():
    global dest
    global headNo
    
    if dest < 255:
        print("Capturing " + str(dest) + " Head " + str(headNo) + "\n")
    
        sendCmd(dest, 3 + headNo)
    
    dest = 255

def release():
    global dest
    global headNo

    if dest < 255:
        print("Releasing " + str(dest) + " Head " + str(headNo) + "\n")

        sendCmd(dest, 7 + headNo)    
    
    dest = 255

m.add_command(label = "Ping", command = ping) 
m.add_command(label = "Wake", command = wake)
m.add_separator()
m.add_command(label = "Capture", command = capture)
m.add_command(label = "Release", command = release)

def setSerial(): 
    global radio 
    radio = serial.Serial(value_inside.get(), 115200, timeout=1)
    print("Selected Option: {}".format(value_inside.get())) 
    serialSelect.destroy()
    dataThread.start()
    heartbeatThread.start()
    return None

def loadHeads():
    global head
    #parrish east
    #head[ 1][0] = c.create_oval(1600,  270, 1620,  290, fill="black")
    head[1][0] = c.create_oval(785,  670, 805,  690, fill=GRAY)
    #etowah west
    #head[ 2][0] = c.create_oval( 300,  310,  320,  330, fill="black")
    head[2][0] = c.create_oval(220,  710,  240,  730, fill=GRAY)
    #Etowah East
    head[3][0] = c.create_oval(1670, 590, 1690, 570, fill=GRAY)
    #Alloy
    head[4][0] = c.create_oval(1290, 630, 1310, 650, fill=GRAY)
    #Reid West
    head[5][0] = c.create_oval(1085, 610, 1105, 630, fill=GRAY)
    #Reid East
    head[6][0] = c.create_oval(785, 590, 805, 570, fill=GRAY)
    #Cotton Mill
    head[7][0] = c.create_oval(405, 630, 425, 650, fill=GRAY)
    #Midland West
    head[8][0] = c.create_oval(220, 610, 240, 630, fill=GRAY)
    #Midland East
    head[9][0] = c.create_oval(1670, 490, 1690, 470, fill=GRAY)
    #Sargent West
    head[10][0] = c.create_oval(1085, 510, 1105, 530, fill=GRAY)
    #Sargent East
    head[11][0] = c.create_oval(785, 490, 805, 470, fill=GRAY)
    #St. Paul West
    head[12][0] = c.create_oval(220, 510, 240, 530, fill=GRAY)
    #St. Paul East
    head[13][0] = c.create_oval(1670, 370, 1690, 350, fill=GRAY)
    head[13][1] = c.create_oval(1670, 390, 1690, 370, fill=GRAY)
    #Elizabeth East
    head[14][0] = c.create_oval(840, 60, 860, 80, fill=GRAY)
    #Elizabth West
    head[15][0] = c.create_oval(520, 50, 540, 70, fill=GRAY)
    #Toonigh
    head[16][0] = c.create_oval(690, 390, 710, 370, fill=GRAY)
    #parrish west
    #head[17][0] = c.create_oval( 190,  520,  210,  540, fill="black")
    #head[17][1] = c.create_oval( 190,  540,  210,  560, fill="black")
    head[17][0] = c.create_oval(1075,  735,  1095,  755, fill=GRAY)
    head[17][1] = c.create_oval(1075,  715,  1095,  735, fill=GRAY)
    #greyrock
    #head[18][0] = c.create_oval(1090,  540, 1110,  560, fill="black")
    head[18][0] = c.create_oval(1860,  740, 1880,  760, fill=GRAY)
    #canton
    #head[19][0] = c.create_oval( 810,  900,  830,  920, fill="black")
    head[19][0] = c.create_oval(1570, 1020, 1590, 1000, fill=GRAY)
    #upper serivce leads
    #head[21][0] = c.create_oval( 670,  530,  690,  550, fill="black")
    head[21][0] = c.create_oval(1455,  730, 1475,  750, fill=GRAY)

    c.pack()

def loadDiags():
    global rssis
    global volts
    global caps
    global rels
    global c

    #Parrish West
    rssis[1]   = c.create_text(825, 690, text="-000", fill="", font=('Times', DIAGSIZE))
    vers[1]    = c.create_text(825, 660, text="V0R0", fill="", font=('Times', DIAGSIZE))

    volts[1]   = c.create_text(830, 675, text="00.0V", fill="", font=('Times', DIAGSIZE))
    leds[1]    = c.create_text(845, 690, text="LED Fault", fill="", font=('Times', DIAGSIZE))
    caps[1][0] = c.create_text(820, 660, text="C1", fill="", font=('Times', DIAGSIZE))
    rels[1][0] = c.create_text(840, 660, text="R1", fill="", font=('Times', DIAGSIZE))
    #Etowah East
    rssis[2]   = c.create_text(265, 730, text="-000", fill="", font=('Times', DIAGSIZE))
    vers[2]    = c.create_text(265, 745, text="V0R0", fill="", font=('Times', DIAGSIZE))

    volts[2]   = c.create_text(270, 715, text="00.0V", fill="", font=('Times', DIAGSIZE))
    leds[2]    = c.create_text(285, 730, text="LED Fault", fill="", font=('Times', DIAGSIZE))
    caps[2][0] = c.create_text(260, 745, text="C1", fill="", font=('Times', DIAGSIZE))
    rels[2][0] = c.create_text(280, 745, text="R1", fill="", font=('Times', DIAGSIZE))
    #Etowah West
    rssis[3]   = c.create_text(1715, 590, text="-000", fill="", font=('Times', DIAGSIZE))
    vers[3]    = c.create_text(1715, 560, text="V0R0", fill="", font=('Times', DIAGSIZE))

    volts[3]   = c.create_text(1720, 575, text="00.0V", fill="", font=('Times', DIAGSIZE))
    leds[3]    = c.create_text(1735, 590, text="LED Fault", fill="", font=('Times', DIAGSIZE))
    caps[3][0] = c.create_text(1710, 560, text="C1", fill="", font=('Times', DIAGSIZE))
    rels[3][0] = c.create_text(1730, 560, text="R1", fill="", font=('Times', DIAGSIZE))
    #Alloy
    rssis[4]   = c.create_text(1335, 650, text="-000", fill="", font=('Times', DIAGSIZE))
    vers[4]    = c.create_text(1335, 665, text="V0R0", fill="", font=('Times', DIAGSIZE))

    volts[4]   = c.create_text(1340, 635, text="00.0V", fill="", font=('Times', DIAGSIZE))
    leds[4]    = c.create_text(1355, 650, text="LED Fault", fill="", font=('Times', DIAGSIZE))
    caps[4][0] = c.create_text(1330, 665, text="C1", fill="", font=('Times', DIAGSIZE))
    rels[4][0] = c.create_text(1350, 665, text="R1", fill="", font=('Times', DIAGSIZE))
    #Reid East
    rssis[5]   = c.create_text(1125, 630, text="-000", fill="", font=('Times', DIAGSIZE))
    vers[5]    = c.create_text(1125, 645, text="V0R0", fill="", font=('Times', DIAGSIZE))

    volts[5]   = c.create_text(1130, 615, text="00.0V", fill="", font=('Times', DIAGSIZE))
    leds[5]    = c.create_text(1145, 630, text="LED Fault", fill="", font=('Times', DIAGSIZE))
    caps[5][0] = c.create_text(1120, 645, text="C1", fill="", font=('Times', DIAGSIZE))
    rels[5][0] = c.create_text(1140, 645, text="R1", fill="", font=('Times', DIAGSIZE))
    #Reid West
    rssis[6]   = c.create_text(825, 590, text="-000", fill="", font=('Times', DIAGSIZE))
    vers[6]    = c.create_text(825, 560, text="V0R0", fill="", font=('Times', DIAGSIZE))

    volts[6]   = c.create_text(830, 575, text="00.0V", fill="", font=('Times', DIAGSIZE))
    leds[6]    = c.create_text(845, 590, text="LED Fault", fill="", font=('Times', DIAGSIZE))
    caps[6][0] = c.create_text(820, 560, text="C1", fill="", font=('Times', DIAGSIZE))
    rels[6][0] = c.create_text(840, 560, text="R1", fill="", font=('Times', DIAGSIZE))
    #Cotton Mill
    rssis[7]   = c.create_text(445, 650, text="-000", fill="", font=('Times', DIAGSIZE))
    vers[7]    = c.create_text(445, 665, text="V0R0", fill="", font=('Times', DIAGSIZE))

    volts[7]   = c.create_text(450, 635, text="00.0V", fill="", font=('Times', DIAGSIZE))
    leds[7]    = c.create_text(465, 650, text="LED Fault", fill="", font=('Times', DIAGSIZE))
    caps[7][0] = c.create_text(440, 665, text="C1", fill="", font=('Times', DIAGSIZE))
    rels[7][0] = c.create_text(460, 665, text="R1", fill="", font=('Times', DIAGSIZE))
    #Midland East
    rssis[8]   = c.create_text(265, 630, text="-000", fill="", font=('Times', DIAGSIZE))
    vers[8]    = c.create_text(265, 645, text="V0R0", fill="", font=('Times', DIAGSIZE))

    volts[8]   = c.create_text(270, 615, text="00.0V", fill="", font=('Times', DIAGSIZE))
    leds[8]    = c.create_text(285, 630, text="LED Fault", fill="", font=('Times', DIAGSIZE))
    caps[8][0] = c.create_text(260, 645, text="C1", fill="", font=('Times', DIAGSIZE))
    rels[8][0] = c.create_text(280, 645, text="R1", fill="", font=('Times', DIAGSIZE))
    #Midland West
    rssis[9]   = c.create_text(1715, 490, text="-000", fill="", font=('Times', DIAGSIZE))
    vers[9]    = c.create_text(1715, 460, text="V0R0", fill="", font=('Times', DIAGSIZE))

    volts[9]   = c.create_text(1720, 475, text="00.0V", fill="", font=('Times', DIAGSIZE))
    leds[9]    = c.create_text(1735, 490, text="LED Fault", fill="", font=('Times', DIAGSIZE))
    caps[9][0] = c.create_text(1710, 460, text="C1", fill="", font=('Times', DIAGSIZE))
    rels[9][0] = c.create_text(1730, 460, text="R1", fill="", font=('Times', DIAGSIZE))
    #Sargent East
    rssis[10]   = c.create_text(1125, 530, text="-000", fill="", font=('Times', DIAGSIZE))
    vers[10]    = c.create_text(1125, 545, text="V0R0", fill="", font=('Times', DIAGSIZE))

    volts[10]   = c.create_text(1130, 515, text="00.0V", fill="", font=('Times', DIAGSIZE))
    leds[10]    = c.create_text(1145, 530, text="LED Fault", fill="", font=('Times', DIAGSIZE))
    caps[10][0] = c.create_text(1120, 545, text="C1", fill="", font=('Times', DIAGSIZE))
    rels[10][0] = c.create_text(1140, 545, text="R1", fill="", font=('Times', DIAGSIZE))
    #Sargent West
    rssis[11]   = c.create_text(825, 490, text="-000", fill="", font=('Times', DIAGSIZE))
    vers[11]    = c.create_text(825, 460, text="V0R0", fill="", font=('Times', DIAGSIZE))

    volts[11]   = c.create_text(830, 475, text="00.0V", fill="", font=('Times', DIAGSIZE))
    leds[11]    = c.create_text(845, 490, text="LED Fault", fill="", font=('Times', DIAGSIZE))
    caps[11][0] = c.create_text(820, 460, text="C1", fill="", font=('Times', DIAGSIZE))
    rels[11][0] = c.create_text(840, 460, text="R1", fill="", font=('Times', DIAGSIZE))
    #St. Paul East
    rssis[12]   = c.create_text(265, 530, text="-000", fill="", font=('Times', DIAGSIZE))
    vers[12]    = c.create_text(265, 545, text="V0R0", fill="", font=('Times', DIAGSIZE))

    volts[12]   = c.create_text(270, 515, text="00.0V", fill="", font=('Times', DIAGSIZE))
    leds[12]    = c.create_text(285, 530, text="LED Fault", fill="", font=('Times', DIAGSIZE))
    caps[12][0] = c.create_text(260, 545, text="C1", fill="", font=('Times', DIAGSIZE))
    rels[12][0] = c.create_text(280, 545, text="R1", fill="", font=('Times', DIAGSIZE))
    #St. Paul West
    rssis[13]   = c.create_text(1715, 390, text="-000", fill="", font=('Times', DIAGSIZE))
    vers[13]    = c.create_text(1715, 360, text="V0R0", fill="", font=('Times', DIAGSIZE))

    volts[13]   = c.create_text(1720, 375, text="00.0V", fill="", font=('Times', DIAGSIZE))
    leds[13]    = c.create_text(1735, 390, text="LED Fault", fill="", font=('Times', DIAGSIZE))
    caps[13][0] = c.create_text(1710, 360, text="C1", fill="", font=('Times', DIAGSIZE))
    rels[13][0] = c.create_text(1730, 360, text="R1", fill="", font=('Times', DIAGSIZE))
    rels[13][1] = c.create_text(1750, 360, text="R2", fill="", font=('Times', DIAGSIZE))
    #Elizabeth West
    rssis[14]   = c.create_text(885, 85, text="-000", fill="", font=('Times', DIAGSIZE))
    vers[14]    = c.create_text(885, 100, text="V0R0", fill="", font=('Times', DIAGSIZE))

    volts[14]   = c.create_text(890, 70, text="00.0V", fill="", font=('Times', DIAGSIZE))
    leds[14]    = c.create_text(905, 85, text="LED Fault", fill="", font=('Times', DIAGSIZE))
    caps[14][0] = c.create_text(880, 100, text="C1", fill="", font=('Times', DIAGSIZE))
    rels[14][0] = c.create_text(900, 100, text="R1", fill="", font=('Times', DIAGSIZE))
    #Elizabeth East
    rssis[15]   = c.create_text(485, 65, text="-000", fill="", font=('Times', DIAGSIZE))
    vers[15]    = c.create_text(485, 80, text="V0R0", fill="", font=('Times', DIAGSIZE))

    volts[15]   = c.create_text(490, 50, text="00.0V", fill="", font=('Times', DIAGSIZE))
    leds[15]    = c.create_text(475, 65, text="LED Fault", fill="", font=('Times', DIAGSIZE))
    caps[15][0] = c.create_text(480, 80, text="C1", fill="", font=('Times', DIAGSIZE))
    rels[15][0] = c.create_text(500, 80, text="R1", fill="", font=('Times', DIAGSIZE))
    #Toonigh
    rssis[16]   = c.create_text(735, 375, text="-000", fill="", font=('Times', DIAGSIZE))
    vers[16]    = c.create_text(735, 390, text="V0R0", fill="", font=('Times', DIAGSIZE))

    volts[16]   = c.create_text(740, 360, text="00.0V", fill="", font=('Times', DIAGSIZE))
    leds[16]    = c.create_text(755, 375, text="LED Fault", fill="", font=('Times', DIAGSIZE))
    caps[16][0] = c.create_text(730, 390, text="C1", fill="", font=('Times', DIAGSIZE))
    rels[16][0] = c.create_text(750, 390, text="R1", fill="", font=('Times', DIAGSIZE))
    #Parrish West
    rssis[17]   = c.create_text(1055, 750, text="-000", fill="", font=('Times', DIAGSIZE))
    vers[17]    = c.create_text(1055, 765, text="V0R0", fill="", font=('Times', DIAGSIZE))

    volts[17]   = c.create_text(1050, 735, text="00.0V", fill="", font=('Times', DIAGSIZE))
    leds[17]    = c.create_text(1035, 750, text="LED Fault", fill="", font=('Times', DIAGSIZE))
    caps[17][0] = c.create_text(1040, 765, text="C1", fill="", font=('Times', DIAGSIZE))
    rels[17][0] = c.create_text(1060, 765, text="R1", fill="", font=('Times', DIAGSIZE))
    rels[17][1] = c.create_text(1080, 765, text="R2", fill="", font=('Times', DIAGSIZE))
    #Greyrock
    rssis[18]   = c.create_text(1860, 715, text="-000", fill="", font=('Times', DIAGSIZE))
    vers[18]    = c.create_text(1860, 730, text="V0R0", fill="", font=('Times', DIAGSIZE))

    volts[18]   = c.create_text(1855, 700, text="00.0V", fill="", font=('Times', DIAGSIZE))
    leds[18]    = c.create_text(1875, 715, text="LED Fault", fill="", font=('Times', DIAGSIZE))
    caps[18][0] = c.create_text(1845, 730, text="C1", fill="", font=('Times', DIAGSIZE))
    rels[18][0] = c.create_text(1865, 730, text="R1", fill="", font=('Times', DIAGSIZE))
    #Canton
    rssis[19]   = c.create_text(1610, 1020, text="-000", fill="", font=('Times', DIAGSIZE))
    vers[19]    = c.create_text(1610, 990, text="V0R0", fill="", font=('Times', DIAGSIZE))

    volts[19]   = c.create_text(1615, 1005, text="00.0V", fill="", font=('Times', DIAGSIZE))
    leds[19]    = c.create_text(1635, 1020, text="LED Fault", fill="", font=('Times', DIAGSIZE))
    caps[19][0] = c.create_text(1605, 990, text="C1", fill="", font=('Times', DIAGSIZE))
    rels[19][0] = c.create_text(1625, 990, text="R1", fill="", font=('Times', DIAGSIZE))
    #Upper Service Leads
    rssis[21]   = c.create_text(1495, 750, text="-000", fill="", font=('Times', DIAGSIZE))
    vers[21]    = c.create_text(1495, 765, text="V0R0", fill="", font=('Times', DIAGSIZE))

    volts[21]   = c.create_text(1500, 735, text="00.0V", fill="", font=('Times', DIAGSIZE))
    leds[21]    = c.create_text(1520, 750, text="LED Fault", fill="", font=('Times', DIAGSIZE))
    caps[21][0] = c.create_text(1490, 765, text="C1", fill="", font=('Times', DIAGSIZE))
    rels[21][0] = c.create_text(1510, 765, text="R1", fill="", font=('Times', DIAGSIZE))

def drawMap():
    global counter
    global trainCount

    lineWidth = 7
    #parrish-etowah main
    #c.create_line(0, 300, 1920, 300, width=lineWidth, fill="white")
    #etowah siding
    #c.create_line(0, 280, 270, 280, 290, 300, width=lineWidth, fill="white")
    #parrish siding upper
    #c.create_line(1630, 300, 1650, 320, 1920, 320, width=lineWidth, fill="white")
    #parrish siding lower
    #c.create_line(0, 500, 200, 500, 180, 520, 0, 520, width=lineWidth, fill="white")

    #St. Paul Siding Upper
    c.create_line(1920, 400, 1670, 400, 1690, 420, 1920, 420, width=lineWidth, fill="white")


    #Bryan Wye
    c.create_line(1590, 320, 1530, 400, width=lineWidth, fill=GRAY)
    #Eagle Ridge Spur
    c.create_line(820, 70, 860, 110, 1370, 110, width=lineWidth, fill=GRAY)
    #D&MT Jct
    c.create_line(820, 70, 780, 110, 0, 110, width=lineWidth, fill=GRAY)
    c.create_line(770, 90, 800, 90, width=lineWidth, fill=GRAY)
    c.create_line(0, 310, 200, 110, width=lineWidth, fill=GRAY)
    #Toonigh Yard
    c.create_line(530, 280, 630, 380, 670, 380, width=lineWidth, fill=GRAY)
    c.create_line(550, 280, 650, 380, width=lineWidth, fill=GRAY)
    c.create_line(570, 280, 690, 400, width=lineWidth, fill=GRAY)
    #Harpur Spur
    c.create_line(1570, 400, 1470, 300, 920, 300, width=lineWidth, fill=GRAY)
    c.create_line(920, 300, 670, 300, width=lineWidth, fill=GRAY)
    #Elizabeth Loop Main
    c.create_line(1630, 360, 1590, 320, 1590, 150, 1490, 50, 570, 50, 470, 150, 470, 300, 570, 400, 1613, 400, width=lineWidth, fill="white")
    #Elizabeth Siding
    c.create_line(550, 70, 820, 70, 840, 50, width=lineWidth, fill="white")


    global uWyeW
    uWyeW = c.create_line(1670, 400, 1613, 400, width=lineWidth, fill=GRAY)
    global uWyeE
    uWyeE = c.create_line(1670, 400, 1630, 360, width=lineWidth, fill="white")

    #Midland - St. Paul Main
    c.create_line(0, 500, 1920, 500, width=lineWidth, fill="white")
    #St. Paul Siding Lower
    c.create_line(0, 520, 200, 520, 220, 500, width=lineWidth, fill="white")
    #Sargent Siding
    c.create_line(785, 500, 805, 520, 1065, 520, 1085, 500, width=lineWidth, fill="white")
    #Midland Siding Upper
    c.create_line(1670, 500, 1690, 520, 1920, 520, width=lineWidth, fill="white")

    #Midland Siding Lower
    c.create_line(0, 620, 200, 620, 220, 600, width=lineWidth, fill="white")
    #Cotton Mill Spur
    c.create_line(385, 600, 405, 620, 605, 620, width=lineWidth, fill=GRAY)
    #Reid Siding
    c.create_line(785, 600, 805, 620, 1065, 620, 1085, 600, width=lineWidth, fill="white")
    #Alloy Spur
    c.create_line(1270, 600, 1290, 620, 1490, 620, width=lineWidth, fill=GRAY)
    #Etowah Siding Upper
    c.create_line(1670, 600, 1690, 620, 1920, 620, width=lineWidth, fill="white")
    #Etowah - Midland Main
    c.create_line(0, 600, 1920, 600, width=lineWidth, fill="white")

    #Parrish - Etowah Main
    c.create_line(0, 700, 1085, 700, width=lineWidth, fill="white")
    #Parrish Siding
    c.create_line(785, 700, 805, 720, 1065, 720, 1085, 700, width=lineWidth, fill="white")
    #Etowah Siding Lower
    c.create_line(0, 720, 200, 720, 220, 700, width=lineWidth, fill="white")
    #wye swtich
    global wyeE
    #wyeE = c.create_line(200, 500, 257, 500, width=lineWidth, fill="#333")
    wyeE = c.create_line(1085, 700, 1142, 700, width=lineWidth, fill=GRAY)
    global wyeW
    #wyeW = c.create_line(200, 500, 240, 540, width=lineWidth, fill="white")
    wyeW = c.create_line(1085, 700, 1125, 740, width=lineWidth, fill="white")
    #wye 3rd leg
    #c.create_line(280, 580, 360, 500, width=lineWidth, fill="#333")
    c.create_line(1165, 780, 1245, 700, width=lineWidth, fill=GRAY)
    #Canton Yard
    c.create_line(1265, 1050, 935, 1050, width=lineWidth, fill=GRAY)
    c.create_line(1085, 1050, 985, 950, width=lineWidth, fill=GRAY)
    c.create_line(1245, 1030, 1085, 1030, width=lineWidth, fill=GRAY)
    c.create_line(1225, 1030, 1145, 950, 1145, 800, width=lineWidth, fill=GRAY)
    c.create_line(1205, 1030, 1125, 950, 1125, 800, width=lineWidth, fill=GRAY)
    c.create_line(1185, 1030, 1105, 950, 1105, 800, width=lineWidth, fill=GRAY)
    c.create_line(1165, 1030, 1085, 950, 1085, 800, width=lineWidth, fill=GRAY)
    c.create_line(1145, 1030, 1065, 950, 1065, 800, width=lineWidth, fill=GRAY)
    c.create_line(1125, 1030, 1045, 950, 1045, 800, width=lineWidth, fill=GRAY)
    c.create_line(1105, 1030, 1025, 950, 1025, 800, width=lineWidth, fill=GRAY)
    c.create_line(1085, 1030, 1005, 950, 1005, 800, width=lineWidth, fill=GRAY)
    #canton loop
    #c.create_line(240, 540, 280, 580, 280, 850, 380, 950, 1000, 950, 1100, 850, 1100, 580, 1080, 560, width=lineWidth, fill="white")
    c.create_line(1125, 740, 1165, 780, 1165, 950, 1265, 1050, 1770, 1050, 1870, 950, 1870, 780, 1850, 760, width=lineWidth, fill="white")
    #canton loop tunnel to service track switch
    #c.create_line(1020, 500, 850, 500, width=lineWidth, fill="white")
    c.create_line(1790, 700, 1535, 700, width=lineWidth, fill="white")
    #canton loop wye to service track swtich
    #c.create_line(822, 500, 257, 500, width=lineWidth, fill="white")
    c.create_line(1507, 700, 1142, 700, width=lineWidth, fill="white")
    #tunnel
    #c.create_line(1080, 560, 1020, 500, width=lineWidth, fill="white", dash=(7, 7))
    c.create_line(1850, 760, 1790, 700, width=lineWidth, fill="white", dash=(7, 7))
    #canton siding
    #c.create_line(400, 930, 800, 930, 820, 950, width=lineWidth, fill="white")
    c.create_line(1285, 1030, 1485, 1030, 1505, 1050, width=lineWidth, fill="white")
    #greyrock siding 
    #c.create_line(800, 930, 990, 930, 1080, 840, 1080, 560, width=lineWidth, fill="white")
    c.create_line(1485, 1030, 1760, 1030, 1850, 940, 1850, 760, width=lineWidth, fill="white")
    #upper service lead switch
    global serviceSwitch1s
    #serviceSwitch1s = c.create_line(850, 500, 830, 520, width=lineWidth, fill="#333")
    serviceSwitch1s = c.create_line(1535, 700, 1515, 720, width=lineWidth, fill=GRAY)
    global serviceSwitch1m
    #serviceSwitch1m = c.create_line(850, 500, 822, 500, width=lineWidth, fill="white")
    serviceSwitch1m = c.create_line(1535, 700, 1507, 700, width=lineWidth, fill="white")
    #lower service lead switch 
    global serviceSwitch2s
    #serviceSwitch2s = c.create_line(400, 930, 380, 910, width=lineWidth, fill="#333")
    serviceSwitch2s = c.create_line(1285, 1030, 1265, 1010, width=lineWidth, fill=GRAY)
    global serviceSwitch2m
    #serviceSwitch2m = c.create_line(400, 930, 380, 950, width=lineWidth, fill="white")
    serviceSwitch2m = c.create_line(1285, 1030, 1265, 1050, width=lineWidth, fill="white")
    #service leads
    #c.create_line(380, 910, 300, 830, 300, 590, 370, 520, 830, 520, width=lineWidth, fill="white")
    c.create_line(1265, 1010, 1185, 930, 1185, 790, 1255, 720, 1515, 720, width=lineWidth, fill="white")
    #c.create_line(360, 890, 360, 860, 320, 820, 320, 600, 380, 540, 650, 540, 670, 520, width=lineWidth, fill="white")
    c.create_line(1245, 990, 1245, 960, 1205, 920, 1205, 800, 1265, 740, 1435, 740, 1455, 720, width=lineWidth, fill="white")

    #siding labels
    c.create_text(670, 90, text="Elizabeth", fill="white", font=('Times', LABELSIZE))
    c.create_text(970, 130, text="Eagle Ridge Branch", fill="white", font=('Times', LABELSIZE))
    c.create_text(100, 90, text="Duluth & Mt. Tabor RR", fill="white", font=('Times', LABELSIZE))
    c.create_text(570, 260, text="Toonigh", fill="white", font=('Times', LABELSIZE))
    c.create_text(820, 280, text="Harpur", fill="white", font=('Times', LABELSIZE))
    c.create_text(1820, 440, text="St. Paul", fill="white", font=('Times', LABELSIZE))
    c.create_text(75, 540, text="St. Paul", fill="white", font=('Times', LABELSIZE))
    c.create_text(935, 540, text="Sargent", fill="white", font=('Times', LABELSIZE))
    c.create_text(1820, 540, text="Midland", fill="white", font=('Times', LABELSIZE))
    c.create_text(75, 640, text="Midland", fill="white", font=('Times', LABELSIZE))
    c.create_text(535, 640, text="Cotton Mill", fill="white", font=('Times', LABELSIZE))
    c.create_text(935, 640, text="Reid", fill="white", font=('Times', LABELSIZE))
    c.create_text(1420, 640, text="Alloy", fill="white", font=('Times', LABELSIZE))
    #c.create_text(1800, 280, text="Parrish", fill="white", font=('Times', LABELSIZE))
    #c.create_text(100, 480, text="Parrish", fill="white", font=('Times', LABELSIZE))
    c.create_text(935, 740, text="Parrish", fill="white", font=('Times', LABELSIZE))
    #c.create_text(150, 260, text="Etowah", fill="white", font=('Times', LABELSIZE))
    c.create_text(75, 740, text="Etowah", fill="white", font=('Times', LABELSIZE))
    c.create_text(1820, 640, text="Etowah", fill="white", font=('Times', LABELSIZE))
    #c.create_text(600, 910, text="Canton", fill="white", font=('Times', LABELSIZE))
    c.create_text(1385, 1010, text="Canton", fill="white", font=('Times', LABELSIZE))
    #c.create_text(1150, 700, text="Greyrock", fill="white", font=('Times', LABELSIZE))
    c.create_text(11795, 800, text="Greyrock", fill="white", font=('Times', LABELSIZE))

    RRSIZE = 70
    EWSIZE = 80
    COUNTERSIZE = 50

    if "Darwin" in platform.system():
        RRSIZE = 50
        EWSIZE = 60
        COUNTERSIZE = 30

    c.create_text(535, 850, text="Canton, St. Paul, & Pacfic Rwy.", fill="white", font=('Times', RRSIZE))
    c.create_text(535, 1000, text="E <-> W", fill="white", font=('Times', EWSIZE))

    counter = c.create_text(150, 50, text="Train Movements: 0", fill="white", font=('Times', COUNTERSIZE))

    c.pack()

def quit_gui():
    global radio
    global f
    global f2
    try:
        radio.close()
        f.close()
        f2.close()
    except AttributeError:
        print("closed without radio")

    gui.destroy()
    sys.exit()

def drawDiag():
    global c
    global diagVisible
    global advDiagVisible
    global rssis
    global volts
    global caps
    global rels
    global leds
    global vers

    for i in range(0, len(rssis)):
        if rssis[i] != 0:
            c.itemconfigure(rssis[i], fill="")
        if vers[i] != 0:
            c.itemconfigure(vers[i], fill="")
        
        if volts[i] != 0:
            if diagVisible:
                c.itemconfigure(volts[i], fill="")
            else:
                c.itemconfigure(volts[i], fill="white")

        if leds[i] != 0:
            if diagVisible:
                c.itemconfigure(leds[i], fill="")
            else:
                c.itemconfigure(leds[i], fill=GRAY)
        for x in range(0,4):
            if caps[i][x] != 0:
                if diagVisible:
                    c.itemconfigure(caps[i][x], fill="")
                else:
                    c.itemconfigure(caps[i][x], fill=GRAY)
            if rels[i][x] != 0:
                if diagVisible:
                    c.itemconfigure(rels[i][x], fill="")
                else:
                    c.itemconfigure(rels[i][x], fill=GRAY)
    diagVisible = not diagVisible
    advDiagVisible = False

def drawAdvDiag():
    global advDiagVisible
    global diagVisible
    global c
    global rssis
    global volts
    global caps
    global rels
    global leds
    global vers

    for i in range(0, len(rssis)):
        if leds[i] != 0:
            c.itemconfigure(leds[i], fill="")
        for x in range(0,4):
            if caps[i][x] != 0:
                c.itemconfigure(caps[i][x], fill="")
            if rels[i][x] != 0:
                c.itemconfigure(rels[i][x], fill="")

        if volts[i] != 0:
            if advDiagVisible:
                c.itemconfigure(volts[i], fill="")
            else:
                c.itemconfigure(volts[i], fill="white")
        if rssis[i] != 0:
            if advDiagVisible:
                c.itemconfigure(rssis[i], fill="")
            else:
                c.itemconfigure(rssis[i], fill="white")
        if vers[i] != 0:
            if advDiagVisible:
                c.itemconfigure(vers[i], fill="")
            else:
                c.itemconfigure(vers[i], fill="white")
    advDiagVisible = not advDiagVisible
    diagVisible = False

def clearCount():
    global trainCount
    trainCount = 0
    c.itemconfigure(counter, text=("Train Movements: " + str(trainCount)))

def drawButtons():
    Button(gui, text="QUIT", command=quit_gui).place(x=0, y=0)
    Button(gui, text="TOGGLE DIAG", command=drawDiag).place(x=65, y=0)
    Button(gui, text="TOGGLE ADV DIAG", command=drawAdvDiag).place(x=185, y=0)
    Button(gui, text="CLEAR COUNT", command=clearCount).place(x=335,y=0)

def get_data():
    global radio
    global nodes
    global changed
    global f
    global f2
    global lastParrish
    global trainCount

    lnIn = ""
    while True:
        try:
            lnIn = radio.readline()
            lnIn = lnIn.decode("ASCII")

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

            if lnIn[0] == ':':
                node = int(lnIn[1] + lnIn[2], 16)
                
                for i in range(0, 4):
                    nodes[node].heads[i] = lnIn[i+3]

                for i in range(0,4):
                    nodes[node].captures[i] = (((int(lnIn[7]) >> i) & 0x01) == 1)  

                for i in range(0,4):
                    nodes[node].releases[i] = (((int(lnIn[8]) >> i) & 0x01) == 1)  

                for i in range(0,4):
                    nodes[node].turnouts[i] = (((int(lnIn[9]) >> i) & 0x01) == 1)

                nodes[node].rssi = -1 * int(lnIn[10] + lnIn[11], 16)
                nodes[node].voltage = (int(lnIn[12] + lnIn[13], 16)/10.0)    

                if 0 in nodes[node].prevVoltages:
                    for i in range(0, len(nodes[node].prevVoltages)):
                        nodes[node].prevVoltages[i] = nodes[node].voltage
                        nodes[node].avgVoltage = nodes[node].voltage
                else:
                    nodes[node].prevVoltages.pop()
                    nodes[node].prevVoltages.append(nodes[node].voltage)

                    nodes[node].avgVoltage = 0
                    for i in range(0, len(nodes[node].prevVoltages)):
                        nodes[node].avgVoltage += nodes[node].prevVoltages[i]
                    nodes[node].avgVoltage /= len(nodes[node].prevVoltages)

                leds = int(lnIn[14] + lnIn[15] + lnIn[16] + lnIn[17], 16)
                for i in range(0, 16):
                    nodes[node].ledError[i] = (((leds >> i) & 0x01) == 1)

                nodes[node].version = int(lnIn[18] + lnIn[19], 16)
                nodes[node].revision = int(lnIn[20] + lnIn[21], 16)

                if node == 1 and (nodes[1].heads[0] == 'A' or nodes[1].heads[0] == 'a') and (lastParrish != 'A' and lastParrish != 'a'):
                    trainCount = trainCount + 1
                    
                lastParrish = nodes[1].heads[0]

                changed = True    

                #ACK
                sendCmd(node, 0)

                print(str(datetime.datetime.now()))        
                print("Sender: " + str(node))
                for i in range(0,4):
                    print("Head " + str(i) + ": " + nodes[node].heads[i])
                print("Captures: " + lnIn[7])
                print("Releases: " + lnIn[8])
                print("Turnouts: " + lnIn[9])
                print("Voltage: " + str(nodes[node].voltage))
                print("RSSI: " + str(nodes[node].rssi) + "\n")
                print("LED Errors: " + str(leds))
                print("Version: " + str(nodes[node].version))
                print("Revision: " + str(nodes[node].revision))
                
                f.write(str(datetime.datetime.now()) + "\n") 
                f.write("Sender: " + str(node) + "\n")
                for i in range(0,4):
                    f.write("Head " + str(i) + ": " + nodes[node].heads[i] + "\n")
                f.write("Captures: " + lnIn[7] + "\n")
                f.write("Releases: " + lnIn[8] + "\n")
                f.write("Turnouts: " + lnIn[9] + "\n")
                f.write("Voltage: " + str(nodes[node].voltage) + "\n")
                f.write("RSSI: " + str(nodes[node].rssi) + "\n")
                f.write("LED Errors: " + str(leds) + "\n")
                f.write("Version: " + str(nodes[node].version) + "\n")
                f.write("Revision: " + str(nodes[node].revision) + "\n\n")

                lastMsg[node] = time.time()

                #handle overlay nodes
                if node == 5: #Reid, Etowah-Reid
                    nodes[4].heads[0] = nodes[5].heads[0]
                    lastMsg[4] = time.time()
                    lastMsg[3] = time.time()

                    if nodes[5].heads[0] == 'A':
                        nodes[3].heads[0] = 'R'
                    elif nodes[5].heads[0] == 'R':
                        nodes[3].heads[0] = 'A'
                    else:
                        nodes[3].heads[0] = nodes[5].heads[0]
                elif node == 6: #Reid, Reid-Midland
                    nodes[7].heads[0] = nodes[6].heads[0]
                    lastMsg[7] = time.time()
                    lastMsg[8] = time.time()

                    if nodes[6].heads[0] == 'A':
                        nodes[8].heads[0] = 'R'
                    elif nodes[6].heads[0] == 'R':
                        nodes[8].heads[0] = 'A'
                    else:
                        nodes[8].heads[0] = nodes[6].heads[0]
                elif node == 9: #Midland, Midland-Sargent
                    lastMsg[10] = time.time()

                    if nodes[9].heads[0] == 'A':
                        nodes[10].heads[0] = 'R'
                    elif nodes[9].heads[0] == 'R':
                        nodes[10].heads[0] = 'A'
                    else:
                        nodes[10].heads[0] = nodes[9].heads[0]
                elif node == 12: #St Paul, Sargent-St Paul
                    lastMsg[11] = time.time()

                    if nodes[12].heads[0] == 'A':
                        nodes[11].heads[0] = 'R'
                    elif nodes[12].heads[0] == 'R':
                        nodes[11].heads[0] = 'A'
                    else:
                        nodes[11].heads[0] = nodes[12].heads[0]
                elif node == 13: #St Paul, St Paul-Elizabeth
                    lastMsg[14] = time.time()
                    lastMsg[15] = time.time()
                    lastMsg[16] = time.time()
                    
                    if nodes[13].heads[0] == 'A':
                        nodes[14].heads[0] = 'R'
                    elif nodes[13].heads[0] == 'R':
                        nodes[14].heads[0] = 'A'
                    else:
                        nodes[14].heads[0] = nodes[13].heads[0]

                    if nodes[13].heads[1] == 'A':
                        nodes[15].heads[0] = 'R'
                        nodes[16].heads[0] = 'R'
                    elif nodes[13].heads[1] == 'R':
                        nodes[15].heads[0] = 'A'
                        nodes[16].heads[0] = 'A'
                    else:
                        nodes[15].heads[0] = nodes[13].heads[1]
                        nodes[16].heads[0] = nodes[13].heads[1]

            elif lnIn[0] == ';':
                #;  - start code
                #00 - sender
                #00 - destination
                #O  - aspect
                #00 - RSSI

                f2.write(str(datetime.datetime.now()) + "\n")
                f2.write("Sender: " + str(int(lnIn[1] + lnIn[2], 16)) + "\n")
                f2.write("Destination: " + str(int(lnIn[3] + lnIn[4], 16)) + "\n")
                f2.write("Aspect: " + lnIn[5] + "\n")
                f2.write("RSSI: " + str(-1 * int(lnIn[6] + lnIn[7], 16)) + "\n\n")

        except:
            pass

        time.sleep(0.05)

def update_gui():
    global head
    global nodes
    global changed
    global wyeE
    global wyeW
    global uWyeE
    global uWyeW
    global serviceSwitch1m
    global serviceSwitch1s
    global serviceSwitch2m
    global serviceSwitch2s
    global diagVisible
    global caps
    global rels
    global lastMsg
    global counter
    global trainCount

    while True:
        if changed:
            changed = False

            c.itemconfigure(counter, text=("Train Movements: " + str(trainCount)))

            for i in range(0, len(head)):
                if head[i][0] != 0:
                    for x in range(0, 4):
                        if head[i][x] != 0:
                            if nodes[i].heads[x] == 'G' or nodes[i].heads[x] == 'g':
                                c.itemconfigure(head[i][x], fill="green")
                            elif nodes[i].heads[x] == 'A' or nodes[i].heads[x] == 'a':
                                c.itemconfigure(head[i][x], fill="yellow")
                            elif nodes[i].heads[x] == 'R' or nodes[i].heads[x] == 'r':
                                c.itemconfigure(head[i][x], fill="red")
                            elif nodes[i].heads[x] == 'L' or nodes[i].heads[x] == 'l':
                                c.itemconfigure(head[i][x], fill="#88F")
                            elif lastMsg[i] != 0:
                                c.itemconfigure(head[i][x], fill=GRAY)
                            else:
                                c.itemconfigure(head[i][x], fill="#222")

                            if diagVisible:
                                if True in nodes[i].ledError:
                                    c.itemconfigure(leds[i], fill="white")
                                else:
                                    c.itemconfigure(leds[i], fill=GRAY)

                            c.itemconfigure(vers[i], text=("V"+format(nodes[i].version, '2d')+"R"+format(nodes[i].revision, '2d')))

                            if volts[i] != 0:
                                c.itemconfigure(volts[i], text=(format(nodes[i].voltage, '02.1f')+"V"))
                                if i < 17:
                                    if nodes[i].voltage < 11.5 and nodes[i].voltage > 0:
                                        c.itemconfigure(volts[i], fill="yellow")
                                    elif diagVisible:
                                        #if nodes[i].avgVoltage + 0.1 < nodes[i].voltage:
                                            #c.itemconfigure(volts[i], fill="green")
                                        #elif nodes[i].avgVoltage - 0.1 > nodes[i].voltage:
                                            #c.itemconfigure(volts[i], fill="red")
                                        #else:
                                        c.itemconfigure(volts[i], fill="white")
                            if rssis[i] != 0:
                                c.itemconfigure(rssis[i], text=(format(nodes[i].rssi, '03d')))

                            if diagVisible:
                                for y in range(0,4):
                                    if caps[i][y] != 0:
                                        if nodes[i].captures[y]:
                                            c.itemconfigure(caps[i][y], fill="white")
                                        else:
                                            c.itemconfigure(caps[i][y], fill=GRAY)
                                    if rels[i][y] != 0:
                                        if nodes[i].releases[y]:
                                            c.itemconfigure(rels[i][y], fill="white")
                                        else:
                                            c.itemconfigure(rels[i][y], fill=GRAY)

                            if i == 17:
                                if nodes[i].turnouts[0]:
                                    c.itemconfigure(wyeW, fill=GRAY)
                                    c.itemconfigure(wyeE, fill="white")
                                else:
                                    c.itemconfigure(wyeE, fill=GRAY)
                                    c.itemconfigure(wyeW, fill="white")
                            elif i == 18:
                                if nodes[i].turnouts[0]:
                                    c.itemconfigure(serviceSwitch1m, fill=GRAY)
                                    c.itemconfigure(serviceSwitch1s, fill="white")
                                else:
                                    c.itemconfigure(serviceSwitch1s, fill=GRAY)
                                    c.itemconfigure(serviceSwitch1m, fill="white")
                            elif i == 19:
                                if nodes[i].turnouts[1]:
                                    c.itemconfigure(serviceSwitch2m, fill=GRAY)
                                    c.itemconfigure(serviceSwitch2s, fill="white")
                                else:
                                    c.itemconfigure(serviceSwitch2s, fill=GRAY)
                                    c.itemconfigure(serviceSwitch2m, fill="white")

        time.sleep(0.5)

def heartbeat():
    global lastMsg
    global nodes

    for i in range(0,256):
        sendCmd(i, 1)
        time.sleep(0.3)

    while True:
        time.sleep(5)
        for i in range(0, 256):
            if lastMsg[i] != 0:

                if 'G' in nodes[i].heads or 'A' in nodes[i].heads or 'R' in nodes[i].heads:
                    if (lastMsg[i] + SIXMIN) < time.time():
                        sendCmd(i, 1)
                        time.sleep(0.2)
                else:
                    if (lastMsg[i] + HOUR) < time.time():
                        sendCmd(i, 1)
                        time.sleep(0.2)
                
                if (lastMsg[i] + HOUR + 250) < time.time():
                    lastMsg[i] = 0
                    nodes[i].heads = ['O' for x in range(0,4)]
                    nodes[i].captures = [False for x in range(0,4)]
                    nodes[i].releases = [False for x in range(0,4)]
                    nodes[i].turnouts = [False for x in range(0,4)]
                    nodes[i].ledError = [False for x in range(0,16)]

#Button(gui, text="QUIT", command=quit_gui).place(x=0, y=0)

#threads
dataThread = threading.Thread(target=get_data)
dataThread.daemon = True 

guiThread = threading.Thread(target=update_gui)
guiThread.daemon = True

heartbeatThread = threading.Thread(target=heartbeat)
heartbeatThread.daemon = True

#main function calls
loadHeads()
loadDiags()
drawMap()
drawButtons()

sW = screenW/DEVW
sH = screenH/DEVH

scale = min(sW, sH)

c.scale('all', 0, 0, scale, scale)

if(len(sys.argv) > 1):
    value_inside.set(sys.argv[1])
    setSerial()
else:
    #list all available ports
    available_ports = list(serial.tools.list_ports.comports())
    serial_ports = []
    for port in available_ports:
        serial_ports.append(port.device)

    if len(serial_ports) == 0:
        print("NO SERIAL DEVICES")
        sys.exit("NO SERIAL DEVICES")
    
    # Set the default value of the variable 
    value_inside.set("Select an Option") 

    optionmenu = OptionMenu(serialSelect, value_inside, *serial_ports)
    optionmenu.pack()

    submit_button = Button(serialSelect, text='Submit', command=setSerial) 
    submit_button.pack()
    
    serialSelect.attributes('-topmost', True)

guiThread.start()

gui.mainloop()
