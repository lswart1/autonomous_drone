// Define HC-SR04 sensor pins
const int trigPin = 2; // Trigger pin connected to GPIO 14
const int echoPin = 15; // Echo pin connected to GPIO 12

void setup() {
  // Initialize the serial communication
  Serial.begin(115200);

  // Set pin modes for the HC-SR04 sensor
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
}

void loop() {

  // Send a 10us pulse to trigger the sensor
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Read the echo pin to measure the duration
  long duration = pulseIn(echoPin, HIGH);

  // Calculate the distance in centimeters
  float distance = (duration * 0.034) / 2;

  // Print the distance to the Serial Monitor
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  // Small delay before the next loop
  delay(500);
}
