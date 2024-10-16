import pygame
import requests
import cv2
import numpy as np
import urllib.request
import time

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

# Initial arm state and throttle value
arm_state = 1500
throttle = 1000  # Starting throttle value
autonomous_mode = False  # Flag for autonomous control
throttle_increment_threshold = 0.5  # Threshold for joystick movement

# ESP32 server URL for control commands
ESP32_URL = "http://192.168.137.53/action"

# URL of the ESP32 camera stream for line-following
camera_url = 'http://192.168.137.53:81/stream'  # Replace with your ESP32 stream path
stream = urllib.request.urlopen(camera_url)
bytes_data = b''  # Buffer to hold chunks of the stream

# Timer for sending control values (10 times per second)
last_sent_time = time.time()
send_interval = 0.02  # 100 ms (0.1 seconds)

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

        current_time = time.time()
        if current_time - last_sent_time >= send_interval:
            if not autonomous_mode:
                # Manual control with joystick
                left_stick_x = joystick.get_axis(0)  # Yaw
                left_stick_y = joystick.get_axis(1)  # Throttle
                right_stick_x = joystick.get_axis(2)  # Roll
                right_stick_y = joystick.get_axis(3)  # Pitch

                roll = int((right_stick_x) * 15 + 1500)  # Roll: 1000 to 2000
                pitch = int((right_stick_y) * 15 + 1500)  # Pitch: 1000 to 2000
                yaw = int((left_stick_x) * 15 + 1500)  # Yaw: 1000 to 2000

                if left_stick_y < -throttle_increment_threshold:
                    throttle = min(throttle + 30, 2000)
                elif left_stick_y > throttle_increment_threshold:
                    throttle = max(throttle - 30, 990)

                # Send joystick control values to ESP32
                send_values(roll, pitch, yaw, throttle, arm_state)

                # Check for arm/disarm
                if joystick.get_button(0):  # Arm/disarm button
                    arm_state = 1800 if arm_state == 1500 else 1500

                # Switch to autonomous control when button 0 (O) is pressed
                if joystick.get_button(1):
                    autonomous_mode = True

            else:
                # Autonomous control using image processing for line-following
                bytes_data += stream.read(1024)

                start = bytes_data.find(b'\xff\xd8')  # Start of JPEG frame
                end = bytes_data.find(b'\xff\xd9')    # End of JPEG frame

                if start != -1 and end != -1:
                    jpg = bytes_data[start:end+2]
                    bytes_data = bytes_data[end+2:]

                    img_array = np.frombuffer(jpg, dtype=np.uint8)
                    frame = cv2.imdecode(img_array, cv2.IMREAD_COLOR)

                    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
                    _, binary = cv2.threshold(gray, 200, 255, cv2.THRESH_BINARY)

                    contours, _ = cv2.findContours(binary, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

                    if contours:
                        line_contour = max(contours, key=cv2.contourArea)
                        M = cv2.moments(line_contour)
                        if M["m00"] > 0:
                            cx = int(M["m10"] / M["m00"])
                            cy = int(M["m01"] / M["m00"])

                            frame_center = frame.shape[1] // 2
                            roll = 1500  # Keep roll neutral
                            pitch = 1500  # Keep pitch neutral

                            if cx < frame_center - 50:  # Line is left
                                yaw = 1300  # Yaw left
                                throttle = 1600  # Slight throttle
                            elif cx > frame_center + 50:  # Line is right
                                yaw = 1700  # Yaw right
                                throttle = 1600  # Slight throttle
                            else:
                                yaw = 1500  # Stay centered
                                throttle = 1600  # Move forward

                            send_values(roll, pitch, yaw, throttle, arm_state)

                            cv2.drawContours(frame, [line_contour], -1, (0, 255, 0), 2)
                            cv2.circle(frame, (cx, cy), 5, (255, 0, 0), -1)

                            direction_text = "Move Left" if cx < frame_center - 50 else "Move Right" if cx > frame_center + 50 else "Move Forward"
                            cv2.putText(frame, direction_text, (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (255, 255, 255), 2)

                    cv2.imshow('ESP32 Camera - Line Following', frame)

                    if cv2.waitKey(1) & 0xFF == ord('q'):
                        break

                # Switch back to manual control if button 0 is pressed again
                if joystick.get_button(1):
                    autonomous_mode = False

            # Update the time of the last send
            last_sent_time = current_time

except KeyboardInterrupt:
    print("Exiting...")
finally:
    pygame.quit()
    cv2.destroyAllWindows()
