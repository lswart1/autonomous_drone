#include "esp_camera.h"
#include <WiFi.h>
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "fb_gfx.h"
#include "soc/soc.h"             // disable brownout problems
#include "soc/rtc_cntl_reg.h"    // disable brownout problems
#include "esp_http_server.h"

// Replace with your network credentials
const char* ssid = "Drone_Server";
const char* password = "Drone123";
int roll_value = 1500;
int pitch_value = 1500;
int yaw_value = 1500;
int throttle_value = 990;
int arm_value = 1500;


#define RC_CHANNEL_MIN 990
#define RC_CHANNEL_MAX 2010

#define SBUS_MIN_OFFSET 173
#define SBUS_MID_OFFSET 992
#define SBUS_MAX_OFFSET 1811
#define SBUS_CHANNEL_NUMBER 16
#define SBUS_PACKET_LENGTH 25
#define SBUS_FRAME_HEADER 0x0f
#define SBUS_FRAME_FOOTER 0x00
#define SBUS_FRAME_FOOTER_V2 0x04
#define SBUS_STATE_FAILSAFE 0x08
#define SBUS_STATE_SIGNALLOSS 0x04
#define SBUS_UPDATE_RATE 15 //ms
#define SBUS_BAUD_RATE 100000
#define PART_BOUNDARY "123456789000000000000987654321"
#define CAMERA_MODEL_WROVER_KIT
  #define PWDN_GPIO_NUM    -1
  #define RESET_GPIO_NUM   -1
  #define XCLK_GPIO_NUM    21
  #define SIOD_GPIO_NUM    26
  #define SIOC_GPIO_NUM    27
  
  #define Y9_GPIO_NUM      35
  #define Y8_GPIO_NUM      34
  #define Y7_GPIO_NUM      39
  #define Y6_GPIO_NUM      36
  #define Y5_GPIO_NUM      19
  #define Y4_GPIO_NUM      18
  #define Y3_GPIO_NUM       5
  #define Y2_GPIO_NUM       4
  #define VSYNC_GPIO_NUM   25
  #define HREF_GPIO_NUM    23
  #define PCLK_GPIO_NUM    22

int rcChannels[SBUS_CHANNEL_NUMBER] = {1500}; // Initialize all channels to mid-value
uint8_t sbusPacket[SBUS_PACKET_LENGTH];
uint32_t sbusTime = 0;


static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";
httpd_handle_t camera_httpd = NULL;
httpd_handle_t stream_httpd = NULL;
int pitch = 1500;
int yaw = 1500;
int throttle = 990;
int tilt = 1500;
int arm = 1500;


