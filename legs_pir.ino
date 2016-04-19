// PIR Wiring
// Red = Power => 5V
// White = Ground => GND
// Black = Alarm => (digital) pin 2 & 4 (don't use pwm pins?)

#include <SoftwareSerial.h>
extern int __bss_end;
extern void *__brkval;


const int PIR1_PIN=2;
const int PIR2_PIN=4;
// const int PirDebounceOn = 400; // millis (file names are 001...)
// const int PirDebounceOff = 300; // millis till it allows -> Off
const int PirWaitOn = 90; // I see 90ms pulses when motion is borderline, longer == on
const int PirWaitOff = 300; // during movement, I see oscillation from on->off, with off <~ 300ms

const int ONBOARD=13; // the on-board led for subtle signalling: ON=1&2, blink=1||2, off=no movement
const int RX=5, TX=6; // for the MP3 board (opposite designation on mp3board)

// SOUND Defn
// Each "zone" of movement (can have several sounds, will be randomly chosen/looped among)
// See MovementSound, and setup_sound_lists()
const int MovementSoundCounts[] = {1,1,1,0}; // how many sounds in each "zone", terminate list with 0
// Idle sounds, start at last_movement_sound + 2 (there's a gap) (file names are 001...)
// see IdleSounds, and setup_sound_lists()
const int IdleSoundCount = 3; // how many sounds for idling


// see https://learn.sparkfun.com/tutorials/mp3-trigger-hookup-guide-v24
// https://cdn.sparkfun.com/datasheets/Widgets/vs1063ds.pdf
struct Player {
    const int* was_sound_list = (const int*) -1; // so it starts different from sound_list
    int sound_idx;

    // the list is a string of the digits of interest "1", "345", etc.
    void rand_play(const int* sound_list);
    };

enum Pir_State { Off, WaitOn, On, WaitOff };
// const int PirNothing = 1; // from digitalRead(pir)
// const int PirMovement = 0; // from digitalRead(pir)

enum PirMachineStates { IDLE, WAIT_ON, ACTIVE, WAIT_OFF };

struct Pir {
    int pin;
    PirMachineStates machine_state;
    bool state;
    unsigned long last_state_time;
    // unsigned long last_change, last_pir_change; // // wait_on, wait_off; // don't need init
    int was;

    Pir(int a_pin) : pin(a_pin), state(false), was(-1), last_state_time(0) {}
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

    if (Serial.available()>0) { handle_commands(); }

    bool p1 = Pir1.check();
    bool p2 = Pir2.check();
    // if (p1||p2) {Serial.print("<");Serial.print(p1);Serial.print(",");Serial.print(p2);Serial.println(">");}

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

void handle_commands() {
    // for serial-console debugging

    static unsigned char command = '?'; // default is show prompt

    // stay in loop while data or "because we want to"
    while (Serial.available() >0 || command >= 0xfe || command == '?') {
        if (Serial.available() > 0) command = Serial.read();
        switch (command) {
            // set command to '?' to display menu w/prompt
            // set command to 0xff to prompt
            // set command to 0xfe to just get input

            case 'i': // play idle sounds
                player.rand_play(IdleSounds);
                command = 0xff;
                break;

            case 'x': // back to regular operation
                return;
                break;
            
            case '1':  // ..9 play sound n
            case '2': 
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                MP3.print("T");MP3.print( (char) command );
                command = 0xff;
                break;

            case '?':
                // menu made by:
    Serial.println(F("a  your command (this is desc)"));
    Serial.println(F("1  ..'9' play sound n"));
                // end menu

            case 0xff : // show prompt, get input
              Serial.print(F("Choose (? for help): "));
              // fallthrough

            case 0xfe : // just get input
              while (Serial.available() <= 0) {
                if (MP3.available()>0) {Serial.print("MP3: ");Serial.println( (char) MP3.read() ); }
                };
              break;

              command = Serial.read();
              Serial.println(command);
              break;

            default : // show help if not understood
              delay(10); while(Serial.available() > 0) { Serial.read(); delay(10); } // empty buffer
              command = '?';
              break;
        }
    }
}

bool Pir::check() {
    // "debounce"/interpret for "at range"
    /* 
        --simplified--
        IDLE (off):
            <stay>
            move (WAIT_ON)
                <stay>
                dead -> OFF IDLE
                >90 -> ACTIVE,on // ignore fast changes from idle
                    ACTIVE:
                        <stay>
                        dead (WAIT_OFF)
                            move -> ON ACTIVE
                            >300 -> OFF IDLE
    */

    unsigned long now = millis();
    bool movement = digitalRead(pin) == 0;

    // even if no change since last pin-read, we have to check elapsed time in a state, etc, so:

    switch (machine_state) {
        case IDLE:
            if (movement) {
                last_state_time = now;
                machine_state = WAIT_ON; // might go on
            }
            // otherwise, stay IDLE
            break;

        case WAIT_ON:
            // we've seen "movement", but is it real?
            if (movement) {
                if (now - last_state_time > PirWaitOn) {
                    last_state_time = now;
                    machine_state = ACTIVE;
                    state = true;
                    }
                // otherwise, waiting for on/off
                }
            else {
                machine_state = IDLE; // return to IDLE if not on long enough
                last_state_time = now;
                }
            break;

        case ACTIVE:
            // (if elapsed > 500 with no "off" in-between, the movement was really close to the detector)
            if (!movement) {
                last_state_time = now;
                machine_state = WAIT_OFF;
                }
            // otherwise, stay on
            break;

        case WAIT_OFF:
            if (movement) {
                last_state_time = now;
                machine_state = ACTIVE; // back to active
                }
            else if (now - last_state_time > PirWaitOff) {
                last_state_time = now;
                machine_state = IDLE;
                state = false;
                }
            // otherwise, wait for movement or time
            break;

    }
    return this->state;
}
