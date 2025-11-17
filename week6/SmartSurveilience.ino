//ESP32 camera: face detection
//NOW WITH LIVE STREAMING WEB SERVER
//It can detect a human face with the model MTMN, move servos, and stream the video.

//ESP32 Camera Driver, Camera OV2640/1600 x 1200/Len Size:1/4"

// --- LIBRARIES ---
#include "esp_camera.h"
#include <WiFi.h>             // --- NEW: Added for web server
#include "esp_http_server.h"  // --- NEW: Added for web server
#include "fd_forward.h"
#include "img_converters.h"   // --- NEW: Added for image conversion
#include "soc/soc.h"           // disable brownout problems
#include "soc/rtc_cntl_reg.h"  // disable brownout problems
#include <ESP32Servo.h>

//***** Servo def
#define DUMMY_SERVO1_PIN 12     //We need to create 2 dummy servos.
#define DUMMY_SERVO2_PIN 13     //So that ESP32Servo library does not interfere with pwm channel and timer used by esp32 camera.
 
#define PAN_PIN 14
#define TILT_PIN 15


Servo dummyServo1;
Servo dummyServo2;
Servo panServo;
Servo tiltServo;
// Published values for SG90 servos; adjust if needed
int minUs = 500;
int maxUs = 2400;
//**** Servo the end

int  posH;  //Position Panoram
int  posV;  //Position Tilt
int altPosH;
bool toNul; //servo moves to Null

// --- 3 THINGS TO CHANGE ---

// 1. & 2. Your Wi-Fi Credentials
const char* ssid = "Archetype Kismees";
const char* password = "raisin2004";

// 3. Your Camera Model
#define CAMERA_MODEL_AI_THINKER

// --- END OF CHANGES ---

#include "camera_pins.h"

// --- NEW: Web Server Handles ---
httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;

// --- Recognition Variables ---
static mtmn_config_t mtmn_config = {0};   // Detection config
 
int noDetection = 0;
  //define a variable that will count how many times we have not detected faces. 
  //We will initialize it with the value zero and increment it every time faces are not detected in a frame.

// --- FIX: Add Function Prototype for initCamera ---
bool initCamera();

//++++++++++++++++++++++++++++++++++++++++++++ function called draw_face_boxes that is used to display a box around a detected face
// --- MODIFIED: This function now just moves the servos ---
static void draw_face_boxes(dl_matrix3du_t *image_matrix, box_array_t *boxes)
{
  int x, y, w, h, i, half_width, half_height;  
  
  for (i = 0; i < boxes->len; i++) {

    // --- FIX: Calculate x,y,w,h first ---
    x = (int)boxes->box[i].box_p[0];
    y = (int)boxes->box[i].box_p[1];
    w = (int)boxes->box[i].box_p[2] - x;
    h = (int)boxes->box[i].box_p[3] - y;

    // finding face centre...
    half_width = w / 2;
    
    // --- FIX: Define face_center_pan. ---
    int face_center_pan = x + half_width; // image frame face centre x co-ordinate
    
    altPosH=posH;
    half_height = h / 2;
    
    // --- SPEED CONTROL ---
    // To make it faster, change the /1 to /2 or /1
    // To make it slower, change it to /3 or /4
    posH =posH + (160 - face_center_pan)/1; //was 4,
    
    if ((altPosH-posH)>0) 
    { 
      //+++++++++
     toNul=false;  //// servo move to 0 grad
    }
    else
    {
       toNul=true; // servo move to plus 180
    }
    
    altPosH=posH;
    
    if (posH>170) 
    { 
      //+++++++++
      posH=170;
    }
    if (posH<20) 
     {
      //+++++++++
     posH=20;
    }
    
    Serial.printf("Center detected at %d dots\n", face_center_pan);
    panServo.write(posH);
    
  // 
  Serial.printf("H%d \n", posH);
   
    //++++++++++++++++++++++++ Center TILT is 120 ++++++++++++++++++++++++++++++++++++++++++++++++++   
    int face_center_tilt = y + half_height;  // image frame face centre y co-ordinate
    
    // --- SPEED CONTROL ---
    posV =posV - (120 - face_center_tilt)/1;  //
    
    if (posV>130) 
    { 
      posV=130;   //LIMIT UP;
       }
    if (posV<50) 
     {
      posV=50;    //LIMIT DOWN
     }
      
       tiltServo.write(posV);
      Serial.printf("V%d \n", posV);
  } 
}


