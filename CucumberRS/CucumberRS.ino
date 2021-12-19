#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_HTS221.h>
#include <Adafruit_NeoPixel.h>
#include <ESPAsync_WiFiManager.h>
#include <PubSubClient.h>
#include <Natthaporn-project-1_inferencing.h>

#define CONVERT_G_TO_MS2    9.80665f
#define FREQUENCY_HZ        EI_CLASSIFIER_FREQUENCY
#define INTERVAL_MS         (1000 / (FREQUENCY_HZ + 1))

// constants
#define I2C_SDA     41
#define I2C_SCL     40
#define LED_PIN     2
#define RGBLED_PIN  18

// persistence variables
Adafruit_BMP280 bmp;
Adafruit_HTS221 hts;
Adafruit_MPU6050 mpu;
Adafruit_NeoPixel pixels(1, RGBLED_PIN, NEO_GRB + NEO_KHZ800);

static unsigned long last_interval_ms = 0;
// to classify 1 frame of data you need EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE values
float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE]; //114
// keep track of where we are in the feature array
size_t feature_ix = 0;

const char SENSOR_TOPIC[] = "cn466/natthaporn/backache";
const char LED_TOPIC[] = "cn466/natthaporn_led/backache";

AsyncWebServer webServer(80);
WiFiClient esp32Client;
DNSServer dnsServer;
PubSubClient mqttClient(esp32Client);
//IPAddress netpieBroker(35, 186, 155, 39);

// setup sensors and LED
void setupHardware() {
  Wire.begin(I2C_SDA, I2C_SCL, 100000);
  if (bmp.begin(0x76)) {    // prepare BMP280 sensor
    Serial.println("BMP280 sensor ready");
  }
  if (hts.begin_I2C()) {    // prepare HTS221 sensor
    Serial.println("HTS221 sensor ready");
  }
  if (mpu.begin()) {       // prepare MPU6050 sensor
    Serial.println("MPU6050 sensor ready");
  }
  pinMode(LED_PIN, OUTPUT);      // prepare LED
  digitalWrite(LED_PIN, HIGH);
  pixels.begin();                // prepare NeoPixel LED
  pixels.clear();
}

// message reception callback
void callback(char* topic, byte* payload, unsigned int length) {
  char cmd[100];
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0; i<length; i++) {
    cmd[i] = (char)payload[i];
  }
  cmd[length] = 0;
  Serial.println(cmd);
  if (strcmp(cmd, "off") == 0) {
    pixels.setPixelColor(0, pixels.Color(0, 0, 0));
    pixels.show();
  }
  if (strcmp(cmd, "red") == 0) {
    pixels.setPixelColor(0, pixels.Color(25, 0, 0));
    pixels.show();
  }
  if (strcmp(cmd, "green") == 0) {
    pixels.setPixelColor(0, pixels.Color(0, 25, 0));
    pixels.show();
  }
  if (strcmp(cmd, "blue") == 0) {
    pixels.setPixelColor(0, pixels.Color(0, 0, 25));
    pixels.show();
  }
  if (strcmp(cmd, "white") == 0) {
    pixels.setPixelColor(0, pixels.Color(25, 25, 25));
    pixels.show();
  }
}

void setupNetwork() {
  // WiFi
  WiFi.begin("Keng-2.4G", "0865919598");
  //WiFi.begin("aisfibre_2.4G_F4C554", "A3F4C554");
  Serial.print("connecting");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
 }

  // HiveMQ
  mqttClient.setServer("broker.hivemq.com", 1883);
  mqttClient.setCallback(callback);
}

// connect MQTT broker
void connectBroker() {
  char client_id[20];

  sprintf(client_id, "cucumber-%d", esp_random()%10000);
  if (mqttClient.connect(client_id)) {
    mqttClient.subscribe(LED_TOPIC);
  }
}

// initialization
void setup() {
  Serial.begin(115200);
  setupHardware();
  setupNetwork();
  connectBroker();
  Serial.println("Starting");
}

int feature_i = 0;
const char *old_status = "";
const char *new_status = "";
float max_value;

void loop() {
  static uint32_t prev_millis = 0;
  char json_body[200];
  const char json_tmpl[] = "{\"acceleration\":  [%.2f,%.2f,%.2f], \"newStatus\": \"%s\"}";
  sensors_event_t temp, humid;
  sensors_event_t a, g;

  if (millis() - prev_millis > 100) {
    prev_millis = millis();
    float p = bmp.readPressure();
    hts.getEvent(&humid, &temp);
    float t = temp.temperature;
    float h = humid.relative_humidity;
    mpu.getEvent(&a, &g, &temp);
    float ax = a.acceleration.x;
    float ay = a.acceleration.y;
    float az = a.acceleration.z;
  
    //mqttClient.publish(SENSOR_TOPIC, json_body);
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));

       // fill the features buffer
        features[feature_i] = ax;
        features[feature_i+1] = ay;
        features[feature_i+2] = az;
        feature_i = feature_i + 3;

        // features buffer full? then classify!
        if (feature_i == EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
            ei_impulse_result_t result;

            // create signal from features frame
            signal_t signal;
            numpy::signal_from_buffer(features, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);

            // run classifier
            EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);
            ei_printf("run_classifier returned: %d\n", res);
            if (res != 0) return;

            // print predictions
            ei_printf("Predictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.): \n",
                result.timing.dsp, result.timing.classification, result.timing.anomaly);

            // print the predictions
            max_value = result.classification[0].value;
            for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
                ei_printf("%s:\t%.5f\n", result.classification[ix].label, result.classification[ix].value);
                if(max_value < result.classification[ix].value && ix != 0){
                   max_value = result.classification[ix].value;
                   new_status = result.classification[ix].label;
                }
            }
            
            if(strlen(old_status) == 0){
              old_status = new_status;
               if(strlen(old_status) != 0){
                  sprintf(json_body, json_tmpl, ax, ay, az, old_status);
                  Serial.println(json_body);
                  mqttClient.publish(SENSOR_TOPIC, json_body);
                  printf("first status => %s\n", old_status);
                }
            }
            else{
               if(new_status != old_status){
                old_status = new_status;
                sprintf(json_body, json_tmpl, ax, ay, az, old_status);
                Serial.println(json_body);
                mqttClient.publish(SENSOR_TOPIC, json_body);
                printf("status change => %s\n", old_status);
               }
            }

            
        #if EI_CLASSIFIER_HAS_ANOMALY == 1
            ei_printf("anomaly:\t%.3f\n", result.anomaly);
        #endif
            // reset features frame
            feature_i = 0;
        }
  }
  if (!mqttClient.connected()) {
    connectBroker();
  }
  mqttClient.loop();
  delay(100);
}

void ei_printf(const char *format, ...) {
    static char print_buf[1024] = { 0 };

    va_list args;
    va_start(args, format);
    int r = vsnprintf(print_buf, sizeof(print_buf), format, args);
    va_end(args);

    if (r > 0) {
        Serial.write(print_buf);
    }
}
