// Wiring
// Red = Power => 5V
// White = Ground => GND
// Black = Alarm => (digital) pin 2 & 4 (don't use pwm pins?)

#include <SoftwareSerial.h>
extern int __bss_end;
extern void *__brkval;


const int PIR1_PIN=2;
const int PIR2_PIN=4;
const int PirDebounceOn = 600; // millis
const int PirDebounceOff = 1000; // millis till it allows -> Off

const int ONBOARD=13; // the on-board led for subtle signalling: ON=1&2, blink=1||2, off=no movement
const int RX=10, TX=11; // for the MP3 board

// Each "zone" of movement (can have several sounds, will be randomly chosen/looped among)
// See MovementSound, and setup_sound_lists()
const int MovementSoundCounts[] = {1,1,1,0}; // how many sounds in each "zone", terminate list with 0
// Idle sounds, start at last_movement_sound + 2 (there's a gap)
// see IdleSounds, and setup_sound_lists()
const int IdleSoundCount = 3; // how many sounds for idling


struct Player {
    const int* was_sound_list = (const int*) -1; // so it starts different from sound_list
    int sound_idx;

    // the list is a string of the digits of interest "1", "345", etc.
    void rand_play(const int* sound_list);
    };

enum Pir_State { Off, WaitOn, On, WaitOff };
const int PirNothing = 1; // from digitalRead(pir)
const int PirMovement = 0; // from digitalRead(pir)
struct Pir {
    int pin;
    Pir_State pir_state;
    unsigned long wait_on, wait_off; // don't need init
    int was; // last digitalRead, PirNothing || PirMovement

    Pir(int a_pin) : pin(a_pin), pir_state(Off), was(-1) {}
    void init() { pinMode(pin, INPUT_PULLUP); }
    bool check();
};


Pir Pir1(PIR1_PIN);
Pir Pir2(PIR2_PIN);
SoftwareSerial MP3(RX,TX);
Player player;
int IdleSounds[IdleSoundCount + 1]; // see setup_sound_lists()
int **MovementSound; // see setup_sound_lists()


void setup() {
    Serial.begin(115200);
    Pir1.init(); 
    Pir2.init();
    pinMode(ONBOARD, OUTPUT);
    digitalWrite(ONBOARD,LOW);

    Serial.print(F("Free memory "));Serial.println(get_free_memory());
    setup_sound_lists(); // let it print the zones/sound-nums
    Serial.print(F("Free memory "));Serial.println(get_free_memory());


    MP3.begin(38400);
    MP3.listen();

    Serial.println();
    mp3_ask("MP3: ","S0");
    mp3_ask("tracks: ","S1");

    player.rand_play(IdleSounds);

    Serial.println("Stabilize...");
    delay(2000);
    Serial.println("Start");
}

int get_free_memory()
{
  int free_memory;

  if((int)__brkval == 0)
    free_memory = ((int)&free_memory) - ((int)&__bss_end);
  else
    free_memory = ((int)&free_memory) - ((int)__brkval);

  return free_memory;
}

// How long is an array that is terminated by a 0 (or null):
int term_array_len(const int *list) {
    int i;
    for (i=0; list[i] != 0; i++) {};
    return i;
}
int term_array_len(const int **list) {
    int i;
    for (i=0; list[i] != NULL; i++) {};
    return i;
}

