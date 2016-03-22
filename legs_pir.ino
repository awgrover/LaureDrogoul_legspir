// Wiring
// Red = Power
// White = Ground
// Black = Alarm

const int PIR1_PIN=2;
const int PIR2_PIN=3;
const int ONBOARD=13;
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
    Pir_State check();
};


Pir Pir1(PIR1_PIN);
Pir Pir2(PIR2_PIN);

void setup() {
    Serial.begin(115200);
    Pir1.init();
    pinMode(ONBOARD, OUTPUT);
    digitalWrite(ONBOARD,LOW);

    Serial.println("Stabilize...");
    delay(2000);
    Serial.println("Start");
}


void loop() {
    int p1 = Pir1.check();

    // So, On, WaitOff is ON
    if (p1 == On || p1 == WaitOff) {
        // ON
        digitalWrite(ONBOARD,HIGH);
    }
    else {
        digitalWrite(ONBOARD,LOW);
    }
}

Pir_State Pir::check() {
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

    return pir_state;
}