// ======================================================================
// === NEW: WEB SERVER CODE ===
// ======================================================================

// --- NEW: Simple HTML Page ---
const char* INDEX_HTML = R"EOF(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32-CAM Face Tracker</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; text-align: center; margin: 0; padding: 0; background-color: #f4f4f4; }
    h1 { background-color: #007bff; color: white; padding: 20px; margin: 0; }
    #stream-container {
      margin: 20px auto;
      border: 5px solid black;
      width: 800px;
      max-width: 90%;
    }
    img { width: 100%; height: auto; display: block; }
  </style>
</head>
<body>
  <h1>ESP32-CAM Face Tracker</h1>
  <div id="stream-container">
    <!-- The src is now blank. JavaScript will fill it in. -->
    <img src="" id="stream">
  </div>

  <!-- --- FIX: This script tells the browser to look for the stream on port 81 --- -->
  <script>
    document.getElementById("stream").src = window.location.protocol + "//" + window.location.hostname + ":81/stream";
  </script>
</body>
</html>
)EOF";

// --- NEW: Handler for the live stream ---
static esp_err_t stream_handler(httpd_req_t *req){
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char * part_buf[64];

  // --- MJPEG Stream Header ---
  res = httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=--boundarydonotcross");
  if(res != ESP_OK){
    return res;
  }
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  // --- CORE LOGIC LOOP ---
  // This loop runs continuously, sending frames to the browser
  while(true){
    
    // --- 1. Get a frame from the camera ---
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Frame buffer not available");
      res = ESP_FAIL;
      break;
    }
    
    // --- 2. Allocate matrix and convert image for detection ---
    // (This is your code from the old loop())
    dl_matrix3du_t *image_matrix = dl_matrix3du_alloc(1, fb->width, fb->height, 3);
    if (!image_matrix) {
      Serial.println("dl_matrix3du_alloc failed");
      esp_camera_fb_return(fb);
      continue; // Skip this frame
    }
    
    if(!fmt2rgb888(fb->buf, fb->len, fb->format, image_matrix->item)){
        Serial.println("fmt2rgb888 failed");
        dl_matrix3du_free(image_matrix);
        esp_camera_fb_return(fb);
        continue; // Skip this frame
    }

    // --- 3. Run Face Detection & Servo Logic ---
    // (This is your code from the old loop())
    box_array_t *boxes = face_detect(image_matrix, &mtmn_config);
    
    if (boxes != NULL) {
      noDetection = 0;
      Serial.printf("Faces detected at %d \n", millis());
      draw_face_boxes(image_matrix, boxes); // This moves the servos
      dl_lib_free(boxes->score);
      dl_lib_free(boxes->box);
      dl_lib_free(boxes->landmark);
      dl_lib_free(boxes);
    }
    else
    {  
      noDetection = noDetection+1;
      Serial.printf("Faces not not detected %d times \n", noDetection);
      switch(noDetection){
        case 10:  Serial.printf("Case10 Nodetected at %d \n", millis()); break;
        case 40: {
                   Serial.printf("Case40 Nodetected at %d \n", millis());
                   noDetection = 0; // stop
                 }
                 break;
        default:  break;
      }
      if (toNul) 
      {
         posH += 2; // --- SPEED INCREASED ---
         if (posH>170) {
           posH=170;
           posV=50;
           tiltServo.write(posV);
           toNul=false;
         }
         panServo.write(posH);
       }
       else 
       {
         posH -= 2; // --- SPEED INCREASED ---
         if (posH<10) { 
           posH=10;
           posV=90;
           tiltServo.write(posV);
           toNul=true;
         }
         panServo.write(posH);
       }
    }

    // --- 4. Convert the final frame (with box) to JPEG ---
    // We convert the RGB matrix *back* to a JPEG to send to the browser
    if(!fmt2jpg(image_matrix->item, fb->width*fb->height*3, fb->width, fb->height, PIXFORMAT_RGB888, 90, &_jpg_buf, &_jpg_buf_len)){
        Serial.println("fmt2jpg failed");
        dl_matrix3du_free(image_matrix);
        esp_camera_fb_return(fb);
        continue;
    }
    
    // --- 5. Free resources from detection ---
    dl_matrix3du_free(image_matrix);
    esp_camera_fb_return(fb); // IMPORTANT: return the original frame buffer

    // --- 6. Send the JPEG frame to the browser ---
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, "--boundarydonotcross\r\n", 22);
    }
    if(res == ESP_OK){
      sprintf((char *)part_buf, "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, strlen((char *)part_buf));
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, "\r\n", 2);
    }
    
    // --- 7. Free the JPEG buffer ---
    free(_jpg_buf);
    _jpg_buf = NULL;
    
    if(res != ESP_OK){
      break; // Stop streaming if an error occurs
    }
    
    delay(10); // Small delay to not overwhelm the system
  } // --- End of while(true) loop ---

  return res;
}

