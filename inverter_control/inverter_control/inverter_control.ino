/// ADC read of voltage 
const int ADC_PIN = 9;
const int n_avg = 1000;
const int wait = 1;

/// Output
const int OUT_PIN = 4;
const float ON_mV = 2150.0;
const float OFF_mv = 2100.0; 

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Com test");

  pinMode(OUT_PIN, OUTPUT);
}

bool isON = false;
void loop() {
  while (!isON){
    if (readAvg() > ON_mV){
      digitalWrite(OUT_PIN, HIGH);
      Serial.println("Turning ON");
      isON = true;
    }
  }

  while (isON){
    if (readAvg() < OFF_mv){
      digitalWrite(OUT_PIN, LOW);
      Serial.println("Turning OFF");
      isON = false;
    }
  } 

  delay(10000);
}

float readAvg() {
  uint32_t sum = 0;
  for (int i=0; i<n_avg; i++){
    sum += analogReadMilliVolts(ADC_PIN);
    delay(wait);
    }
  float avg = sum / n_avg;
  Serial.printf("ADC: %f mV\n", avg);
  return avg;
}
