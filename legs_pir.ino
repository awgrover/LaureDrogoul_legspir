// Wiring
// Red = Power => 5V
// White = Ground => GND
// Black = Alarm => (digital) pin 2 & 4 (don't use pwm pins?)

const int PIR1_PIN=2;
const int PIR2_PIN=4;
const int ONBOARD=13;

const int MP3TRIGGER=5; // we use MP3TRIGGER..IdleSound pins
const int MovementSounds=3; // +0..2 are the movement cases
const int IdleSound=MP3TRIGGER + MovementSounds; // idlesound pin
const int IdleOnMP3=MovementSounds+1; // Which trigger on the mp3, just for reference

const int PirDebounceOn = 600; // millis
const int PirDebounceOff = 1000; // millis till it allows -> Off

enum Pir_State { Off, WaitOn, On, WaitOff };
const int PirNothing = 1;
const int PirMovement = 0;

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

void setup() {
    Serial.begin(115200);
    Pir1.init(); 
    Pir2.init();
    pinMode(ONBOARD, OUTPUT);
    digitalWrite(ONBOARD,LOW);

    Serial.print("Sounds start at digital ");Serial.print(MP3TRIGGER);Serial.print(", idle-sound is digital ");Serial.print(IdleSound);Serial.print(" which is the TRIG");Serial.println(IdleOnMP3);

    for (int i=MP3TRIGGER; i<=IdleSound; i++) {
        pinMode(i, OUTPUT);
        digitalWrite(i, HIGH); // HIGH is "nothing", LOW is "trigger"
        }
    digitalWrite(IdleSound, LOW);

    Serial.println("Stabilize...");
    delay(2000);
    Serial.println("Start");
}


void loop() {
 
    bool p1 = Pir1.check();
    bool p2 = Pir2.check();

    // So, On, WaitOff is ON
    if (p1 && p2) {
        // ON
        digitalWrite(ONBOARD,HIGH);
    }
    else if (p1 || p2) {
      digitalWrite(ONBOARD, ((millis()/300) % 2) ? HIGH : LOW);
    }
    else {
        digitalWrite(ONBOARD,LOW);
    }
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
