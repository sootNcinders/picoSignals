import json
import tkinter as tk

window = tk.Tk(screenName="Signal Configurer")

label = tk.Label(text="Number of Heads")
label.pack()
box = tk.Spinbox(window, from_=1, to=4, wrap=True)
box.pack()

window.mainloop()