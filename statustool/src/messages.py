from enum import Enum

class MessageType(Enum):
    DOOR = "DOOR"
    AWAKE = "AWAKE"
    PIN = "PIN"
    SENSE = "SENSE"

           
def parse_message(message):
    """Parse a message and return a dictionary of key-value pairs."""
    
    if message == "OK.":
        return {
            "type": "OK"
        }
    
    
    if "=" not in message:
        return None
    
    

    key, value = message.split("=", 1)


    if key == "DOOR":
        return parse_door_message(value)
    elif key == "AWAKE":
        return parse_awake_message(value)
    elif key == "PIN":
        return pass_value_message(key,value)
    elif key == "SENSE":
        return pass_value_message(key,value)

    return None

def parse_door_message(value):
    if len(value) != 4:
        return {"type": "DOOR", "error": "Invalid payload length"}
    return {
        "type": "DOOR",
        "locked": "Locked" if value[0] == '1' else "Unlocked",
        "closed": "Closed" if value[1] == '1' else "Open",
        "motor_state": {
            '0': "stopped",
            '1': "closing",
            '2': "opening"
        }.get(value[2], "Unknown"),
        "motor_action": "Success" if value[3] == '1' else "Failed"
    }

def parse_awake_message(value):
    return {
        "type": "AWAKE",
        "awake": "Running" if value == '1' else "Sleep"
    }
def pass_value_message(key,value):
    return {
        "type": key,
        "value": value
    }