void sbusPreparePacket(uint8_t packet[], int channels[], bool isSignalLoss, bool isFailsafe) {
    static int output[SBUS_CHANNEL_NUMBER] = {0};

    for (uint8_t i = 0; i < SBUS_CHANNEL_NUMBER; i++) {
        output[i] = map(channels[i], RC_CHANNEL_MIN, RC_CHANNEL_MAX, SBUS_MIN_OFFSET, SBUS_MAX_OFFSET);
    }

    uint8_t stateByte = 0x00;
    if (isSignalLoss) {
        stateByte |= SBUS_STATE_SIGNALLOSS;
    }
    if (isFailsafe) {
        stateByte |= SBUS_STATE_FAILSAFE;
    }

    packet[0] = SBUS_FRAME_HEADER; // Header
    packet[1] = (uint8_t) (output[0] & 0x07FF);
    packet[2] = (uint8_t) ((output[0] & 0x07FF) >> 8 | (output[1] & 0x07FF) << 3);
    packet[3] = (uint8_t) ((output[1] & 0x07FF) >> 5 | (output[2] & 0x07FF) << 6);
    packet[4] = (uint8_t) ((output[2] & 0x07FF) >> 2);
    packet[5] = (uint8_t) ((output[2] & 0x07FF) >> 10 | (output[3] & 0x07FF) << 1);
    packet[6] = (uint8_t) ((output[3] & 0x07FF) >> 7 | (output[4] & 0x07FF) << 4);
    packet[7] = (uint8_t) ((output[4] & 0x07FF) >> 4 | (output[5] & 0x07FF) << 7);
    packet[8] = (uint8_t) ((output[5] & 0x07FF) >> 1);
    packet[9] = (uint8_t) ((output[5] & 0x07FF) >> 9 | (output[6] & 0x07FF) << 2);
    packet[10] = (uint8_t) ((output[6] & 0x07FF) >> 6 | (output[7] & 0x07FF) << 5);
    packet[11] = (uint8_t) ((output[7] & 0x07FF) >> 3);
    packet[12] = (uint8_t) ((output[8] & 0x07FF));
    packet[13] = (uint8_t) ((output[8] & 0x07FF) >> 8 | (output[9] & 0x07FF) << 3);
    packet[14] = (uint8_t) ((output[9] & 0x07FF) >> 5 | (output[10] & 0x07FF) << 6);  
    packet[15] = (uint8_t) ((output[10] & 0x07FF) >> 2);
    packet[16] = (uint8_t) ((output[10] & 0x07FF) >> 10 | (output[11] & 0x07FF) << 1);
    packet[17] = (uint8_t) ((output[11] & 0x07FF) >> 7 | (output[12] & 0x07FF) << 4);
    packet[18] = (uint8_t) ((output[12] & 0x07FF) >> 4 | (output[13] & 0x07FF) << 7);
    packet[19] = (uint8_t) ((output[13] & 0x07FF) >> 1);
    packet[20] = (uint8_t) ((output[13] & 0x07FF) >> 9 | (output[14] & 0x07FF) << 2);
    packet[21] = (uint8_t) ((output[14] & 0x07FF) >> 6 | (output[15] & 0x07FF) << 5);
    packet[22] = (uint8_t) ((output[15] & 0x07FF) >> 3);
    packet[23] = stateByte; // Flags byte
    packet[24] = SBUS_FRAME_FOOTER; // Footer
}

static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<html>
  <head>
    <title>ESP32-CAM Drone Control</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
      body { font-family: Arial; text-align: center; margin:0px auto; padding-top: 30px;}
      table { margin-left: auto; margin-right: auto; }
      .button {
        background-color: #2f4468;
        border: none;
        color: white;
        padding: 10px 20px;
        font-size: 18px;
        margin: 6px 3px;
        cursor: pointer;
      }
    </style>
  </head>
  <body>
    <h1>ESP32-CAM Drone Control</h1>
    <table>
      <tr>
        <td>Roll: <input type="number" id="roll" min="990" max="2000" value="1500" onchange="sendValues()"></td>
      </tr>
      <tr>
        <td>Pitch: <input type="number" id="pitch" min="990" max="2000" value="1500" onchange="sendValues()"></td>
      </tr>
      <tr>
        <td>Yaw: <input type="number" id="yaw" min="990" max="2000" value="1500" onchange="sendValues()"></td>
      </tr>
      <tr>
        <td>Throttle: <input type="number" id="throttle" min="990" max="2000" value="990" onchange="sendValues()"></td>
      </tr>
      <tr>
        <td>Arm: <input type="number" id="arm" min="990" max="2000" value="1500" onchange="sendValues()"></td>
      </tr>
    </table>
   <script>
   function sendValues() {
     var roll = document.getElementById("roll").value;
     var pitch = document.getElementById("pitch").value;
     var yaw = document.getElementById("yaw").value;
     var throttle = document.getElementById("throttle").value;
     var arm = document.getElementById("arm").value;
     var xhr = new XMLHttpRequest();
     xhr.open("GET", "/action?roll=" + roll + "&pitch=" + pitch + "&yaw=" + yaw + "&throttle=" + throttle +"&arm=" + arm, true);
     xhr.send();
   }
   </script>
  </body>
</html>
)rawliteral";



static esp_err_t index_handler(httpd_req_t *req){
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, (const char *)INDEX_HTML, strlen(INDEX_HTML));
}

