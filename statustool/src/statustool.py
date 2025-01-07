"""
Create a Python script using tkinter to debug the atmega of my lock project with the following features:

    Status Display
      A window with labels and icons to display the status various states and information.
      There are several status messages send by the aduino. They are indicated with a keyword and separated from the value with the equal sign.
      Door Status:
          The Door status will be indicated by a string that starts looks like "DOOR"
          The payload will be a four byte ascii string like "1002" with 4 values where bytes have the following meaning:
            payload[0] is '0' if the door is unlocked and '1' if the door is locked
            payload[1] is '0' if the door is open and '1' door is closed
            payload[2] has several values {'0':"Motor stopped", '1': "motor driving close" , '2': "motor driving open", 
            payload[3] is '1' if the lase motor action as finished successfully
      Awake Status:
          The awake status will be indicated by a string that starts looks like "AWAKE"
          The payload will be one byte. either '1' or '0' idicating that the arduino is running or sleep state.
      PIN Message:
          The "PIN" message indicates that the user has terminated the pi entry
          the payload will we the entered pin code
      Sense Message
          The sense message will send the value of the adc of the motor shunt
    Four buttons:
        Connect: Opens the serial port /dev/ttyUSB0 with a baud rate of 9600. Starts a thread to read data continuously from the serial port. Update the labels with values sent by the connected device (e.g., DOOR=1112, SENSE=708, AWAKE=1, PIN=1234).
        Disconnect: Closes the serial port and stops the data-reading thread. Provide error handling for failures.
        Open: Sends the command !D1\r to the serial device if the serial port is connected. Update the Status 1 label to show "Open Command Sent (!D1)".
        Close: Sends the command !D0\r to the serial device if the serial port is connected. Update the Status 1 label to show "Close Command Sent (!D0)".
    Ensure that:
        When the serial port is connected, the Connect button is disabled, and the Disconnect button is enabled (and vice versa).
        If the serial port is not connected, clicking Open or Close should show an appropriate error message in Status 1.
    Use a thread for reading serial data so the GUI remains responsive.
    Include error handling for serial connection failures and invalid input.




"""
import tkinter as tk
from serial_comm import SerialComm
from messages import parse_message
from FablockMainScreen import FablockMainScreen

# Initialize the GUI
root = tk.Tk()

# Initialize SerialComm object
serial_comm = SerialComm()

ui = FablockMainScreen(root,serial_comm)

# Run the application
root.mainloop()
