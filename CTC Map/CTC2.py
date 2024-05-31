import queue
import threading
from tkinter import *
from tkinter import ttk
import serial
import serial.tools.list_ports
import datetime
import time
import sys
import platform

GRAY = "#555"

#guis
gui = Tk()
gui.attributes('-fullscreen', True)
gui.resizable(False, False)
c = Canvas(gui, bg="black", height=1080, width=1920)
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

nodes = [node() for i in range(0,256)]
rssis = [0 for i in range(0,256)]
volts = [0 for i in range(0,256)]
caps  = [[0 for i in range(0,4)] for j in range(0,256)]
rels  = [[0 for i in range(0,4)] for j in range(0,256)]

diagVisible = False

changed = False

def do_rclick(event):
    global dest
    global headNo
    
    x = event.x_root
    y = event.y_root

    if "Darwin" in platform.system():
        y = y - 40 #offset 40 pixels for mac with notch

    print("RCLICK X:" + str(x) + " Y:" + str(y))

    try:
        if x >= 700 and x <= 720 and y >= 670 and y <= 690:
            dest = 1
            headNo = 0
            m.tk_popup(x, y)
        elif x >= 220 and x <= 240 and y >= 710 and y <= 730:
            dest = 2
            headNo = 0
            m.tk_popup(x, y)
        elif x >= 990 and x <= 1010 and y >= 715 and y <= 755:
            dest = 17
            if y >= 735:
                headNo = 0
            else:
                headNo = 1
            m.tk_popup(x, y)
        elif x >= 1680 and x <= 1700 and y >= 740 and y <= 760:
            dest = 18
            headNo = 0
            m.tk_popup(x, y)
        elif x>= 1400 and x <= 1420 and y >= 1000 and y <= 1020:
            dest = 19
            headNo = 0
            m.tk_popup(x, y)
        elif x >= 1370 and x <= 1390 and y >= 730 and y <= 750:
            dest = 21
            headNo = 0
            m.tk_popup(x, y)
        else:
            dest = 255
            headNo = 0
            m.tk_popup(x, y)
            
    finally:
        m.grab_release()
        
c.bind("<Button-3>", do_rclick)
c.bind("<Button-2>", do_rclick)

def sendCmd(addr: int, cmd: int):
    global radio
    #Serial Out:
    #: - Start Character
    #00 - Destination node, 0-FF
    #00 - Command, 0-FF
    #       00 - ACK
    #       01 - Ping
    #       02 - Wake
    #       03 - Capture 1
    #       04 - Capture 2
    #       05 - Capture 3
    #       06 - Capture 4
    #       07 - Release 1
    #       08 - Release 2
    #       09 - Release 3
    #       0A - Release 4

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

    for i in range(0, len(head)):
        if head[i][0] != 0:
            sendCmd(i, 1)
    return None

if(len(sys.argv) > 1):
    value_inside.set(sys.argv[1])
    setSerial()
else:
    #list all available ports
    available_ports = list(serial.tools.list_ports.comports())
    serial_ports = []
    for port in available_ports:
        serial_ports.append(port.device)
    
    # Set the default value of the variable 
    value_inside.set("Select an Option") 

    optionmenu = OptionMenu(serialSelect, value_inside, *serial_ports)
    optionmenu.pack()

    submit_button = Button(serialSelect, text='Submit', command=setSerial) 
    submit_button.pack() 

