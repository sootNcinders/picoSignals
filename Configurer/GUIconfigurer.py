import json
import tkinter as tk
from tkinter import filedialog

def fileFinder():
    global file
    global fileLabel

    file = filedialog.askopenfilename(filetypes=[('JSON File', '*.json')])
    fileLabel.config(text=file)
    print(file)

#open a file diag window to select a directory to save the file in
def folderFinder():
    global folder

    folder = filedialog.askdirectory()
    print(folder)

window = tk.Tk(screenName="Signal Configurer")

file = "JSON"
fileLabel = tk.Label(window, text=file)
fileLabel.pack()

oF = tk.Button(window, text="Open File", command=fileFinder)
oF.pack()

sF = tk.Button(window, text="Save File", command=folderFinder)
sF.pack()

addr = tk.IntVar(value=1)
tk.Label(window, text="Address").pack()
tk.Spinbox(window, from_=1, to=255, textvariable=addr, wrap=True).pack()

retries = tk.IntVar(value=10)
tk.Label(window, text="Number of Retries").pack()
tk.Spinbox(window, from_=1, to=255, textvariable=retries, wrap=True).pack()

retryTime = tk.IntVar(value=100)
tk.Label(window, text="Retry Time in ms").pack()
tk.Spinbox(window, from_=1, to=255, textvariable=retryTime, wrap=True).pack()

dimTime = tk.IntVar(value=15)
tk.Label(window, text="Time before Dimming in Minutes").pack()
tk.Spinbox(window, from_=1, to=255, textvariable=dimTime, wrap=True).pack()

sleepTime = tk.IntVar(value=30)
tk.Label(window, text="Time before Sleeping in Minutes").pack()
tk.Spinbox(window, from_=1, to=255, textvariable=sleepTime, wrap=True).pack()

lowVolts = tk.DoubleVar(value=11.0)
tk.Label(window, text="Low Battery Threshold Voltage").pack()
tk.Spinbox(window, from_=0.1, to=36.0, increment=0.1, textvariable=lowVolts, wrap=True).pack()

resetVolts = tk.DoubleVar(value=12.0)
tk.Label(window, text="Low Battery Reset Voltage").pack()
tk.Spinbox(window, from_=0.1, to=36.0, increment=0.1, textvariable=resetVolts, wrap=True).pack()

#tk.Label(window, text="CTC Map Present").pack()
ctc = tk.Checkbutton(window, text="CTC Map Present")
ctc.pack()

heads = tk.IntVar(value=1)
tk.Label(window, text="Number of Heads").pack()
tk.Spinbox(window, from_=1, to=4, textvariable=heads, wrap=True).pack()

window.mainloop()