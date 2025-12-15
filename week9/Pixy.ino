#include <Pixy.h>

// Use hardware serial Serial1 (pins 18=TX, 19=RX)
Pixy pixy(&Serial1);  // Pass Serial1 to Pixy

void setup() {
  Serial.begin(9600);    // For debugging (USB Serial Monitor)
  Serial1.begin(9600);   // For Pixy communication
  pixy.init();
  Serial.println("✅ Pixy1 connected via Serial1. Waiting for objects...");
}

void loop() {
  int blocks = pixy.getBlocks();

  if (blocks > 0) {
    for (int i = 0; i < blocks; i++) {
      int sig = pixy.blocks[i].signature;
      int x = pixy.blocks[i].x;
      int y = pixy.blocks[i].y;

      String colorName = "";
      switch(sig) {
        case 1: colorName = "blue"; break;
        case 2: colorName = "red"; break;
        case 3: colorName = "yellow"; break;
        default: colorName = "unknown"; break;
      }

      Serial.print("🎯 Object ");
      Serial.print(i + 1);
      Serial.print(": Sig=");
      Serial.print(sig);
      Serial.print(" (");
      Serial.print(colorName);
      Serial.print(") at X=");
      Serial.print(x);
      Serial.print(", Y=");
      Serial.println(y);
    }
  } else {
    Serial.println("🔍 No objects detected.");
  }

  delay(200);
}