def loadHeads():
    global head
    #parrish east
    #head[ 1][0] = c.create_oval(1600,  270, 1620,  290, fill="black")
    head[1][0] = c.create_oval(700,  670, 720,  690, fill="black")
    #etowah west
    #head[ 2][0] = c.create_oval( 300,  310,  320,  330, fill="black")
    head[2][0] = c.create_oval(220,  710,  240,  730, fill="black")
    #Etowah East
    c.create_oval(1500, 590, 1520, 570, fill="black")
    #Alloy
    c.create_oval(1120, 630, 1140, 650, fill="black")
    #Reid West
    c.create_oval(1000, 610, 1020, 630, fill="black")
    #Reid East
    c.create_oval(700, 590, 720, 570, fill="black")
    #Cotton Mill
    c.create_oval(320, 630, 340, 650, fill="black")
    #Midland West
    c.create_oval(220, 610, 240, 630, fill="black")
    #Midland East
    c.create_oval(1500, 490, 1520, 470, fill="black")
    #Sargent West
    c.create_oval(1000, 510, 1020, 530, fill="black")
    #Sargent East
    c.create_oval(700, 490, 720, 470, fill="black")
    #St. Paul West
    c.create_oval(220, 510, 240, 530, fill="black")
    #St. Paul East
    c.create_oval(1500, 370, 1520, 350, fill="black")
    c.create_oval(1500, 390, 1520, 370, fill="black")
    #Elizabeth West
    c.create_oval(670, 60, 690, 80, fill="black")
    #Elizabth East
    c.create_oval(350, 50, 370, 70, fill="black")
    #Toonigh
    c.create_oval(520, 390, 540, 370, fill="black")
    #parrish west
    #head[17][0] = c.create_oval( 190,  520,  210,  540, fill="black")
    #head[17][1] = c.create_oval( 190,  540,  210,  560, fill="black")
    head[17][0] = c.create_oval(990,  735,  1010,  755, fill="black")
    head[17][1] = c.create_oval(990,  715,  1010,  735, fill="black")
    #greyrock
    #head[18][0] = c.create_oval(1090,  540, 1110,  560, fill="black")
    head[18][0] = c.create_oval(1680,  740, 1700,  760, fill="black")
    #canton
    #head[19][0] = c.create_oval( 810,  900,  830,  920, fill="black")
    head[19][0] = c.create_oval(1400, 1020, 1420, 1000, fill="black")
    #upper serivce leads
    #head[21][0] = c.create_oval( 670,  530,  690,  550, fill="black")
    head[21][0] = c.create_oval(1370,  730, 1390,  750, fill="black")

    c.pack()

def loadDiags():
    global rssis
    global volts
    global caps
    global rels
    global c

    #Parrish West
    volts[1] = c.create_text(745, 675, text="00.0V", fill="black", font=('Times', 14))
    rssis[1] = c.create_text(740, 690, text="-000", fill="black", font=('Times', 14))
    caps[1][0] = c.create_text(735, 660, text="C1", fill="black", font=('Times', 14))
    rels[1][0] = c.create_text(755, 660, text="R1", fill="black", font=('Times', 14))
    #Etowah East
    volts[2] = c.create_text(265, 715, text="00.0V", fill="black", font=('Times', 14))
    rssis[2] = c.create_text(260, 730, text="-000", fill="black", font=('Times', 14))
    caps[2][0] = c.create_text(255, 745, text="C1", fill="black", font=('Times', 14))
    rels[2][0] = c.create_text(275, 745, text="R1", fill="black", font=('Times', 14))

    #Parrish West
    volts[17] = c.create_text(965, 735, text="00.0V", fill="black", font=('Times', 14))
    rssis[17] = c.create_text(970, 750, text="-000", fill="black", font=('Times', 14))
    caps[17][0] = c.create_text(955, 765, text="C1", fill="black", font=('Times', 14))
    rels[17][0] = c.create_text(975, 765, text="R1", fill="black", font=('Times', 14))
    rels[17][1] = c.create_text(995, 765, text="R2", fill="black", font=('Times', 14))
    #Greyrock
    volts[18] = c.create_text(1685, 700, text="00.0V", fill="black", font=('Times', 14))
    rssis[18] = c.create_text(1690, 715, text="-000", fill="black", font=('Times', 14))
    caps[18][0] = c.create_text(1675, 730, text="C1", fill="black", font=('Times', 14))
    rels[18][0] = c.create_text(1695, 730, text="R1", fill="black", font=('Times', 14))
    #Canton
    volts[19] = c.create_text(1445, 1005, text="00.0V", fill="black", font=('Times', 14))
    rssis[19] = c.create_text(1440, 1020, text="-000", fill="black", font=('Times', 14))
    caps[19][0] = c.create_text(1435, 990, text="C1", fill="black", font=('Times', 14))
    rels[19][0] = c.create_text(1455, 990, text="R1", fill="black", font=('Times', 14))
    #Upper Service Leads
    volts[21] = c.create_text(1415, 735, text="00.0V", fill="black", font=('Times', 14))
    rssis[21] = c.create_text(1410, 750, text="-000", fill="black", font=('Times', 14))
    caps[21][0] = c.create_text(1405, 765, text="C1", fill="black", font=('Times', 14))
    rels[21][0] = c.create_text(1425, 765, text="R1", fill="black", font=('Times', 14))

