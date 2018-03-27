#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "janus_font.h"


#define OLED_ADDRESS                 0x3C

#define RFID_RST_PIN                 D4
#define RFID_SS_PIN                  A2

#define RELAY_PIN                    D5


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

#define TIME_DEPARTING           10000
#define TIME_AUTH_FAIL           3000
#define TIME_AUTH_REQ            5000

state_t prev_state, new_state;
MFRC522::Uid current_uid;
long state_timer = 0;
long use_timer = 0;
bool cloud_open = false;

const unsigned char janus_logo [] PROGMEM = {
        // 'janus_logo_small', 32x32px
        0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x07, 0xff, 0xff, 0x83, 0xc1, 0xff, 0xf0, 0x03, 0xc0, 0x0f,
        0xe7, 0xf0, 0x0f, 0xe3, 0xcf, 0xfe, 0x7f, 0xfb, 0xdf, 0xfe, 0x7f, 0xf9, 0xc7, 0xfe, 0x7f, 0xf3,
        0xc3, 0xfe, 0x7f, 0xc3, 0x98, 0x7e, 0x7e, 0x19, 0x9e, 0x7e, 0x7e, 0x79, 0x9e, 0x7e, 0x7f, 0x79,
        0x8e, 0x7e, 0x7e, 0x71, 0xc6, 0x7e, 0x7e, 0x63, 0x9f, 0x3e, 0x7c, 0xf9, 0x9f, 0x9e, 0x79, 0xf9,
        0xbf, 0xbe, 0x7d, 0xfd, 0x3f, 0x3e, 0x7c, 0xfc, 0x07, 0x7e, 0x7e, 0x60, 0xc2, 0x7e, 0x7e, 0x43,
        0xd8, 0xfe, 0x7f, 0x19, 0xcc, 0xfe, 0x7f, 0x33, 0xef, 0xfe, 0x7f, 0xf7, 0xcf, 0xf8, 0x1f, 0xf3,
        0x9f, 0x01, 0x80, 0xf9, 0xbe, 0x0f, 0xf0, 0x7d, 0x88, 0xfe, 0x7f, 0x11, 0xc0, 0xc0, 0x03, 0x83,
        0xfc, 0x0f, 0xf0, 0x3f, 0xff, 0x1f, 0xf8, 0xff, 0xff, 0xc0, 0x01, 0xff, 0xff, 0xf8, 0x0f, 0xff,
};


void setup() {
    Serial.begin(9600);

    SPI.begin();
    mfrc522.PCD_Init();

    Particle.function("cmd", handle_cmd);

    prev_state = STATE_STARTUP;

    display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
//    display.setFont(&JanusFont);
//    display.setRotation(3);

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);

    display.drawBitmap(30, 16, janus_logo, 32, 32, 1);

//    oled_message("Startup...");

    delay(2000);

    set_state(STATE_IDLE);
}

int handle_cmd(String cmd) {

    if (cmd == "auth-ok") {
        handle_auth_ok();
    } else if (cmd == "auth-fail") {
        handle_auth_fail();
    } else if (cmd == "depart") {
        handle_depart();
    }

    return 0;
}

void handle_auth_ok() {
    if (prev_state == STATE_AUTHENTICATING) {
        set_state(STATE_AUTHENTICATED);
    }
}

void handle_auth_fail() {
    if (prev_state == STATE_AUTHENTICATING) {
        set_state(STATE_AUTH_FAIL);
    }
}

void handle_depart() {
    set_state(STATE_DEPARTED);
}

void loop() {

    state_t cur_state = new_state;

    switch(cur_state) {
        case STATE_IDLE:
            run_state_idle();
            break;
        case STATE_AUTHENTICATING:
            run_state_authenticating();
            break;
        case STATE_AUTHENTICATED:
            run_state_authenticated();
            break;
        case STATE_DEPARTING:
            run_state_departing();
            break;
        case STATE_DEPARTED:
            run_state_departed();
            break;
        case STATE_AUTH_FAIL:
            run_state_auth_fail();
            break;
    }

    prev_state = cur_state;
}

void set_state(state_t s) {
    new_state = s;
}

bool is_state_change() {
    return new_state != prev_state;
}

bool is_card_present() {
    return mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial();
}

bool is_card_different() {
    return uidcmp(current_uid, mfrc522.uid);
}

void oled_message(String str) {
    display.clearDisplay();
    display.setCursor(10,0);
    display.println(str);
    display.display();
}

void run_state_idle() {

    if (is_state_change()) {
        oled_message("Insert Card");
    }

    if (is_card_present()) {
        current_uid = mfrc522.uid;
        set_state(STATE_AUTHENTICATING);
    }
}

void run_state_authenticating() {

    // TODO: send authenticating event, wait for response

    if (is_state_change()) {
        Particle.publish("auth-req", current_uid);
        state_timer = millis();
    }

    if (state_timer + TIME_AUTH_REQ < millis()) {
        oled_message("Auth timeout...");
        delay(2000);
        set_state(STATE_IDLE);
    }
}

void run_state_authenticated() {

    if (is_state_change()) {
        use_timer = millis();
        digitalWrite(RELAY_PIN, HIGH);
        current_uid = mfrc522.uid;

        display.clearDisplay();
        display.setCursor(10,0);
        display_uid(current_uid);
        display.println("");
        display.display();
    }

    if (!is_card_present()) {
        set_state(STATE_DEPARTING);
    }
}

void run_state_departing() {

    if (is_state_change()) {
        oled_message("Stopping...");

        state_timer = millis();
    }

    if (is_card_present()) {
        if (is_card_different()) {
            set_state(STATE_IDLE);
        } else {
            set_state(STATE_AUTHENTICATED);
        }
    } else {
        if (state_timer + TIME_DEPARTING < millis()) {
            set_state(STATE_DEPARTED);
        }
    }
}

void run_state_departed() {

    digitalWrite(RELAY_PIN, LOW);

    set_state(STATE_IDLE);
}

void run_state_auth_fail() {

    if (is_state_change()) {
        oled_message("Auth failed!");
    }

    delay(3000);
    set_state(STATE_IDLE);
}

void display_uid(MFRC522::Uid uid) {
    for (byte i = 0; i < uid.size; i++) {
        display.print(uid.uidByte[i] < 0x10 ? " 0" : " ");
        display.print(uid.uidByte[i], HEX);
    }
}

bool uidcmp(MFRC522::Uid u1, MFRC522::Uid u2) {
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
