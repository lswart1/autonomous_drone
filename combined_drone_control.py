
import pygame
import requests
import cv2
import numpy as np

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

# ESP32 server URL
ESP32_URL = "http://<ESP32_IP>/action"

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

# Function to process video frame and return control commands (roll, pitch, yaw, throttle)
def process_image_frame(frame):
    # Convert frame to grayscale
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    
    # Apply threshold to detect line
    _, binary = cv2.threshold(gray, 150, 255, cv2.THRESH_BINARY_INV)
    
    # Find contours
    contours, _ = cv2.findContours(binary, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    
    if contours:
        # Assume the largest contour is the line to follow
        line_contour = max(contours, key=cv2.contourArea)
        
        # Calculate the moments of the contour to find the centroid
        M = cv2.moments(line_contour)
        if M["m00"] > 0:
            cx = int(M["m10"] / M["m00"])  # Centroid x-coordinate
            cy = int(M["m01"] / M["m00"])  # Centroid y-coordinate

            # Determine the direction based on centroid position
            frame_center = frame.shape[1] // 2

            roll = 1500  # Keep drone level
            pitch = 1500  # Keep drone steady
            
            if cx < frame_center - 50:  # Line is on the left side
                yaw = 1300  # Yaw left
                throttle = 1600  # Slight throttle up for movement
            elif cx > frame_center + 50:  # Line is on the right side
                yaw = 1700  # Yaw right
                throttle = 1600  # Slight throttle up
            else:  # Line is centered
                yaw = 1500  # Stay centered
                throttle = 1600  # Move forward

            return roll, pitch, yaw, throttle

    return None, None, None, None  # No valid command from image processing

try:
    use_image_control = False  # Start with joystick control

    # Set up video capture for image-based control (from ESP32-CAM or another camera)
    cap = cv2.VideoCapture('http://<ESP32_CAM_IP>/video')

    while True:
        pygame.event.pump()

        # Read joystick axes (for manual control)
        left_stick_x = joystick.get_axis(0)  # X-axis for roll
        left_stick_y = joystick.get_axis(1)  # Y-axis for pitch
        right_stick_x = joystick.get_axis(2)  # X-axis for yaw
        right_stick_y = joystick.get_axis(3)  # Y-axis for throttle (inverted)

        # Map joystick values to required ranges
        roll = int((left_stick_x + 1) * 500 + 1500)  # Roll: 1000 to 2000
        pitch = int((left_stick_y + 1) * 500 + 1500)  # Pitch: 1000 to 2000
        yaw = int((right_stick_x + 1) * 500 + 1500)  # Yaw: 1000 to 2000
        throttle = int((1 - (right_stick_y + 1) / 2) * 1000 + 1000)  # Throttle: 1000 to 2000

        # Check for button press (Button X is usually button 0)
        if joystick.get_button(0):  # Button X
            use_image_control = not use_image_control  # Toggle between joystick and image control

        if use_image_control:
            # Image-based control
            ret, frame = cap.read()
            if ret:
                img_roll, img_pitch, img_yaw, img_throttle = process_image_frame(frame)
                if img_roll is not None:
                    roll, pitch, yaw, throttle = img_roll, img_pitch, img_yaw, img_throttle
                cv2.imshow('ESP32 Camera - Line Following', frame)

        # Send the values to the ESP32
        send_values(roll, pitch, yaw, throttle, arm_state)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break  # Exit on 'q' key press

        pygame.time.delay(100)  # Delay to limit the rate of sending

except KeyboardInterrupt:
    print("Exiting...")

finally:
    pygame.quit()
    cap.release()
    cv2.destroyAllWindows()