def drawMap():
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
    c.create_line(1750, 400, 1500, 400, 1520, 420, 1750, 420, width=lineWidth, fill="white")


    #Bryan Wye
    c.create_line(1420, 320, 1360, 400, width=lineWidth, fill=GRAY)
    #Eagle Ridge Spur
    c.create_line(650, 70, 670, 90, 1200, 90, width=lineWidth, fill=GRAY)
    #D&MT Jct
    c.create_line(650, 70, 610, 110, 0, 110, width=lineWidth, fill=GRAY)
    c.create_line(600, 90, 630, 90, width=lineWidth, fill=GRAY)
    c.create_line(0, 210, 100, 110, width=lineWidth, fill=GRAY)
    #Toonigh Yard
    c.create_line(360, 280, 460, 380, 500, 380, width=lineWidth, fill=GRAY)
    c.create_line(380, 280, 480, 380, width=lineWidth, fill=GRAY)
    c.create_line(400, 280, 520, 400, width=lineWidth, fill=GRAY)
    #Elizabeth Loop Main
    c.create_line(1460, 360, 1420, 320, 1420, 150, 1320, 50, 400, 50, 300, 150, 300, 300, 400, 400, 1443, 400, width=lineWidth, fill="white")
    #Elizabeth Siding
    c.create_line(380, 70, 650, 70, 670, 50, width=lineWidth, fill="white")
    #Harpur Spur
    c.create_line(1400, 400, 1300, 300, 750, 300, width=lineWidth, fill="white")
    c.create_line(750, 300, 500, 300, width=lineWidth, fill=GRAY)


    global uWyeW
    uWyeW = c.create_line(1500, 400, 1443, 400, width=lineWidth, fill=GRAY)
    global uWyeE
    uWyeE = c.create_line(1500, 400, 1460, 360, width=lineWidth, fill="white")

    #Midland - St. Paul Main
    c.create_line(0, 500, 1750, 500, width=lineWidth, fill="white")
    #St. Paul Siding Lower
    c.create_line(0, 520, 200, 520, 220, 500, width=lineWidth, fill="white")
    #Sargent Siding
    c.create_line(700, 500, 720, 520, 980, 520, 1000, 500, width=lineWidth, fill="white")
    #Midland Siding Upper
    c.create_line(1500, 500, 1520, 520, 1750, 520, width=lineWidth, fill="white")

    #Midland Siding Lower
    c.create_line(0, 620, 200, 620, 220, 600, width=lineWidth, fill="white")
    #Cotton Mill Spur
    c.create_line(300, 600, 320, 620, 520, 620, width=lineWidth, fill=GRAY)
    #Reid Siding
    c.create_line(700, 600, 720, 620, 980, 620, 1000, 600, width=lineWidth, fill="white")
    #Alloy Spur
    c.create_line(1100, 600, 1120, 620, 1320, 620, width=lineWidth, fill=GRAY)
    #Etowah Siding Upper
    c.create_line(1500, 600, 1520, 620, 1750, 620, width=lineWidth, fill="white")
    #Etowah - Midland Main
    c.create_line(0, 600, 1750, 600, width=lineWidth, fill="white")

    #Parrish - Etowah Main
    c.create_line(0, 700, 1000, 700, width=lineWidth, fill="white")
    #Parrish Siding
    c.create_line(700, 700, 720, 720, 980, 720, 1000, 700, width=lineWidth, fill="white")
    #Etowah Siding Lower
    c.create_line(0, 720, 200, 720, 220, 700, width=lineWidth, fill="white")
    #wye swtich
    global wyeE
    #wyeE = c.create_line(200, 500, 257, 500, width=lineWidth, fill="#333")
    wyeE = c.create_line(1000, 700, 1057, 700, width=lineWidth, fill=GRAY)
    global wyeW
    #wyeW = c.create_line(200, 500, 240, 540, width=lineWidth, fill="white")
    wyeW = c.create_line(1000, 700, 1040, 740, width=lineWidth, fill="white")
    #wye 3rd leg
    #c.create_line(280, 580, 360, 500, width=lineWidth, fill="#333")
    c.create_line(1080, 780, 1160, 700, width=lineWidth, fill=GRAY)
    #Canton Yard
    c.create_line(1180, 1050, 850, 1050, width=lineWidth, fill=GRAY)
    c.create_line(1000, 1050, 900, 950, width=lineWidth, fill=GRAY)
    c.create_line(1160, 1030, 1000, 1030, width=lineWidth, fill=GRAY)
    c.create_line(1140, 1030, 1060, 950, 1060, 800, width=lineWidth, fill=GRAY)
    c.create_line(1120, 1030, 1040, 950, 1040, 800, width=lineWidth, fill=GRAY)
    c.create_line(1100, 1030, 1020, 950, 1020, 800, width=lineWidth, fill=GRAY)
    c.create_line(1080, 1030, 1000, 950, 1000, 800, width=lineWidth, fill=GRAY)
    c.create_line(1060, 1030, 980, 950, 980, 800, width=lineWidth, fill=GRAY)
    c.create_line(1040, 1030, 960, 950, 960, 800, width=lineWidth, fill=GRAY)
    c.create_line(1020, 1030, 940, 950, 940, 800, width=lineWidth, fill=GRAY)
    c.create_line(1000, 1030, 920, 950, 920, 800, width=lineWidth, fill=GRAY)
    #canton loop
    #c.create_line(240, 540, 280, 580, 280, 850, 380, 950, 1000, 950, 1100, 850, 1100, 580, 1080, 560, width=lineWidth, fill="white")
    c.create_line(1040, 740, 1080, 780, 1080, 950, 1180, 1050, 1600, 1050, 1700, 950, 1700, 780, 1680, 760, width=lineWidth, fill="white")
    #canton loop tunnel to service track switch
    #c.create_line(1020, 500, 850, 500, width=lineWidth, fill="white")
    c.create_line(1620, 700, 1450, 700, width=lineWidth, fill="white")
    #canton loop wye to service track swtich
    #c.create_line(822, 500, 257, 500, width=lineWidth, fill="white")
    c.create_line(1422, 700, 1057, 700, width=lineWidth, fill="white")
    #tunnel
    #c.create_line(1080, 560, 1020, 500, width=lineWidth, fill="white", dash=(7, 7))
    c.create_line(1680, 760, 1620, 700, width=lineWidth, fill="white", dash=(7, 7))
    #canton siding
    #c.create_line(400, 930, 800, 930, 820, 950, width=lineWidth, fill="white")
    c.create_line(1200, 1030, 1400, 1030, 1420, 1050, width=lineWidth, fill="white")
    #greyrock siding 
    #c.create_line(800, 930, 990, 930, 1080, 840, 1080, 560, width=lineWidth, fill="white")
    c.create_line(1400, 1030, 1590, 1030, 1680, 940, 1680, 760, width=lineWidth, fill="white")
    #upper service lead switch
    global serviceSwitch1s
    #serviceSwitch1s = c.create_line(850, 500, 830, 520, width=lineWidth, fill="#333")
    serviceSwitch1s = c.create_line(1450, 700, 1430, 720, width=lineWidth, fill=GRAY)
    global serviceSwitch1m
    #serviceSwitch1m = c.create_line(850, 500, 822, 500, width=lineWidth, fill="white")
    serviceSwitch1m = c.create_line(1450, 700, 1422, 700, width=lineWidth, fill="white")
    #lower service lead switch 
    global serviceSwitch2s
    #serviceSwitch2s = c.create_line(400, 930, 380, 910, width=lineWidth, fill="#333")
    serviceSwitch2s = c.create_line(1200, 1030, 1180, 1010, width=lineWidth, fill=GRAY)
    global serviceSwitch2m
    #serviceSwitch2m = c.create_line(400, 930, 380, 950, width=lineWidth, fill="white")
    serviceSwitch2m = c.create_line(1200, 1030, 1180, 1050, width=lineWidth, fill="white")
    #service leads
    #c.create_line(380, 910, 300, 830, 300, 590, 370, 520, 830, 520, width=lineWidth, fill="white")
    c.create_line(1180, 1010, 1100, 930, 1100, 790, 1170, 720, 1430, 720, width=lineWidth, fill="white")
    #c.create_line(360, 890, 360, 860, 320, 820, 320, 600, 380, 540, 650, 540, 670, 520, width=lineWidth, fill="white")
    c.create_line(1160, 990, 1160, 960, 1120, 920, 1120, 800, 1180, 740, 1350, 740, 1370, 720, width=lineWidth, fill="white")

    #siding labels
    c.create_text(500, 90, text="Elizabeth", fill="white", font=('Times', 16))
    c.create_text(800, 110, text="Eagle Ridge Branch", fill="white", font=('Times', 16))
    c.create_text(100, 90, text="Duluth & Mt. Tabor RR", fill="white", font=('Times', 16))
    c.create_text(400, 260, text="Toonigh", fill="white", font=('Times', 16))
    c.create_text(650, 280, text="Harpur", fill="white", font=('Times', 16))
    c.create_text(1650, 440, text="St. Paul", fill="white", font=('Times', 16))
    c.create_text(75, 540, text="St. Paul", fill="white", font=('Times', 16))
    c.create_text(850, 540, text="Sargent", fill="white", font=('Times', 16))
    c.create_text(1650, 540, text="Midland", fill="white", font=('Times', 16))
    c.create_text(75, 640, text="Midland", fill="white", font=('Times', 16))
    c.create_text(400, 640, text="Cotton Mill", fill="white", font=('Times', 16))
    c.create_text(850, 640, text="Reid", fill="white", font=('Times', 16))
    c.create_text(1200, 640, text="Alloy", fill="white", font=('Times', 16))
    #c.create_text(1800, 280, text="Parrish", fill="white", font=('Times', 16))
    #c.create_text(100, 480, text="Parrish", fill="white", font=('Times', 16))
    c.create_text(850, 740, text="Parrish", fill="white", font=('Times', 16))
    #c.create_text(150, 260, text="Etowah", fill="white", font=('Times', 16))
    c.create_text(75, 740, text="Etowah", fill="white", font=('Times', 16))
    c.create_text(1650, 640, text="Etowah", fill="white", font=('Times', 16))
    #c.create_text(600, 910, text="Canton", fill="white", font=('Times', 16))
    c.create_text(1300, 1010, text="Canton", fill="white", font=('Times', 16))
    #c.create_text(1150, 700, text="Greyrock", fill="white", font=('Times', 16))
    c.create_text(1625, 800, text="Greyrock", fill="white", font=('Times', 16))

    c.create_text(450, 850, text="Canton, St. Paul, & Pacfic Rwy.", fill="white", font=('Times', 60))
    c.create_text(450, 1000, text="E <-> W", fill="white", font=('Times', 60))

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
    global rssis
    global volts
    global caps
    global rels

    for i in range(0, len(rssis)):
        if rssis[i] != 0:
            if diagVisible:
                c.itemconfigure(rssis[i], fill="black")
            else:
                c.itemconfigure(rssis[i], fill="white")
        if volts[i] != 0:
            if diagVisible:
                c.itemconfigure(volts[i], fill="black")
            else:
                c.itemconfigure(volts[i], fill="white")
        for x in range(0,4):
            if caps[i][x] != 0:
                if diagVisible:
                    c.itemconfigure(caps[i][x], fill="black")
                else:
                    c.itemconfigure(caps[i][x], fill=GRAY)
            if rels[i][x] != 0:
                if diagVisible:
                    c.itemconfigure(rels[i][x], fill="black")
                else:
                    c.itemconfigure(rels[i][x], fill=GRAY)
    diagVisible = not diagVisible

