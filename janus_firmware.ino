#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "janus_font.h"


#define OLED_ADDRESS                 0x3C

#define RFID_RST_PIN                 D4
#define RFID_SS_PIN                  A2



MFRC522 mfrc522(RFID_SS_PIN, RFID_RST_PIN);
Adafruit_SSD1306 display(-1);


enum state_t {
    STATE_IDLE,
    STATE_STARTUP,
    STATE_AUTHENTICATING,
    STATE_AUTHENTICATED,
    STATE_AUTH_FAIL,
    STATE_DEPARTING,
    STATE_DEPARTED
};

state_t state;
MFRC522::Uid current_uid;


void setup() {
    Serial.begin(9600);

    SPI.begin();
    mfrc522.PCD_Init();

    display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
//    display.setFont(&AuraFont);
//    display.setRotation(3);

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);

    display.clearDisplay();

    state = STATE_STARTUP;

    display.setCursor(10,0);
    display.println("Startup");
    display.display();
    delay(1000);
}

void loop() {


    switch(state) {
        case STATE_IDLE:
            run_state_idle();
            break;
    }


    display.clearDisplay();
    display.setCursor(10,0);
    display.println("Looking for card...");
    display.display();


    if (! mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
        return;
    }



    display.clearDisplay();
    display.setCursor(10,0);
    display_uid(mfrc522.uid);
    display.println("");
    display.display();

    delay(5000);
}


void run_state_idle() {

}

void display_uid(MFRC522::Uid uid) {
    for (byte i = 0; i < uid.size; i++) {
        display.print(uid.uidByte[i] < 0x10 ? " 0" : " ");
        display.print(uid.uidByte[i], HEX);
    }
}

boolean uidcmp(MFRC522::Uid u1, MFRC522::Uid u2) {
    if (u1.size != u2.size) {
        return false;
    }
    for (byte i = 0; i < u1.size; i++) {
        if (u1.uidByte[i] != u2.uidByte[i]) {
            return false;
        }
    }
    return true;
}



void testscrolltext(void) {
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(10,0);
    display.clearDisplay();
    display.println("scroll");
    display.display();

    display.startscrollright(0x00, 0x0F);
    delay(2000);
    display.stopscroll();
    delay(1000);
    display.startscrollleft(0x00, 0x0F);
    delay(2000);
    display.stopscroll();
    delay(1000);
    display.startscrolldiagright(0x00, 0x07);
    delay(2000);
    display.startscrolldiagleft(0x00, 0x07);
    delay(2000);
    display.stopscroll();
}
