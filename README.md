# Motion Sensors triggering MP3-Board

Plays idle sounds, then another sound when the overlapping motion sensors both fire.

Parts:

* Arduino UNO compatible
* Sound-board (Sparkfun *WIG-13720*)
* 2x PIR sensors (Sparkfun *SEN-13285*)
* USB power supply (500mA probably works) & appropriate usb-cable for the Arduino
* Wiring

## Sound File Setup

The mp3-board takes a micro-sd car with a FAT or VFAT filesystem (i.e. format it on a windows machine).

The sound-board (Sparkfun *WIG-13720*) can play some `.wav`, many `.mp3` (not 32bit?), and *ogg-vorbis*. See [the v21063s datasheet](https://cdn.sparkfun.com/datasheets/Widgets/vs1063ds.pdf).

File names start with 3 digits, e.g. `003something.mp3`. The rest of the file-name can be anything, though it ought to be descriptive.

Extra files are ignored, except for MP3TRIGR.INI. You can look up the rules for that file on the [Hookup Guide](https://learn.sparkfun.com/tutorials/mp3-trigger-hookup-guide-v24). But, we aren't relying on it to do anything useful.

### Idle

We continously play _idle_ sounds when there is no movement. We are setup to randomly cycle through 3 sounds, you could make them all the same:

* `005idle1.mp3`
* `006idle1.mp3`
* `007idle1.mp3`
* You could make them different sounds for more variation, of course.
* (remember, the first 3 digits are important, the rest is up to you).

### Overlap Motion

When both sensors detect motion at the same time, we play `002` (looping while there is motion), e.g.:

* `002both.mp3`
* (remember, the first 3 digits are important, the rest is up to you).

### Edge Motion

When only one sensor detects motion, we play `001` (looping while there is motion), e.g.:

* `001justone.mp3`
* You could put the _idle_ sound in as `001`. It will re-start the sound, so you may hear it.
* Not sure what happens if you leave the sound out (no `001`), might ignore the situation....
* (remember, the first 3 digits are important, the rest is up to you).

### More Randomness

Do you want more variation for the movement sounds?

The _only-one_ and _both_ situations can play more than one sound (randomly). You have to change the code:

Find this in `legs_pir.ino`:

    const int MovementSoundCounts[] = {1,1,1,0};

It's a list of how many sounds in _only-one_, and _both_.

If you want to randomly play from 2 sounds for the **only-one-sensor-detect-motion** situation, change the 1st number, like this:

    const int MovementSoundCounts[] = {2,1,1,0};

If you want to randomly play from 3 sounds for the **both-sensors-detect-motion** situation, change the 2nd number, like this:

    const int MovementSoundCounts[] = {1,3,1,0};

You can mix those rules, of course.

Now, **the numbering of the files is changed**. If you use the Arduino IDE, you can open the *serial console*, and it will print the numbering of the sounds (after you compile/upload the new code). Otherwise, count:

* The *only-one-sensor* sounds are: `001`, `002`, `...` depending on the first number in `MovementSoundCounts`. I.e. the example above has *2*, so it would be 2 files, `001` and `002`.
* The *both-sensors* sounds are the next numbers, depending on the second number in `MovementSoundCounts`. So 
    * in the 1st example above, it would be `003` (1 of them, starting after 2). 
    * in the 2nd example above, it would be `002`, `003`, `004` (3 of them, starting after 1). 
* The *idle* sounds are numbered above that, *with a gap*.
    * in the 1st example above, it would be `005` (starting after a gap, i.e. no 004)
    * in the 2nd example above, it would be `006` (starting after a gap, i.e. no 005)

If you want more (or less) sounds for **idle**, change:

    const int IdleSoundCount = 3;

And then you'll need more or less files. Extra files are ignored.

Remember the manual rocker on the MP3 board too!

# Install

You only need the Arduino IDE (or equivalent). This was developed under Arduino IDE 1.6.8. 

Just `upload` to a UNO compatible Arduino.

Wiring notes are in the .ino file. See `PIR1_PIN`, `PIR2_PIN`, `RX`, `TX` if you need to change pins. On the Arduino that I was using (a Sparkfun RedBoard), the `PWM` pins would not work to read the PIR signals (thus 2 & 4).

# Development

We are using the serial protocol, so the rules about trigger-pins don't apply.
The "trigger" command in the serial protocol is really "play track n", and doesn't relate to the trigger pins, nor to the config in `MP3TRIGR.INI`.

There's info in the `serial console`.

And, the on-board LED signals the movement: blinking for *only-one-sensor*, solid for *both sensors*.

## Tuning

The `Pir::check` function *smooths out* (or debounces) the PIR signal. I found that the PIR oscillates when it triggers (goes from on->off->on->off). The length of the on vs. off is dependant on how far an object is from the PIR: closer objects cause the on length to be very long (1 second or so). The oscillations get shorter when the motion stops.

So,
* A `on` signal has to last at least `PirWaitOn` msecs to trigger `ACTIVE`.
* It won't go in-active till `PirWaitOff` msecs.

Tune those 2 constants as you like. You could make the `ACTIVE` persist longer (when motion stops) by increasing `PirWaitOff`.

I worked out a more elaborate processing, since I noticed that off/on with very short cyles (< 90msec) is pretty definitively in-active. But, that didn't seem worth it for this.

# Debugging

Watch the status LED on the mp3-board. Three quick blinks at power-up are good. The LED is solid when playing sounds. Look up the rest of the codes.

Use the rocker-control on the mp3-board to cycle through the sounds.

The `handle_commands` method implements some debugging commands. *Send* in the `serial console` to use them. Use `?` to get a menu.

There is a `Makefile` (and `menu.mk`):

* Rebuild the *print-menu* stuff in `handle_commands` method, from the case statements: `make menu`. Optional, for convenience.
* Copy the files in `mp3-trigger/` to the microsd card (also just convenience):
    * Make a link to your microsd mount point: e.g. `ln -s /mnt/mp3microsd microsd`
    * Put sound files in `mp3-trigger/`
    * `make loadsd`

And, there are some commented out `Serial` commands that were useful to me.