// --- NEW: Handler for the HTML Page (v1.0.6 compatible) ---
static int index_handler(httpd_req_t *req){
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "identity");
    return httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
}

// --- NEW: Function to start the web servers ---
void startCameraServer(){
  
  // --- FIX: Use two separate config structs for safety ---
  httpd_config_t config_main = HTTPD_DEFAULT_CONFIG();

  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t stream_uri = {
    .uri       = "/stream",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };

  // Start the server for port 80 (HTML page)
  if (httpd_start(&camera_httpd, &config_main) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
  }

  // Start the server for port 81 (Stream)
  httpd_config_t config_stream = HTTPD_DEFAULT_CONFIG(); // Create a NEW config
  config_stream.server_port = 81;
  config_stream.ctrl_port = 32769; // Use a different control port
  
  if (httpd_start(&stream_httpd, &config_stream) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  }
}
      
// ======================================================================
// === SETUP FUNCTION ===
// ======================================================================
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // disable brownout detector
  
  Serial.begin(115200);
    
  // --- FIX: Attach dummy servos *before* camera init ---
  dummyServo1.attach(DUMMY_SERVO1_PIN);
  dummyServo2.attach(DUMMY_SERVO2_PIN);

  if (!initCamera()) {
    Serial.printf("Failed to initialize camera...");
    return;
  }
    
  posH=90;
  posV=90;
  toNul = true; // Start by searching
  
  Serial.println("Face_FollowMe24-1");
  
  mtmn_config = mtmn_init_config();
  //****** Servo
  panServo.setPeriodHertz(50);      // Standard 50hz servo
  tiltServo.setPeriodHertz(50);      // Standard 50hz servo
  
  panServo.attach(PAN_PIN, minUs, maxUs);
  tiltServo.attach(TILT_PIN, minUs, maxUs);
  panServo.write(posH); //pan angle 90 start
  tiltServo.write(posV);  //tilt angle 90 start
  
  // --- NEW: Connect to WiFi ---
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected!");

  // --- NEW: Start Web Server ---
  startCameraServer();

  Serial.print("Go to this IP in your browser: http://");
  Serial.println(WiFi.localIP());
}


// ======================================================================
// === LOOP FUNCTION ===
// ======================================================================
void loop() {
  // Everything is handled by the web server (stream_handler)
  // The loop can be empty.
  delay(10000);
}

// ======================================================================
// === CAMERA INIT FUNCTION ===
// ======================================================================
bool initCamera() {
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
  
  // --- FIX: Remove the bad line ---
  config.pin_href = HREF_GPIO_NUM; // Corrected

  // --- FINAL FIX (v1.0.6): Use the correct 1.0.6 board version pin names ---
  config.pin_sscb_sda = SIOD_GPIO_NUM; // Was config.pin_sccb_sda
  config.pin_sscb_scl = SIOC_GPIO_NUM; // Was config.pin_sccb_scl
 
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;

  // --- MODIFIED: Use a raw format for face detection ---
  config.pixel_format = PIXFORMAT_YUV422; // Was JPEG
  config.frame_size = FRAMESIZE_QVGA;     // 320x240
  config.jpeg_quality = 10;
  config.fb_count = 1;
 
  // --- FINAL TYPO FIX ---
  esp_err_t result = esp_camera_init(&config); // Was esp_camera_.init
 
  if (result != ESP_OK) {
    return false;
  }
 
  return true;
}