void setup_sound_lists() {
    // Need to setup MovementSound = { {1,2,3,0}, {4,5,0} ... }
    // Try to be generalized and easy in the declaration, so calculate & setup based on counts

    // allocate space MovementSound[]
    int zone_ct = term_array_len( MovementSoundCounts );
    MovementSound = new int* [zone_ct + 1];
    MovementSound[zone_ct] = NULL; // terminate it
    Serial.print("Zones ");Serial.println(zone_ct);

    // allocate each entry in MovementSoundCounts[ [] ]
    int next_sound_num = 1;
    for (int zone_i=0; MovementSoundCounts[zone_i] != 0; zone_i++) {
        for (int sound_i=0; sound_i <  MovementSoundCounts[zone_i]; sound_i++) {
             MovementSound[zone_i] = new int [ MovementSoundCounts[zone_i] + 1];
        }

        int *zone = MovementSound[zone_i];

        Serial.print("Zone ");Serial.print(zone_i);
        // Serial.print(" "); Serial.print((unsigned int) zone); Serial.print(" count "); Serial.print(MovementSoundCounts[zone_i]); Serial.print(" start sound_num ");
        Serial.print(" from ");Serial.print(next_sound_num);

        // Fill it now, from last sound_num
        int sound_i;
        for (sound_i=0; sound_i <  MovementSoundCounts[zone_i]; sound_i++) {
            zone[sound_i] = next_sound_num;
            next_sound_num++;
        }
        sound_i -= 1;
        zone[sound_i + 1] = 0; // terminate it
        // Serial.print("Zone ");Serial.print(zone_i);
        // Serial.print(" last soundnum "); 
        Serial.print(" to ");Serial.println( zone[sound_i] );
    }


    // setup IdleSounds, after a gap
    next_sound_num++;

    Serial.print("IdleSound from ");
    // Serial.print("@");Serial.print((unsigned int) IdleSounds);Serial.print(" ");
    Serial.print(next_sound_num);

    int idle_i;
    for (idle_i=0; idle_i<IdleSoundCount; idle_i++) {
        // Serial.print("  ");Serial.print(idle_i);Serial.print(" = ");Serial.print(next_sound_num);
        IdleSounds[idle_i] = next_sound_num++;
    }
    IdleSounds[idle_i] = 0; // terminate list
    Serial.print(" to "); Serial.println( IdleSounds[idle_i-1] );
}

char mp3_ask(const char *msg, const char *cmd) {
    // We return the last char, because sometimes that's useful(?)
    char v = 0;

    Serial.print(msg);
    MP3.print(cmd);
    while (MP3.available() < 1) {}
    while (MP3.available() > 0) {
        v = MP3.read();
        Serial.print(v);
        delay(10);
        }
    Serial.println();
    return v;
}


void Player::rand_play(const int* sound_list) {
    // you should repeatedly call this, and it will play the next random sound when the previous finishes

    int sound_len; // delay calculating

    // start, pick one
    if (was_sound_list != sound_list) {
        sound_len = term_array_len(sound_list); // could cache this
        this->was_sound_list = sound_list; // a flag, so we know if we have to re-start
        this->sound_idx = random(sound_len);
        Serial.print(millis()); Serial.print(" First idx from ");Serial.print((unsigned int)sound_list);Serial.print(" len ");Serial.print(sound_len);Serial.print(" -> ");Serial.println(sound_idx);
        MP3.print("T");MP3.print( sound_list[sound_idx] );
        return;
        }

    // Must be playing, so wait
    if (MP3.available() < 1) { return; }

    // Expect an 'X' for "done"
    // We will get a 'x' when we change sounds() (on the next rand_play()), which is ok
    char v = MP3.read();
    if (v != 'X') {
        Serial.print(millis()); Serial.print(" mp3 didn't finish: ");Serial.println(v);
        return;
        }

    sound_len = term_array_len(sound_list); // could cache this

    // e.g. if sound_len is 3, subtract one so it's like 0,1,2
    // add rand(1 or 2), 
    // mod so back to 0..2 (being not the original sound_idx), 
    // and index is 0 based so done
    int offset = random(sound_len - 1) + 1;
    // Serial.print("(was ");Serial.print(sound_idx);Serial.print(" offset will be ");Serial.print(offset);Serial.print(" sum ");Serial.print(sound_idx + offset);Serial.print(" mod ");Serial.println( ( sound_idx + offset) % sound_len );
    this->sound_idx = ( sound_idx + offset) % sound_len;
    Serial.print(millis()); Serial.print(" Next idx ");Serial.print(sound_idx);Serial.print("/");Serial.print(sound_len); Serial.print(" sound_num ");Serial.println( sound_list[sound_idx]);

    MP3.print("T");MP3.print( sound_list[sound_idx] );
}

void loop() {

    bool p1 = Pir1.check();
    bool p2 = Pir2.check();

    // We want a progression of sounds:
    // idle -> idle sound
    // idle, det1 -> "intro sound"
    // det1 + det2 -> vigorous
    // det1&det2 - det1 -> bye-sound
    // NB: this immediately changes the sound
    // But, some states may be short
    if (p1 && p2) {
        // ON
        digitalWrite(ONBOARD, HIGH);
        player.rand_play(MovementSound[0]);
    }
    else if (p1 || p2) {
        digitalWrite(ONBOARD, ((millis()/300) % 2) ? HIGH : LOW);
        player.rand_play(MovementSound[1]);
    }
    else {
        digitalWrite(ONBOARD,LOW);
        player.rand_play(IdleSounds);
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
