// Wiring
// Red = Power => 5V
// White = Ground => GND
// Black = Alarm => (digital) pin 2 & 4 (don't use pwm pins?)

const int PIR1_PIN=2;
const int PIR2_PIN=4;
const int ONBOARD=13;

#include <SoftwareSerial.h>
const int RX=10, TX=11;

const int MP3TRIGGER=1; // we use MP3TRIGGER..IdleSound pins
const int MovementSounds=3; // +0..2 are the movement cases
const int IdleSound=MP3TRIGGER + MovementSounds; // idlesound pin
const int IdleOnMP3=MovementSounds+1; // Which trigger on the mp3, just for reference


const int PirDebounceOn = 600; // millis
const int PirDebounceOff = 1000; // millis till it allows -> Off

enum Pir_State { Off, WaitOn, On, WaitOff };
const int PirNothing = 1;
const int PirMovement = 0;

struct Player {
    const char* sound_list = NULL;
    const char* was_sound_list = (const char*) -1; 
    int sound_idx;

    // the list is a string of the digits of interest "1", "345", etc.
    void sounds(const char* sound_list) { this->sound_idx=-1; this->sound_list=sound_list; }
    void rand_play();
    };

struct Pir {
    int pin;
    Pir_State pir_state;
    unsigned long wait_on, wait_off; // don't need init
    int was;

    Pir(int a_pin) : pin(a_pin), pir_state(Off), was(-1) {}
    void init() { pinMode(pin, INPUT_PULLUP); }
    bool check();
};


Pir Pir1(PIR1_PIN);
Pir Pir2(PIR2_PIN);
SoftwareSerial MP3(RX,TX);
Player player;


void setup() {
    Serial.begin(115200);
    Pir1.init(); 
    Pir2.init();
    pinMode(ONBOARD, OUTPUT);
    digitalWrite(ONBOARD,LOW);

    Serial.print("Sounds start at digital ");Serial.print(MP3TRIGGER);Serial.print(", idle-sound is digital ");Serial.print(IdleSound);Serial.print(" which is the TRIG");Serial.println(IdleOnMP3);

    MP3.begin(38400);
    MP3.listen();
    mp3_ask("MP3: ","S0");
    mp3_ask("tracks: ","S1");

    Serial.print(millis()); Serial.print(" T");Serial.println(IdleSound+1);
    player.sounds("567");
    player.rand_play();

    Serial.println("Stabilize...");
    // delay(2000);
    Serial.println("Start");
}

void mp3_ask(const char *msg, const char *cmd) {
    Serial.print(msg);
    MP3.print(cmd);
    while (MP3.available() < 1) {}
    while (MP3.available() > 0) {
        char v = MP3.read();
        Serial.print(v);
        delay(10);
        }
    Serial.println();
}

void loop() {
        player.rand_play();
        }

void Player::rand_play() {
    // you should repeatedly call this, and it will play the next random sound when the previous finishes

    // we are safe
    if (sound_list == NULL) { return; }

    int sound_len = strlen(sound_list); // could cache this

    // start, pick one
    if (was_sound_list != sound_list) {
        this->was_sound_list = sound_list; // a flag, so we know if we have to re-start
        this->sound_idx = random(sound_len);
        Serial.print("First idx from ");Serial.print(sound_list);Serial.print(" -> ");Serial.println(sound_idx);
        MP3.print("T");MP3.print( sound_list[sound_idx] );
        return;
        }

    // Must be playing, so wait
    if (MP3.available() < 1) { return; }

    // Expect an 'X' for "done"
    char v = MP3.read();
    if (v != 'X') {
        Serial.print("mp3 didn't finish: ");Serial.println(v);
        return;
        }

    // e.g. if sound_len is 3, subtract one so it's like 0,1,2
    // add rand(1 or 2), 
    // mod so back to 0..2 (being not the original sound_idx), 
    // and index is 0 based so done
    this->sound_idx = ( sound_idx + (random(sound_len - 1) + 1)) % sound_len;
    Serial.print("Next idx ");Serial.println(sound_idx);

    MP3.print("T");MP3.print( sound_list[sound_idx] );
}

void xloop() {

    bool p1 = Pir1.check();
    bool p2 = Pir2.check();

    // We want a progression of sounds:
    // idle -> idle sound
    // idle, det1 -> "intro sound"
    // det1 + det2 -> vigorous
    // det1&det2 - det1 -> bye-sound
    if (p1 && p2) {
        // ON
        digitalWrite(ONBOARD,HIGH);
    }
    else if (p1 || p2) {
        digitalWrite(ONBOARD, ((millis()/300) % 2) ? HIGH : LOW);
        reset_to_sound(MP3TRIGGER);
    }
    else {
        digitalWrite(ONBOARD,LOW);
        reset_to_sound(IdleSound);
    }
}

void reset_to_sound(int want_pin) {
    static int last_pin = 0;

    // Avoid doing anything if no change
    if (last_pin == want_pin) { return; }

    Serial.print("Pin ");Serial.print(want_pin);Serial.print(" was ");Serial.println(last_pin);

    last_pin = want_pin;

    // Have to turn off the sound before turning on the next
    for (int i=MP3TRIGGER; i<=IdleSound; i++) {
        Serial.print(i);
        //digitalWrite(i, HIGH); // "donothing"
        // digitalWrite(i, LOW); // pullup off
        // pinMode(i, INPUT); // "open"==pull-up=="donothing"
        }
    Serial.println();

    //digitalWrite(want_pin, LOW); // HIGH is "nothing", LOW is "trigger"
    Serial.print("->");Serial.println(want_pin);
}

bool Pir::check() {
    // returns true if "on"
    int pirVal = digitalRead(pin);

    // States: debounce on / off
    // Off -> movement for wait_on -> On
    //     <- nothing
    // Off <- nothing for wait_off <- nothing
    //                    movement ->

    switch ( pir_state ) {
        case Off:
            if (pirVal == PirMovement) {
                wait_on = millis() + PirDebounceOn;
                pir_state = WaitOn;
            }
            // otherwise, nothing -> stay off
            break;

        case WaitOn:
            if ( pirVal == PirMovement) {
                if (millis() >= wait_on ) {
                    pir_state = On;
                    Serial.print("[");Serial.print(pin);Serial.print("] ");Serial.print(millis()); Serial.print(": ");Serial.println("on");
                }
                // otherwise wait
            }
            else {
                pir_state = Off; // didn't make it to on, too short
            }
            break;

        case On:
            if (pirVal == PirNothing ) {
                wait_off = millis() + PirDebounceOff;
                pir_state = WaitOff;
            }
            // otherwise, movement -> stay on
            break;

        case WaitOff:
            if ( pirVal == PirNothing) {
                if (millis() >= wait_off ) {
                    Serial.print("[");Serial.print(pin);Serial.print("] ");Serial.print(millis()); Serial.print(": ");Serial.println("off");
                    pir_state = Off;
                }
                // otherwise wait
            }
            else {
                pir_state = On; // didn't make it to off, too short
            }
            break;
    }

    if (was != pirVal) {
        Serial.print("[");Serial.print(pin);Serial.print("] ");Serial.print(millis()); Serial.print(": ");Serial.print(pirVal); Serial.print(" "); Serial.println(pir_state);
        was = pirVal;
        }

    return pir_state == On || pir_state == WaitOff;
}
