import json

config = {
    "address" : 1
}

head = [{}, {}, {}, {}]

val = 0

while val <= 0 or val > 255:
    print("Address 1-255: ")
    val = int(input())
config.update({"address" : val})
addr = val

num = 0
while num <= 0 or num > 4:
    print("Number of Heads: ")
    num = int(input())

for i in range(0, num):
    val = ' '
    while val != 'n' and val != 'y':
        print("Is Head " + str(i) + " RGB? [y or n]")
        val = input()
    rgb = (val == 'y')

    numDests = 0
    while numDests < 1 or numDests > 6:
            print("Number of Destinations " + " 1-6: ")
            numDests = int(input())

    dests = [0,0,0,0,0,0]
    for x in range(0, numDests):
        val = 0
        while val < 1 or val > 255:
            if(x == 0):
                print("Head " + str(i) + " Primary Destination" + " 1-255: ")
            else:
                print("Head " + str(i) + " Secondary Destination " + str(x) + " 1-255: ")
            val = int(input())
        dests[x] = val
    head[i].update({"destination" : dests})

    val = -1
    while val < 0 or val > 255:
        print("Dim brightness 0-255: ")
        val = int(input())
    head[i].update({"dim" : val})

    if(rgb):
        colors = ["Red:", "Green:", "Blue:", "Amber:", "Lunar:"]
        colorslc = ["red", "green", "blue", "amber", "lunar"]

        for x in range(0, 3):
            print(colors[x])

            val = -1
            while val < 0 or val > 15:
                print("Pin # 0-15: ")
                val = int(input())
            pin = val

            val = -1
            while val < 0 or val > 58:
                print("Current (mA) 0-58:")
                val = int(input())
            current = val

            head[i].update({colorslc[x] : {"pin" : pin, "current" : current}})
        
        print("RGB Values:")
        for x in range(0, 5):
            if colorslc[x] != "blue":
                print(colors[x])

                val = -1
                while val < 0 or val > 255:
                    print("Red 0-255: ")
                    val = int(input())
                red = val

                val = -1
                while val < 0 or val > 255:
                    print("Green 0-255: ")
                    val = int(input())
                green = val

                val = -1
                while val < 0 or val > 255:
                    print("Blue 0-255: ")
                    val = int(input())
                blue = val

                if x < 3:
                    head[i][colorslc[x]].update({"rgb" : [red, green, blue]})
                else:
                    head[i].update({colorslc[x] : {"rgb" : [red, green, blue]}})


    else:
        colors = ["Green:", "Amber:", "Red:"]
        colorslc = ["green", "amber", "red"]

        for x in range(0, 3):
            print(colors[x])

            val = -1
            while val < 0 or val > 15:
                print("Pin # 0-15: ")
                val = int(input())
            pin = val

            val = -1
            while val < 0 or val > 58:
                print("Current (mA) 0-58:")
                val = int(input())
            current = val

            val = 0
            while val < 1 or val >255:
                print("Brightness 0-255:")
                val = int(input())
            brightness = val

            head[i].update({colorslc[x] : {"pin" : pin, "current": current, "brightness": brightness}})
    
    config.update({"head" + str(i):head[i]})

val = 0 
while val < 2 or val > 8:
    print("Number of Inputs 2-8:")
    val = int(input())
num = val

for i in range(0, num):
    val = ' '
    while val != 'c' and val != 'r' and val != 't':
        print("Pin " + str(i) + " Function: c for capture, r for release, t for turnout:")
        val = input()
    
    if val == 'c':
        val = -1
        while val < 0 or val > 3:
            print("Head Nuber 0-3:")
            val = int(input())
        headNum = val

        config.update({"pin" + str(i): {"mode" : "capture", "head1" : headNum}})

        val = ' '
        while val != 'n' and val != 'y':
            print("Second Head? y or n:")
            val = input()
        
        if(val == 'y'):
            val = -1
            while val < 0 or val > 3:
                print("Head Nuber 0-3:")
                val = int(input())
            headNum = val

            val = -1
            while val < 0 or val > 8:
                print("Turnout Pin Number:")
                val = int(input())
            turnoutNum = val

            config.update({"pin" + str(i): {"head2" : headNum, "turnout" : turnoutNum}})
        
    elif val == 'r':
        val = -1
        while val < 0 or val > 3:
            print("Head Nuber 0-3:")
            val = int(input())
        headNum = val

        config.update({"pin" + str(i): {"mode" : "release", "head" : headNum}})
    
    elif val == 't':
        config.update({"pin" + str(i): {"mode" : "turnout"}})

with open("config" + str(addr) + ".json", 'w') as config_file:
    json.dump(config, config_file, indent=4)