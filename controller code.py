import pygame
import requests

# Initialize Pygame and the joystick
pygame.init()
pygame.joystick.init()

# Check for joystick
if pygame.joystick.get_count() == 0:
    print("No joystick found!")
    exit()

# Set up joystick
joystick = pygame.joystick.Joystick(0)
joystick.init()

# Initial arm state
arm_state = 1500
# Initial throttle value
throttle = 1000  # Starting throttle value
# Threshold for joystick movement
throttle_increment_threshold = 0.5  # Adjust as needed
previous_throttle_y = 0  # To track the previous state of the joystick

# ESP32 server URL
ESP32_URL = "http://192.168.0.190/action"

def send_values(roll, pitch, yaw, throttle, arm):
    params = {
        'roll': roll,
        'pitch': pitch,
        'yaw': yaw,
        'throttle': throttle,
        'arm': arm
    }
    try:
        response = requests.get(ESP32_URL, params=params)
        print(f"Sent values: {params} | Response: {response.status_code}")
    except Exception as e:
        print(f"Error sending values: {e}")

try:
    while True:
        pygame.event.pump()

        # Read joystick axes
        left_stick_x = joystick.get_axis(0)  # 
        left_stick_y = joystick.get_axis(1)  # Y-axis for throttle
        right_stick_x = joystick.get_axis(2)  # 
        right_stick_y = joystick.get_axis(3)  # 

        # Map joystick values to required ranges
        roll = int((right_stick_x) * 15 + 1500)  # Roll: 1000 to 2000
        pitch = int((right_stick_y) * 15 + 1500)  # Pitch: 1000 to 2000
        yaw = int((left_stick_x) * 15 + 1500)  # Yaw: 1000 to 2000

        # Check if the left joystick is pushed upwards past the threshold
        if left_stick_y < -throttle_increment_threshold:
            # Increment throttle by 30, ensuring it doesn't exceed 2000
            throttle = min(throttle + 30, 2000)
        elif left_stick_y > throttle_increment_threshold:
            throttle = max(throttle - 30, 990)

        # Send the values to the ESP32
        send_values(roll, pitch, yaw, throttle, arm_state)

        # Check for button press (Button X is usually button 0) for arm state
        if joystick.get_button(0):  # Button X
            arm_state = 1800 if arm_state == 1500 else 1500

        if joystick.get_button(2):  # Button O
            throttle = 1000

        pygame.time.delay(100)  # Delay to limit the rate of sending

except KeyboardInterrupt:
    print("Exiting...")
finally:
    pygame.quit()
