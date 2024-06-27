

const int FLEX_PINS[] = {A7, A2, A3, A4, A9}; // Pins connected to voltage divider outputs
const int NUM_SENSORS = 5; // Number of sensors

// Measure the voltage at 3.3V and the actual resistance of your
// 10k resistor, and enter them below:
const float VCC = 3.3; // Measured voltage of Arduino 3.3V line
const float R_DIV = 10000.0; // Measured resistance of 10k resistor

// Upload the code, then try to adjust these values to more
// accurately calculate bend degree.
const float STRAIGHT_RESISTANCE = 10300.0; // resistance when straight
const float BEND_RESISTANCE = 90000.0; // resistance at 90 deg

void setup() 
{
  Serial.begin(9600);
  for (int i = 0; i < NUM_SENSORS; i++) {
    pinMode(FLEX_PINS[i], INPUT);
  }
}

void loop() 
{
  String dataString = ""; // Initialize an empty string for plotting

  for (int i = 0; i < NUM_SENSORS; i++) {
    // Read the ADC, and calculate voltage and resistance from it
    int flexADC = analogRead(FLEX_PINS[i]);
    float flexV = flexADC * VCC / 1023.0;
    float flexR = R_DIV * (VCC / flexV - 1.0);

    // Append the resistance value to the data string
    dataString += String(flexR);
    if (i < NUM_SENSORS - 1) {
      dataString += "\t"; // Add a tab separator between values
    }
  }

  Serial.println(dataString); // Print the data string for plotting

  delay(50);
}
