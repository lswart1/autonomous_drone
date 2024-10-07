import cv2
import numpy as np
import urllib.request

# URL of the ESP32 camera stream
url = 'http://192.168.137.184:81/stream'  # Replace with your ESP32 IP and stream path

# Open the stream using urllib
stream = urllib.request.urlopen(url)

# Buffer to hold chunks of the stream
bytes_data = b''

while True:
    # Read a chunk from the stream
    bytes_data += stream.read(1024)

    # Look for the JPEG frame start and end markers
    start = bytes_data.find(b'\xff\xd8')  # Start of JPEG frame
    end = bytes_data.find(b'\xff\xd9')    # End of JPEG frame

    if start != -1 and end != -1:
        # Extract the full JPEG frame
        jpg = bytes_data[start:end+2]
        bytes_data = bytes_data[end+2:]

        # Convert the JPEG frame to a NumPy array
        img_array = np.frombuffer(jpg, dtype=np.uint8)

        # Decode the image array into an OpenCV image
        frame = cv2.imdecode(img_array, cv2.IMREAD_COLOR)

        # Proceed with your line-following logic (gray conversion, thresholding, etc.)
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

        # Threshold the image to create a binary image where white is the line
        _, binary = cv2.threshold(gray, 200, 255, cv2.THRESH_BINARY)

        # Find contours of the white line
        contours, _ = cv2.findContours(binary, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

        if contours:
            # Assume the largest contour is the line to follow
            line_contour = max(contours, key=cv2.contourArea)

            # Calculate the moments of the contour to find the centroid
            M = cv2.moments(line_contour)
            if M["m00"] > 0:
                cx = int(M["m10"] / M["m00"])  # Centroid x-coordinate
                cy = int(M["m01"] / M["m00"])  # Centroid y-coordinate

                # Draw the contour and centroid on the frame for visualization
                cv2.drawContours(frame, [line_contour], -1, (0, 255, 0), 2)
                cv2.circle(frame, (cx, cy), 5, (255, 0, 0), -1)

                # Determine the direction the robot needs to go
                frame_center = frame.shape[1] // 2

                # Visualization of the direction
                if cx < frame_center - 50:  # Line is on the left side
                    direction_text = "Move Left"
                    cv2.arrowedLine(frame, (frame_center, cy), (cx, cy), (0, 0, 255), 5)
                elif cx > frame_center + 50:  # Line is on the right side
                    direction_text = "Move Right"
                    cv2.arrowedLine(frame, (frame_center, cy), (cx, cy), (0, 0, 255), 5)
                else:  # Line is centered
                    direction_text = "Move Forward"
                    cv2.arrowedLine(frame, (frame_center, cy), (frame_center, cy - 50), (0, 255, 0), 5)

                # Display the direction on the frame
                cv2.putText(frame, direction_text, (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (255, 255, 255), 2)

        # Display the frame with the line following visualization
        cv2.imshow('ESP32 Camera - Line Following', frame)

        # Break loop on 'q' key press
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

# Release the stream and close windows
cv2.destroyAllWindows()
