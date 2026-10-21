"""
AgriGantry three-axis controller for AF-434 wiring.

This script gives a command-line interface for all three gantry axes.
Features:
- Automatic Arduino detection
- Emergency stop functionality with KeyboardInterrupt handling (Input Detection too ("S"))
- Case-insensitive command processing
- Improved error handling and user feedback
- Global O emergency-stop key, active even while waiting for a move response
- Resume functionality after emergency stop ("S0" or "CLEAR")
"""

import serial
import time
import threading
import sys
import signal
import argparse
import serial.tools.list_ports
try:
    from pynput import keyboard
except (ImportError, OSError):
    keyboard = None

class FarmBotController:
    def __init__(self, port=None):
        self.arduino = None
        self.port = port
        self.emergency_stop_active = False
        self.running = True
        self.serial_lock = threading.Lock()
        self.keyboard_listener = None
        self.stop_key_down = False

    def on_key_press(self, key):
        """O is reserved for stopping, regardless of which window has focus."""
        if (getattr(key, "char", None) or "").lower() == "o" and not self.stop_key_down:
            self.stop_key_down = True
            self.emergency_stop()

    def on_key_release(self, key):
        if (getattr(key, "char", None) or "").lower() == "o":
            self.stop_key_down = False

    def start_emergency_listener(self):
        if keyboard is None:
            print("O hotkey unavailable. Install Pi/requirements.txt and run in a desktop session.")
            return False
        try:
            self.keyboard_listener = keyboard.Listener(
                on_press=self.on_key_press, on_release=self.on_key_release)
            self.keyboard_listener.start()
            self.keyboard_listener.wait()
            print("O EMERGENCY STOP active: press O (no Enter). The key is captured globally.")
            return True
        except Exception as exc:
            print(f"Could not start the O emergency-stop listener: {exc}")
            return False
        
    def find_arduino_port(self):
        """Find the Arduino port"""
        ports = serial.tools.list_ports.comports()
        for port in ports:
            if (port.vid in (0x2341, 0x2A03, 0x1A86)
                    or any(tag in (port.description or "") for tag in ("Arduino", "Mega", "CH340", "USB Serial"))
                    or "ttyACM" in port.device or "ttyUSB" in port.device):
                return port.device
        return None
    
    def open_serial_connection(self, port, baudrate=115200, timeout=2):
        """Open serial connection to Arduino"""
        try:
            connection = serial.Serial(port, baudrate, timeout=timeout, write_timeout=1,
                                       xonxoff=False, rtscts=False, dsrdtr=False)
            connection.dtr = False
            time.sleep(0.15)
            connection.reset_input_buffer()
            connection.reset_output_buffer()
            connection.dtr = True
            time.sleep(2.5)
            return connection
        except serial.SerialException as e:
            print(f"Error opening serial connection: {e}")
            return None
    
    def send_command(self, command):
        """Send command to Arduino and read response"""
        if not self.arduino:
            print("No Arduino connection available")
            return False
            
        try:
            #Send command
            with self.serial_lock:
                self.arduino.write((command + '\n').encode())
            time.sleep(0.1)  #Short delay for command processing
            
            #Read response with timeout
            start_time = time.time()
            while time.time() - start_time < 2.0:  #2 second timeout
                if self.arduino.in_waiting:
                    response = self.arduino.readline().decode().strip()
                    if response:
                        print(f"Arduino: {response}")
                        
                        #Check if emergency stop was confirmed
                        if "EMERGENCY STOP" in response:
                            self.emergency_stop_active = True
                        elif "Emergency stop cleared" in response or "operations resumed" in response:
                            self.emergency_stop_active = False
                            
                time.sleep(0.01)  #Small delay to prevent busy waiting
                
            return True
            
        except serial.SerialException as e:
            print(f"Error sending command: {e}")
            return False
        except Exception as e:
            print(f"Unexpected error: {e}")
            return False
    
    def emergency_stop(self):
        #Send emergency stop command 
        print("\n*** EMERGENCY STOP ACTIVATED ***")
        if self.arduino:
            try:
                with self.serial_lock:
                    self.arduino.write(b'O\n')
                    self.emergency_stop_active = True
                print("Emergency stop command sent to Arduino")
            except Exception as e:
                print(f"Error sending emergency stop: {e}")
    
    def signal_handler(self, signum, frame):
        """Handle Ctrl+C signal"""
        print("\nKeyboard interrupt detected!")
        self.emergency_stop()
        self.running = False
        sys.exit(0)
    
    def print_help(self):
        """Print available commands"""
        print("\n" + "-"*50)
        print("AgriGantry Controller - Available Commands:")
        print("="*50)
        print("Movement Commands:")
        print("  X####/Y####/Z####     - Move relative steps (e.g., X1000, X-500)") 
        print("  PX/PY/PZ   - Absolute Positioning of Desired Axis")
        print("  PX25       - Move to 25% position") #Absolute Positions (Later Changed depending on actual positional matrix of farm)
        print("  PX50       - Move to 50% position") 
        print("  PX75       - Move to 75% position")
        print()
        print("System Commands:")
        print("  0         - Record current positions as the manual origin")
        print("  0X/0Y/0Z  - Set the manual origin of just one axis")
        print("  No homing: this gantry has no limit switches")
        print("  R         - Report current position and status")
        print("  O         - Emergency stop ALL motors, no Enter needed")
        print("  S         - Emergency stop command (with Enter)")
        print("  S0        - Resume operations after emergency stop") #After Stop Comman was sent ("S") NOTHING SHOULD HAPPEN If S0 has not been sent
        print()
        print("Control Commands:")
        print("  help      - Show this help message") #For long-term testing (I might forget what I am doing)
        print("  exit      - Exit the program")
        print("  quit      - Exit the program")
        print()
        print("Percentage moves need measured *_TRAVEL_STEPS in Config.h and a manual 0.")
        print("O is a global hotkey reserved for stopping while this controller runs.")
        print("Note: Commands are case-insensitive")
        print("      Use Ctrl+C for immediate emergency stop")
        print("-"*50)
    
    def command_loop(self):
        try:
            self.print_help()
            
            while self.running:
                try:
                    #Show current status in prompt
                    status = "EMERGENCY" if self.emergency_stop_active else "READY"
                    command = input(f"\nFarmBot [{status}]> ").strip()
                    
                    if not command:
                        continue
                    
                    #Convert to lowercase for comparison (Commands are case sensitive)
                    cmd_lower = command.lower()
                    
                    #Handle local commands
                    if cmd_lower in ['exit', 'quit']:
                        print("Exiting FarmBot controller...")
                        break
                    elif cmd_lower in ['help', '?']:
                        self.print_help()
                        continue
                    elif cmd_lower == 'connection':
                        print(f"Connection: {'Connected' if self.arduino else 'Disconnected'}")
                        print(f"Emergency Stop: {'Active' if self.emergency_stop_active else 'Inactive'}")
                        continue
                    
                    #Send command to Arduino
                    success = self.send_command(command)
                    
                    if not success:
                        print("Failed to send command. Check connection.")
                        
                except KeyboardInterrupt:
                    print("\nKeyboard interrupt detected!")
                    self.emergency_stop()
                    
                    #Ask if user wants to continue
                    try:
                        response = input("\nContinue? (y/n): ").strip().lower()
                        if response in ['n', 'no']:
                            break
                    except KeyboardInterrupt:
                        break
                        
                except EOFError:
                    print("\nEOF detected. Exiting...")
                    break
                    
        except Exception as e:
            print(f"Error in command loop: {e}")
        finally:
            self.cleanup()
    
    def cleanup(self):
        """Clean up resources"""
        self.running = False
        if self.keyboard_listener is not None:
            self.keyboard_listener.stop()
            self.keyboard_listener = None
        if self.arduino:
            try:
                self.emergency_stop()
                with self.serial_lock:
                    self.arduino.close()
                    self.arduino = None
                print("Arduino connection closed.")
            except Exception as e:
                print(f"Error closing Arduino connection: {e}")
    
    def run(self):
        """MAIN RUN FUNCTION"""
        #Set up signal handler for Ctrl+C
        signal.signal(signal.SIGINT, self.signal_handler)
        
        print("AgriGantry Controller Starting...")
        print("Searching for Arduino...")
        
        #Find Arduino port
        port = self.port or self.find_arduino_port()
        if not port:
            print("Arduino not found!")
            print("\nAvailable ports:")
            ports = serial.tools.list_ports.comports()
            if ports:
                for p in ports:
                    print(f"  - {p.device} ({p.description})")
            else:
                print("  No serial ports found")
            return False
        
        print(f"Arduino found on port: {port}")
        
        #Open connection
        self.arduino = self.open_serial_connection(port)
        if not self.arduino:
            print("Failed to connect to Arduino")
            return False

        if not self.start_emergency_listener():
            self.cleanup()
            return False
        
        print(f"Connected to Arduino on {port}")
        print("Waiting for Arduino to initialize...") #End of Main run print, Available Commands should be shown in Terminal 
        time.sleep(2)
        
        #Start command loop
        self.command_loop()
        
        return True

def main():
    """MAIN ENTRY"""
    parser = argparse.ArgumentParser(description="AgriGantry controller with O emergency-stop hotkey")
    parser.add_argument("--port", help="Serial port, for example COM9 or /dev/ttyACM0")
    args = parser.parse_args()
    controller = FarmBotController(port=args.port)
    
    try:
        success = controller.run()
        if not success:
            sys.exit(1)
    except KeyboardInterrupt:
        print("\nProgram interrupted by user")
        controller.emergency_stop()
    except Exception as e:
        print(f"Unexpected error: {e}")
        sys.exit(1)
    finally:
        controller.cleanup()

if __name__ == "__main__":
    main()