def drawButtons():
    Button(gui, text="QUIT", command=quit_gui).place(x=0, y=0)
    Button(gui, text="TOGGLE DIAG", command=drawDiag).place(x=65, y=0)

def get_data():
    global radio
    global nodes
    global changed
    global f
    global f2

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
                
                f.write(str(datetime.datetime.now()) + "\n") 
                f.write("Sender: " + str(node) + "\n")
                for i in range(0,4):
                    f.write("Head " + str(i) + ": " + nodes[node].heads[i] + "\n")
                f.write("Captures: " + lnIn[7] + "\n")
                f.write("Releases: " + lnIn[8] + "\n")
                f.write("Turnouts: " + lnIn[9] + "\n")
                f.write("Voltage: " + str(nodes[node].voltage) + "\n")
                f.write("RSSI: " + str(nodes[node].rssi) + "\n\n")

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

    while True:
        if changed:
            for i in range(0, len(head)):
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
                        else:
                            c.itemconfigure(head[i][x], fill="black")

                        if volts[i] != 0:
                            c.itemconfigure(volts[i], text=(format(nodes[i].voltage, '02.1f')+"V"))
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
            changed = False
        time.sleep(0.5)

#Button(gui, text="QUIT", command=quit_gui).place(x=0, y=0)

#threads
dataThread = threading.Thread(target=get_data)
dataThread.daemon = True

guiThread = threading.Thread(target=update_gui)
guiThread.daemon = True
guiThread.start()

#main function calls
loadHeads()
loadDiags()
drawMap()
drawButtons()

gui.mainloop()
