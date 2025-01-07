import serial
import threading

class SerialComm:
    def __init__(self, port="/dev/ttyUSB0", baudrate=9600):
        self.port = port
        self.baudrate = baudrate
        self.ser = None
        self.serial_thread_running = False
        self.callback = None  # Function to handle parsed messages

    def connect(self):
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=1)
            self.serial_thread_running = True
            threading.Thread(target=self._read_serial_data, daemon=True).start()
        except Exception as e:
            raise ConnectionError(f"Failed to connect to serial port: {e}")

    def disconnect(self):
        try:
            self.serial_thread_running = False
            if self.ser and self.ser.is_open:
                self.ser.close()
        except Exception as e:
            raise ConnectionError(f"Failed to disconnect: {e}")

    def send_command(self, command):
        try:
            if self.ser and self.ser.is_open:
                self.ser.write(command.encode('utf-8'))
            else:
                raise ConnectionError("Serial port is not connected")
        except Exception as e:
            raise IOError(f"Failed to send command: {e}")

    def set_callback(self, callback):
        """Set a callback function to handle parsed messages."""
        self.callback = callback

    def _read_serial_data(self):
        while self.serial_thread_running:
            try:
                if self.ser and self.ser.is_open:
                    line = self.ser.readline().decode('utf-8').strip()
                    if self.callback:
                        self.callback(line)
            except Exception as e:
                print(f"Error reading serial data: {e}")
                break
 