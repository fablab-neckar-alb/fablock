import tkinter as tk
from tkinter import messagebox
from serial_comm import SerialComm
from messages import parse_message

import logging

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s]: %(message)s")

class FablockMainScreen:
    def __init__(self, root, serial_comm):
        self.root = root
        self.serial_comm = serial_comm
        self.setup_ui()

    def setup_ui(self):
        self.status_label = tk.Label(self.root, text="Status: Offline", font=("Arial", 12))
        self.status_label.pack(pady=5)

        # Grouping related labels into a frame
        status_frame = tk.Frame(self.root, borderwidth=5, relief="groove")
        status_frame.pack(pady=5)

        

        
        self.door_status_label = tk.Label(status_frame, text="Door: Unknown", font=("Arial", 12))
        self.door_status_label.pack(pady=5)

        self.awake_status_label = tk.Label(status_frame, text="Awake: reset", font=("Arial", 12))
        self.awake_status_label.pack(pady=5)

        self.pin_status_label = tk.Label(status_frame, text="PIN: Unknown", font=("Arial", 12))
        self.pin_status_label.pack(pady=5)

        self.sense_status_label = tk.Label(status_frame, text="Motor ADC: Unknown", font=("Arial", 12))
        self.sense_status_label.pack(pady=5)

        # Grouping related labels into a frame
        connect_frame = tk.Frame(self.root, borderwidth=5, relief="sunken")
        connect_frame.pack(pady=5)

        self.connect_button = tk.Button(connect_frame, text="Connect", command=self.connect_serial, font=("Arial", 12))
        self.connect_button.pack(pady=10)

        self.disconnect_button = tk.Button(connect_frame, text="Disconnect", command=self.disconnect_serial, font=("Arial", 12), state=tk.DISABLED)
        self.disconnect_button.pack(pady=10)

        command_frame = tk.Frame(self.root, borderwidth=5, relief="solid")
        command_frame.pack(pady=5)

        self.open_button = tk.Button(command_frame, text="Open", command=self.open_command, font=("Arial", 12))
        self.open_button.pack(pady=10)

        self.close_button = tk.Button(command_frame, text="Close", command=self.close_command, font=("Arial", 12))
        self.close_button.pack(pady=10)


    # Callback to update the GUI with parsed messages
    def update_status(self,message):
        if len(message) == 0:
            return
        
        parsed_data = parse_message(message)
        if not parsed_data:
            logging.info(f"'{message}' with len {len(message)} was not parsed")
            return
        logging.debug(parsed_data)
        if parsed_data["type"] == "DOOR":
            self.door_status_label.config(
                text=f"Door: {parsed_data['locked']}, {parsed_data['closed']}, "
                    f"{parsed_data['motor_state']}, Action: {parsed_data['motor_action']}"
            )
        elif parsed_data["type"] == "AWAKE":
            self.awake_status_label.config(text=f"Awake: {parsed_data['awake']}")
        elif parsed_data["type"] == "PIN":
            self.pin_status_label.config(text=f"PIN: {parsed_data['value']}")
        elif parsed_data["type"] == "SENSE":
            self.sense_status_label.config(text=f"Motor ADC: {parsed_data['value']}")
        elif parsed_data["type"] == "OK":
            pass
        else:
            logging.info(f"Message type {parsed_data['type']} was not evaluated")

    # Connect button functionality
    def connect_serial(self):
        try:
            self.serial_comm.connect()
            self.serial_comm.set_callback(self.update_status)
            self.connect_button.config(state=tk.DISABLED)
            self.disconnect_button.config(state=tk.NORMAL)
            self.status_label.config(text="Connected to Serial Port")
        except Exception as e:
            logging.error("Open the serial connection failed:" +str(e))

    # Disconnect button functionality
    def disconnect_serial(self):
        try:
            self.serial_comm.disconnect()
            self.connect_button.config(state=tk.NORMAL)
            self.disconnect_button.config(state=tk.DISABLED)
            self.status_label.config(text="Disconnected from Serial Port")
        except Exception as e:
            logging.error("Closing the serial connection failed:" +str(e))

    # Open button functionality
    def open_command(self):
        try:
            self.serial_comm.send_command("!D1\r")
            self.status_label.config(text="Open Command Sent (!D1)")
        except Exception as e:
            logging.error("Sending open command failed:" +str(e))

    # Close button functionality
    def close_command(self):
        try:
            self.serial_comm.send_command("!D0\r")
            self.status_label.config(text="Close Command Sent (!D0)")
        except Exception as e:
            logging.error("Sending close command failed:" +str(e))

