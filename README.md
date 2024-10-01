# autonomous_drone
Under Development

Line Following Autonomous drone utilzing an ESP32 and a laptop for image processing. The ESP32 integrates a camera along with ultrasonic sensors for object detection/avoidance.The video stream is sent over a webserver and then accessed by a laptop or GCS for image processing. The relevant control commands are developed on the GCS based on the image processing and then transmitted over the webserver back to the ESP32. In addition, a joystick is added to the GCS code for manual control which will override the autonomous control of the drone. The code is set up to follow a white line on a dark/black background.

The ESP32 then communicates over SBUS to the Betaflight Flight Controlller.

![image](https://github.com/user-attachments/assets/b6a2b589-14a9-49c7-b56e-308d0a296d5a)

