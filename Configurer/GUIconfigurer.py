import json
import tkinter as tk
from tkinter import filedialog

def fileFinder():
    global file
    global fileLabel

    file = filedialog.askopenfilename(filetypes=[('JSON File', '*.json')])
    fileLabel.config(text=file)
    print(file)

window = tk.Tk(screenName="Signal Configurer")

file = "JSON"
fileLabel = tk.Label(window, text=file)
fileLabel.pack()

b = tk.Button(window, text="Choose File", command=fileFinder)
b.pack()

label = tk.Label(window, text="Number of Heads")
label.pack()
box = tk.Spinbox(window, from_=1, to=4, wrap=True)
box.pack()

window.mainloop()