static esp_err_t stream_handler(httpd_req_t *req){
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char * part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if(res != ESP_OK){
    return res;
  }

  while(true){
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    } else {
      if(fb->width > 400){
        if(fb->format != PIXFORMAT_JPEG){
          bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
          esp_camera_fb_return(fb);
          fb = NULL;
          if(!jpeg_converted){
            Serial.println("JPEG compression failed");
            res = ESP_FAIL;
          }
        } else {
          _jpg_buf_len = fb->len;
          _jpg_buf = fb->buf;
        }
      }
    }
    if(res == ESP_OK){
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if(fb){
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if(_jpg_buf){
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if(res != ESP_OK){
      break;
    }
    //Serial.printf("MJPG: %uB\n",(uint32_t)(_jpg_buf_len));
  }
  return res;
}

static esp_err_t cmd_handler(httpd_req_t *req){
  char*  buf;
  size_t buf_len;
  
  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = (char*)malloc(buf_len);
    if(!buf){
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      char roll[32] = {0,};
      char pitch[32] = {0,};
      char yaw[32] = {0,};
      char throttle[32] = {0,};
      char arm[32] = {0,};

      // Parse each parameter
      httpd_query_key_value(buf, "roll", roll, sizeof(roll));
      httpd_query_key_value(buf, "pitch", pitch, sizeof(pitch));
      httpd_query_key_value(buf, "yaw", yaw, sizeof(yaw));
      httpd_query_key_value(buf, "throttle", throttle, sizeof(throttle));
      httpd_query_key_value(buf, "arm", arm, sizeof(arm));

      // Convert strings to integers and clamp them
      roll_value = constrain(atoi(roll), 990, 2000);
      pitch_value = constrain(atoi(pitch), 990, 2000);
      yaw_value = constrain(atoi(yaw), 990, 2000);
      throttle_value = constrain(atoi(throttle), 990, 2000);
      arm_value = constrain(atoi(arm), 990, 2000);

      Serial.printf("Roll: %d, Pitch: %d, Yaw: %d, Throttle: %d, Arm: %d\n", roll_value, pitch_value, yaw_value, throttle_value, arm_value);
    } else {
      free(buf);
      httpd_resp_send_404(req);
      return ESP_FAIL;
    }
    free(buf);
  } else {
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, NULL, 0);
}


void startCameraServer(){
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t cmd_uri = {
    .uri       = "/action",
    .method    = HTTP_GET,
    .handler   = cmd_handler,
    .user_ctx  = NULL
  };
  httpd_uri_t stream_uri = {
    .uri       = "/stream",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };
  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &cmd_uri);
  }
  config.server_port += 1;
  config.ctrl_port += 1;
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG; 
  
  if(psramFound()){
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  // Wi-Fi connection
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  
  Serial.print("Camera Stream Ready! Go to: http://");
  Serial.println(WiFi.localIP());
  
  // Start streaming web server
  startCameraServer();
  Serial1.begin(SBUS_BAUD_RATE, SERIAL_8E2, -1, 1); // Initialize Serial1 with 100000 baud rate, TX = GPIO 9
    
    for (uint8_t i = 0; i < SBUS_CHANNEL_NUMBER; i++) {
        rcChannels[i] = 1500; // Initialize channels to mid-value
        
    }
}

void loop() {
  rcChannels[0] = roll_value;
  rcChannels[1] = pitch_value;
  rcChannels[2] = throttle_value;
  rcChannels[3] = yaw_value;
  rcChannels[4] = arm_value;
  uint32_t currentMillis = millis();
    // Send SBUS packet at regular intervals
    if (currentMillis > sbusTime) {
        sbusPreparePacket(sbusPacket, rcChannels, false, false);
        Serial1.write(sbusPacket, SBUS_PACKET_LENGTH); // Send SBUS packet through Serial1
        sbusTime = currentMillis + SBUS_UPDATE_RATE;
    }
  
}