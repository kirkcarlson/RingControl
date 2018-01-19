/*
want to incorporate/unify all of the commands
percent survives other commands
    - fade to black or fade to target change the overall brightness without touching the saved value
color can survive most other commands
- some scenes/modes use the set color
- other scenes/modes use their own color

does scene/mode?
off could just fade to black without changing the scene/mode
  - this would change the effect immediately
  - if scene/mode is still active, it may set effects as it desires (meteor)
  - last scene/mode can be preserved by off and used by on
on could just ramp up from the last known scene/mode

    how do you start/select a group?
        are groups predefined, so you just pick a number/name <<<easiest to do
        are group dynamically defined
            create at will
            destroy when done
    how many groups can control a particular LED
        zero -- for special case LEDs
        one -- normal
            make it easy to assign... just a group# or 255 for none
        more than one -- for layering, maybe sprites
    --need a list. assume most LED arrays use a single set of numbering 0..NUM_LEDS-1
    we want to be able to assign a group of LEDs in a rectangular array to be a clock
    we want to be able to assign a group of LEDs to be the surrounding marquee
        this means that the group defined as an array of LED numbers
            this array could be shorted to a bit map
                no it cannot... it must be ordered for marquee effects
        to access the led: groups->members->led
        This adds a level of indirection to the processing

    Can groups be dynamically reassigned
        have a set of groups for horizontal rows
        switch to a set of groups for vertical columns
        maybe a group for the periphery marquee

    is it possible to have a looping mechanism?
        something like
            GROUP_REPEAT_FOREVER,
                GROUP_ON,
                GROUP_WAIT, 200,
                GROUP_OFF,
                GROUP_WAIT, 200,
            GROUP_REPEAT_END
        or
            GROUP_REPEAT_COUNT, 6,
                GROUP_ON,
                GROUP_WAIT, 200,
                GROUP_OFF,
                GROUP_WAIT, 200,
            GROUP_REPEAT_END
        to prevent multiple instances, GROUP_REPEATx could check the loop count
            this would require zeroing the counter, it the process was interupted (or restarted)
            should not allow interpretation within the loop

need to be able to "call" subroutines
something like:

groupFlashType1:
  GROUP ON
  GROUP WAIT
  GROUP OFF
  GROUP WAIT

ledFlashType1:
  LED ON
  LED WAIT
  LED ON
  LED WAIT

DIRECTOR:
  GROUP 1 groupFlashType1
  GROUP 3 groupFlashType1
  LED 2 ledFlashType1
  GROUP WAIT 15 * 1000

to make this more symmetrical how about:

scene1
  GROUP START 1 groupFlashType1
  GROUP START 3 groupFlashType1
  LED START 2 ledFlashType1
  GROUP WAIT 3 * 1000
  LED START 2 ledFlashType2
  GROUP WAIT 3 * 1000

directorGroup1
  GROUP START 1 scene1
  GROUP WAIT 3 * 1000
  GROUP START 1 scene2
  GROUP WAIT 3 * 1000
  GROUP START 1 scene3
  GROUP WAIT 3 * 1000


need to had a transparent color to allow color to shine from lower layers
need to have color set by group, not as a whole
need to control dimming by whole and by group

transparency
  maybe think of this like a drawing
    it consists of a set of objects
    the objects are ordered from top to bottom
  in this case the objects are (for now) groups and LEDs
  the ordering can be a ordered list
    LEDs are normally at the bottom, but not necessarily so
    groups may be in any order.
 to find the color of a pixel, dive down through the objects until action one found

 think about sprites... sort of in the sense of the CodeBug sprites
   they can be one or two dimensions
   they can move from one point to another at some velocity
   two dimentional sprites maybe a string of characters

 DIRECTOR
   GROUP 1 multiColorMarquee
   GROUP 2 background
   GROUP 3 flyInStringFromRight
 DIRECTOR END
 DIRECTOR
   GROUP 1 multiColorMarquee
   GROUP 2 background2
   GROUP 3 scrollInStringFromRight
 DIRECTOR END

    some commands are for immediate execution (GROUP_COLOR, GROUP_LOAD_PATTERN)
    some commands wait for a timer (GROUP_WAIT)
    some commands pass control to the FSM (e.g. GROUP_CLOCK)

    this can and should change the group FSM state, the calling function can do more

    Or is there another philosopy at play here.
        do actions until done
        done when no more to do (STOP) or wait for timer (WAIT)
        commands that do not change state should not FastLED.show()
        commands that do change to a stable state should do a FastLED.show()

    this latter philosophy would need a loop here.
*/



// **** INCLUDES ****

//Load Fast LED Library and set it's options.
#include <FastLED.h>
FASTLED_USING_NAMESPACE;
#define PARTICLE_NO_ARDUINO_COMPATIBILITY 1


//**** CONSTANTS ****

#define TIME unsigned long
#define NUM_LEDS 24
#define FASTLED_PIN D6
#define ledBias 2 /* minimum led value during sine wave */

// divide LEDs into quadrants
#define NUM_LEDS_Q1 0
#define NUM_LEDS_Q2 (NUM_LEDS >> 2)                 //6
#define NUM_LEDS_Q3 (NUM_LEDS >> 1)                 //12
#define NUM_LEDS_Q4 (NUM_LEDS - (NUM_LEDS >> 2))    //18
#define NUM_LEDS_Q5 NUM_LEDS                        //24

// divide LEDs into eights
#define NUM_LEDS_81 0
#define NUM_LEDS_82 (NUM_LEDS >> 3)                     //3
#define NUM_LEDS_83 (NUM_LEDS >> 2)                     //6
#define NUM_LEDS_84 ((NUM_LEDS >> 2) + (NUM_LEDS >> 3)) //9
#define NUM_LEDS_85 (NUM_LEDS >> 1)                     //12
#define NUM_LEDS_86 ((NUM_LEDS >> 1) + (NUM_LEDS >> 3)) //15
#define NUM_LEDS_87 ((NUM_LEDS >> 1) + (NUM_LEDS >> 2)) //18
#define NUM_LEDS_88 (NUM_LEDS - (NUM_LEDS >> 3))        //21
#define NUM_LEDS_89 NUM_LEDS                            //24

#define NUM_GROUPS 6 // number of possible groups, tentatively 0 is all LEDs

#define CLOCK_EFFECT_TIME 100 // how often to check on clock effect

const int RED = (255<<16) + (  0<<8) +   0;
const int YELLOW = (255<<16) + (255<<8) +   0;
const int WHITE = (255<<16) + (255<<8) + 255;
const int BLUE = (  0<<16) + (  0<<8) + 255;
const int GREEN = (  0<<16) + (128<<8) +   0;
const int ORANGE = (255<<16) + (165<<8) +   0;
const int VIOLET = (238<<16) + (130<<8) + 238;
const int BLACK = (  0<<16) + (  0<<8) +   0;


enum LEDModes {
    offMode,
    demo1,
    singleColor,
    staticMultiColor,
    twinklingMultiColor,
    staticRainbow,
    staticWheel,
    colorWave,
    clockFace,
    marqueeLeft,
    marqueeRight,
    allFlash,
    quadFlasher,
    quadFlasher2,
    quadFlasher3,
    quadFlasher4,
    quadFlasher5,
    quadFlasher6,
    quadFlasher7,
    octaFlasher,
    octaFlasher2,
    circulator,
    pendulum,
    breathe,
    flashSynched,
    flashPhased,
    winkSynched,
    winkPhased,
    blinkSynched,
    blinkPhased,
    meteorSynched,
    meteorPhased,
    NUM_LED_MODES
};

String ledModeNames []= {
    "offMode",
    "demo1",
    "singleColor",
    "staticMultiColor",
    "twinklingMultiColor",
    "staticRainbow",
    "staticWheel",
    "colorWave",
    "clockFace",
    "marqueeLeft",
    "marqueeRight",
    "allFlash",
    "quadFlasher",
    "quadFlasher2",
    "quadFlasher3",
    "quadFlasher4",
    "quadFlasher5",
    "quadFlasher6",
    "quadFlasher7",
    "octaFlasher",
    "octaFlasher2",
    "circulator",
    "pendulum",
    "breathe",
    "flashSynched",
    "flashPhased",
    "winkSynched",
    "winkPhased",
    "blinkSynched",
    "blinkPhased",
    "meteorSynched",
    "meteorPhased",
};

enum ledEvent {
    LED_TIME_OUT,
    LED_ON,
    LED_OFF,
    LED_ON_COLOR, // color
    LED_FADE_TO_BLACK, //duration
    LED_FADE_TO_COLOR, //duration, color
    LED_WAIT, // duration
    LED_LOOP,
    LED_STOP
};

enum groupEvent {
    GROUP_TIME_OUT,
    GROUP_ON,
    GROUP_ON_COLOR,
    GROUP_OFF,
    GROUP_ON_MULTI,
    GROUP_COLOR,
    GROUP_COLOR_SEGMENT,
    GROUP_LOAD_PATTERN,
    GROUP_RAINBOW,
    GROUP_WHEEL,
    GROUP_WAVE,
    GROUP_TWINKLE,
    GROUP_BREATHE,
    GROUP_ROTATE_RIGHT,
    GROUP_ROTATE_LEFT,
    GROUP_ROTATE_RIGHT_COUNT,
    GROUP_ROTATE_LEFT_COUNT,
    GROUP_REPEAT_FOREVER,
    GROUP_REPEAT_COUNT,
    GROUP_REPEAT_END,
    GROUP_CLOCK,
    GROUP_CHASE_CW_PAIR,
    GROUP_WAIT,
    GROUP_FADE_TO_BLACK,
    GROUP_FADE_TO_COLOR,
    GROUP_GCOLOR,
    GROUP_GFORK,
    GROUP_SET_GROUP,
    GROUP_SET_ALL,
    GROUP_SET_MEMBERS,
    GROUP_PHASE_DELAY,
    GROUP_LFORK,
    GROUP_LFORK_ALL,
    GROUP_LKILL,
    GROUP_LKILL_ALL,
    GROUP_LOOP,
    GROUP_STOP
};


// individual LED command arrays
// these loop unless directed not to with LED_STOP
int flash250 [] = {
    LED_ON,
    LED_WAIT, 250,
    LED_OFF,
    LED_WAIT, 250,
    LED_LOOP
};

int wink [] = {
    LED_ON,
    LED_WAIT, 2500,
    LED_OFF,
    LED_WAIT, 100,
    LED_LOOP
};

int blink [] = {
    LED_OFF,
    LED_WAIT, 2500,
    LED_ON,
    LED_WAIT, 100,
    LED_LOOP
};

int meteor [] = {
    LED_ON,
    LED_WAIT, 30,
    LED_FADE_TO_BLACK, 80,
    LED_OFF,
    LED_WAIT, 1000,
    LED_LOOP
};


// group LED command arrays
// these loop unless directed not to with GROUP_STOP
int offGC [] = {
    GROUP_OFF,
    GROUP_STOP
};

int singleColorGC [] = {
    GROUP_ON,
    GROUP_STOP
};

int multiColorGC [] = {
    GROUP_ON_MULTI,
    GROUP_STOP
};

int multiColorTwinkleGC [] = {
    GROUP_ON_MULTI,
    GROUP_TWINKLE,
    GROUP_STOP
};

int allFlashGC [] = {
    GROUP_COLOR, RED,
    GROUP_ON,
    GROUP_WAIT, 250,
    GROUP_OFF,
    GROUP_WAIT, 250,
    GROUP_COLOR, BLUE,
    GROUP_ON,
    GROUP_WAIT, 250,
    GROUP_OFF,
    GROUP_WAIT, 250,
    GROUP_LOOP
};


int quadFlashGC [] = {
    GROUP_LOAD_PATTERN, 24,
                        RED,RED,RED,RED,RED,RED,
                        BLACK,BLACK,BLACK,BLACK,BLACK,BLACK,
                        RED,RED,RED,RED,RED,RED,
                        BLACK,BLACK,BLACK,BLACK,BLACK,BLACK,
    GROUP_WAIT, 250,
    GROUP_LOAD_PATTERN, 24,
                        BLACK,BLACK,BLACK,BLACK,BLACK,BLACK,
                        RED,RED,RED,RED,RED,RED,
                        BLACK,BLACK,BLACK,BLACK,BLACK,BLACK,
                        RED,RED,RED,RED,RED,RED,
    GROUP_WAIT, 250,
    GROUP_LOOP
};


int quadFlashGC2 [] = {
    GROUP_LOAD_PATTERN, 24,
                        RED,RED,RED,RED,RED,RED,
                        BLACK,BLACK,BLACK,BLACK,BLACK,BLACK,
                        RED,RED,RED,RED,RED,RED,
                        BLACK,BLACK,BLACK,BLACK,BLACK,BLACK,
    GROUP_WAIT, 250,
    GROUP_ROTATE_RIGHT,
    GROUP_ROTATE_RIGHT,
    GROUP_ROTATE_RIGHT,
    GROUP_ROTATE_RIGHT,
    GROUP_ROTATE_RIGHT,
    GROUP_ROTATE_RIGHT,
    GROUP_WAIT, 250,
    GROUP_LOOP
};


int quadFlashGC3 [] = {
    GROUP_LOAD_PATTERN, 24,
                        RED,RED,RED,RED,RED,RED,
                        BLACK,BLACK,BLACK,BLACK,BLACK,BLACK,
                        RED,RED,RED,RED,RED,RED,
                        BLACK,BLACK,BLACK,BLACK,BLACK,BLACK,
    GROUP_WAIT, 250,
    GROUP_ROTATE_RIGHT_COUNT, NUM_LEDS/4,
    GROUP_WAIT, 250,
    GROUP_LOOP
};



int quadFlashGC4 [] = {
    GROUP_COLOR_SEGMENT, RED, 0, NUM_LEDS/4-1,
    GROUP_COLOR_SEGMENT, BLACK, NUM_LEDS/4, NUM_LEDS/2-1,
    GROUP_COLOR_SEGMENT, RED, NUM_LEDS/2, 3*NUM_LEDS/4-1,
    GROUP_COLOR_SEGMENT, BLACK, 3*NUM_LEDS/4, NUM_LEDS-1,
    GROUP_WAIT, 250,
    GROUP_COLOR_SEGMENT, BLACK, 0, NUM_LEDS/4-1,
    GROUP_COLOR_SEGMENT, RED, NUM_LEDS/4, NUM_LEDS/2-1,
    GROUP_COLOR_SEGMENT, BLACK, NUM_LEDS/2, 3*NUM_LEDS/4-1,
    GROUP_COLOR_SEGMENT, RED, 3*NUM_LEDS/4, NUM_LEDS-1,
    GROUP_WAIT, 250,
    GROUP_LOOP
};


int quadFlashGC5 [] = {
    GROUP_COLOR_SEGMENT, RED, 0, NUM_LEDS/4-1,
    GROUP_COLOR_SEGMENT, BLACK, NUM_LEDS/4, NUM_LEDS/2-1,
    GROUP_COLOR_SEGMENT, RED, NUM_LEDS/2, 3*NUM_LEDS/4-1,
    GROUP_COLOR_SEGMENT, BLACK, 3*NUM_LEDS/4, NUM_LEDS-1,
    GROUP_WAIT, 250,
    GROUP_ROTATE_RIGHT_COUNT, NUM_LEDS/4,
    GROUP_WAIT, 250,
    GROUP_LOOP
};


int quadFlashGC6 [] = {
    GROUP_COLOR_SEGMENT, BLACK, 0, NUM_LEDS-1,
    GROUP_COLOR_SEGMENT, RED, 0, NUM_LEDS/4-1,
    GROUP_COLOR_SEGMENT, RED, NUM_LEDS/2, 3*NUM_LEDS/4-1,
    GROUP_WAIT, 250,
    GROUP_CHASE_CW_PAIR, NUM_LEDS/4-1, NUM_LEDS/2-1, NUM_LEDS/4, 30, //fromLED, toLED, count, delay
    GROUP_WAIT, 250,
    GROUP_CHASE_CW_PAIR, NUM_LEDS/2-1, 3*NUM_LEDS/4-1, NUM_LEDS/4, 30, //fromLED, toLED, count, delay
    GROUP_LOOP
};


int quadFlashGC7 [] = {
    GROUP_LOAD_PATTERN, 24,
                        RED,WHITE,BLUE,BLUE,WHITE,RED,
                        BLACK,BLACK,BLACK,BLACK,BLACK,BLACK,
                        RED,WHITE,BLUE,BLUE,WHITE,RED,
                        BLACK,BLACK,BLACK,BLACK,BLACK,BLACK,
    GROUP_WAIT, 250,
    GROUP_CHASE_CW_PAIR, NUM_LEDS/4-1, NUM_LEDS/2-1, NUM_LEDS/4, 30, //fromLED, toLED, count, delay
    GROUP_WAIT, 250,
    GROUP_CHASE_CW_PAIR, NUM_LEDS/2-1, 3*NUM_LEDS/4-1, NUM_LEDS/4, 30, //fromLED, toLED, count, delay
    GROUP_LOOP
};


int octaFlashGC [] = {
    GROUP_LOAD_PATTERN, 24,
                        RED,RED,RED,BLACK,BLACK,BLACK,
                        RED,RED,RED,BLACK,BLACK,BLACK,
                        RED,RED,RED,BLACK,BLACK,BLACK,
                        RED,RED,RED,BLACK,BLACK,BLACK,
    GROUP_WAIT, 250,
    GROUP_ROTATE_RIGHT_COUNT, NUM_LEDS/8,
    GROUP_WAIT, 250,
    GROUP_LOOP
};


int octaFlashGC2 [] = {
    GROUP_COLOR_SEGMENT, BLACK, 0, NUM_LEDS-1,
    GROUP_COLOR_SEGMENT, RED, 0,              NUM_LEDS/8-1,
    GROUP_COLOR_SEGMENT, RED, 2*NUM_LEDS/8, 3*NUM_LEDS/8-1,
    GROUP_COLOR_SEGMENT, RED, 4*NUM_LEDS/8, 5*NUM_LEDS/8-1,
    GROUP_COLOR_SEGMENT, RED, 6*NUM_LEDS/8, 7*NUM_LEDS/8-1,
    GROUP_WAIT, 250,
    GROUP_ROTATE_RIGHT_COUNT, NUM_LEDS/8,
    GROUP_WAIT, 250,
    GROUP_LOOP
};


int rainbowGC [] = {
    GROUP_RAINBOW,
    GROUP_STOP
};

int wheelGC [] = {
    GROUP_WHEEL,
    GROUP_STOP
};

int breatheGC [] = {
    GROUP_BREATHE,
    GROUP_STOP
};

int waveGC [] = {
    GROUP_WAVE,
    GROUP_WAIT, 30,
    GROUP_LOOP
};

int marqueeLeftGC [] = {
    GROUP_ROTATE_LEFT,
    GROUP_WAIT, 200,
    GROUP_LOOP
};

int marqueeRightGC [] = {
    GROUP_ROTATE_RIGHT,
    GROUP_WAIT, 200,
    GROUP_LOOP
};

int clockGC [] = {
    GROUP_CLOCK,
    GROUP_STOP
};

int circulatorGC [] = {
    GROUP_COLOR_SEGMENT, BLACK, 0, NUM_LEDS-1,
    GROUP_COLOR_SEGMENT, YELLOW, NUM_LEDS/2, NUM_LEDS/2+4,
    GROUP_REPEAT_COUNT, NUM_LEDS - 5,
        GROUP_ROTATE_RIGHT,
        GROUP_WAIT, 40,
    GROUP_REPEAT_END,
    GROUP_REPEAT_COUNT, 5,
        GROUP_ROTATE_RIGHT,
        GROUP_WAIT, 200,
    GROUP_REPEAT_END,
    GROUP_LOOP
};

int pendulumGC [] = {
    GROUP_COLOR_SEGMENT, BLACK, 0, NUM_LEDS-1,
    GROUP_COLOR_SEGMENT, YELLOW, NUM_LEDS/2, NUM_LEDS/2+4,
    GROUP_REPEAT_COUNT, 5,
        GROUP_ROTATE_LEFT,
        GROUP_WAIT, 100,
    GROUP_REPEAT_END,
    GROUP_REPEAT_COUNT, 5,
        GROUP_ROTATE_RIGHT,
        GROUP_WAIT, 100,
    GROUP_REPEAT_END,
    GROUP_LOOP
};


int demo1GC [] = {
    //GROUP_SET_GROUP, 1, // set this group to 1, not sure if this is a good thing to do
    GROUP_GCOLOR, 0, BLUE,
    GROUP_GFORK, 0, (int)&singleColorGC,
    GROUP_WAIT, 3000,

    GROUP_GCOLOR, 0, YELLOW,
    GROUP_GFORK, 0, (int)&singleColorGC,
    GROUP_WAIT, 3000,

    GROUP_GCOLOR, 0, GREEN,
    GROUP_GFORK, 0, (int)&singleColorGC,
    GROUP_WAIT, 3000,

    GROUP_GFORK, 0, (int)&allFlashGC,
    GROUP_WAIT, 3000,

    GROUP_GFORK, 0, (int)&quadFlashGC6,
    GROUP_WAIT, 3000,

    GROUP_GFORK, 0, (int)&quadFlashGC7,
    GROUP_WAIT, 3000,

    GROUP_GFORK, 0, (int)&pendulumGC,
    GROUP_WAIT, 3000,

    GROUP_GFORK, 0, (int)&circulatorGC,
    GROUP_WAIT, 3000,

    GROUP_GFORK, 0, (int)&clockGC,
    GROUP_WAIT, 3000,

    GROUP_GFORK, 0, (int)&marqueeRightGC,
    GROUP_WAIT, NUM_LEDS * 200,

    GROUP_GFORK, 0, (int)&marqueeLeftGC,
    GROUP_WAIT, NUM_LEDS * 200,

    GROUP_GCOLOR, 0, BLUE,
    GROUP_GFORK, 0, (int)&singleColorGC,
    GROUP_GFORK, 0, (int)&breatheGC,
    GROUP_WAIT, 3000,

    GROUP_SET_ALL,
    GROUP_PHASE_DELAY, 30,
    GROUP_LFORK_ALL, (int)&flash250,
    GROUP_WAIT, 3000,
    GROUP_LKILL_ALL,

    GROUP_GCOLOR, 0, BLACK,
    GROUP_GFORK, 0, (int)&singleColorGC,

    // the folloning changes the members of the director group dynamically
    // I don't think that was the intent of the GROUP_SET_MEMBERS function.
    // This was intended to be on group-group rather than a director-group.
    // Let's see if it works.
    GROUP_SET_MEMBERS, 12, 0,1,2,3,4,5,6,7,8,9,10,11,
    GROUP_PHASE_DELAY, 30,
    GROUP_LFORK_ALL, (int)&meteor,
    GROUP_SET_MEMBERS, 12, 12,13,14,15,16,17,18,19,20,21,22,23,
    GROUP_PHASE_DELAY, 30,
    GROUP_LFORK_ALL, (int)meteor,
    GROUP_WAIT, 3000,
    GROUP_SET_ALL,  //need to set context to all LEDs, before killing them
    GROUP_LKILL_ALL,
    GROUP_LOOP
};


//**** TYPEDEFs ****

// LedState is the state information of indivually controlled LEDs
struct LedState {
    int ledNumber;              // number of LED being controlled
    int *currentCommandArray;   // pointer to array of commands
    int currentCommandIndex;    // index into current array of commands
    int (*currentState)( LedState*, ledEvent, TIME);
    uint8_t fadeStepNumber; // color is proportioned 255*(fadeSteps - fadeStepNumber)/fadeSteps
    uint8_t fadeSteps;      // total number of steps in fade
    CRGB fromColor;
    CRGB toColor;
    TIME startTime; // time upon entering the current state
    unsigned int timeToWait;           // time to remain in the current state
};


struct LedGroupState {
    int groupNumber;        // number of LED group being controlled
    int *currentCommandArray;   // pointer to array of commands
    int currentCommandIndex;    // index into current array of commands
    int fromLED;
    int toLED;
    int onLED;
    int waveCount;//within wave effect
    int count;//within a REPEAT
    int repeatIndex;    // index into current array of commands for start of repeat
    uint8_t fadeStepNumber; // color is proportioned 255*(fadeSteps - fadeStepNumber)/fadeSteps
    uint8_t fadeSteps;      // total number of steps in fade
    uint8_t breatheIndex; // state of breathing for group
    uint8_t breathStep;   // amount to step index for each period
    CRGB fromColor; // fading from color
    CRGB toColor;   // fading to color
    CRGB color;
    CRGB pattern [NUM_LEDS]; // array of colors
    int members [NUM_LEDS];    // ordered array of LED numbers in group
    int numLeds;            // number of LED members in group
    int (*currentState)( LedGroupState*, groupEvent, TIME);
    TIME startTime; // time upon entering the current state
    unsigned int timeToWait;           // time to remain in the current state
};


//**** GLOBALS ****

CRGB leds[NUM_LEDS]; // main LED array which is directly written to the LEDs

int ledBrightness; // level of over all brightness desired 0..100 percent
int LEDPoint = 0; // 0..23 LED direction for selected modes
//int LEDSpeed = 0; // 0..100 speed of LED display mode
CRGB ledColor = CRGB::White; // default color of individual LEDs
int selectedGroup = 0; // currently selected group

LEDModes LEDMode = offMode; // LED display mode, see above


LedState ledStates [ NUM_LEDS];
LedGroupState ledGroupStates [NUM_GROUPS];


//**** FORWARD REFERENCES ****

int groupNullSPF ( LedGroupState *group, groupEvent event, TIME currentTime);
int groupStopSPF ( LedGroupState *group, groupEvent event, TIME currentTime);
int ledNullSPF ( LedState *state, ledEvent event, TIME currentTime);
void ledAllStatesSPF( LedState *state, ledEvent event, TIME currentTime); // handles messages for all states
void groupAllStatesSPF( LedGroupState *group, groupEvent event, TIME currentTime);


// **** FUNCTIONS ****

void setupAllLedFSMs ()
{
    for (int i = 0; i < NUM_LEDS; i++)
    {
        ledStates[i].ledNumber = i;
    }
}


void stopAllLedFSMs ()
{
    for (int i = 0; i < NUM_LEDS; i++)
    {
        ledStates[i].currentState = ledNullSPF;
        ledStates[i].timeToWait = 0;
    }
}


void setupAllLedGroupFSMs ()
{
    CRGB color = CRGB::White;
    for (int i = 0; i < NUM_GROUPS; i++)
    {
        ledGroupStates[i].groupNumber = i;
        ledGroupStates[i].color = (color.r<<16) + (color.g<<8) + color.b;
        //Serial.printf("setup i:%d FSM:%X\r\n", i, (ledGroupStates[i].color.r<<16)+(ledGroupStates[i].color.g<<8)+ledGroupStates[i].color.b);
    }
}


void stopAllLedGroupFSMs()
{
    for (int i = 0; i < NUM_GROUPS; i++)
    {
        ledGroupStates[i].currentState = groupNullSPF;
        ledGroupStates[i].timeToWait = 0;
    }
}


NSFastLED::CRGB makeColor (uint8_t red, uint8_t green, uint8_t blue); // voodo to apease compiler gods

NSFastLED::CRGB makeColor (uint8_t red, uint8_t green, uint8_t blue)
{
    CRGB newColor;
    newColor.r = red;
    newColor.g = green;
    newColor.b = blue;
    return newColor;
}


void fillLedColor( NSFastLED::CRGB color)
{
    for (int i = 0; i < NUM_LEDS; i++) {
        ////Serial.printf ("fillLedColor %d R%d G%d B%d\r\n", i, color.r, color.g, color.b);
        leds[i] = color;
    }
}


void partialFillLEDcolor( int start, int end, NSFastLED::CRGB color)
{
    for (int i = start; i < end; i++) {
        leds[i] = color;
    }
}


void fillLedMultiColor ()
{
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i].r = rand() % 256;
        leds[i].g = rand() % 256;
        leds[i].b = rand() % 256;
    }
}


NSFastLED::CRGB wheel(uint8_t wheelPos); // voodo to apease compiler gods

NSFastLED::CRGB wheel(uint8_t wheelPos) //from Adafruit and modified
/*
  WheelPos 0..255 position about a circle (like 2Pi radians)
*/
{
    //WheelPos = 255 - WheelPos;
    if(wheelPos < 85) // first third of wheel
    {
        // red falling, no green, blue rising
        return makeColor(255- wheelPos * 3, 0, wheelPos * 3);
    }
    else if(wheelPos < 170) // second third of wheel
    {
        wheelPos -= 85;
        // no red, green rising, blue falling
        return makeColor(0, wheelPos * 3, 255 - wheelPos * 3);
    }
    else // last third of wheel
    {
        wheelPos -= 170;
        // red rising, green falling, no blue
        return makeColor(wheelPos * 3, 255 - wheelPos * 3, 0);
    }
}


void fillWheel()
// spreading a color wheel across all of the LEDS
{
    CRGB col;
    for (int i = 0; i < NUM_LEDS; i++)
    {
        col = wheel( i * 255 / NUM_LEDS);
        //Serial.printf("fillWheel %d %d R%d G%d B%d\r\n", i, i * 255 / NUM_LEDS, col.r, col.g, col.b);
        leds[i] = wheel( i * 255 / NUM_LEDS);
    }
}


void rotateRight ()
//rotate all of the LEDs to the right
{
    CRGB saveLed = leds[ NUM_LEDS-1];
    for (int i = NUM_LEDS - 1; i>0; i--)
    {
        leds[i] = leds[i-1];
    }
    leds [0] = saveLed;
}


void rotateLeft ()
//rotate all of the LEDs to the left
{
    CRGB saveLed = leds[ 0];
    for(int i = 0; i < NUM_LEDS - 1; i++)
    {
        leds[ i] = leds[ i+1];
    }
    leds[ NUM_LEDS - 1] = saveLed;
}


void printStateInfo( LedState *state)
{
    Serial.printf("Led:%d state:%d commandArray:%d commandIndex:%d timeToWait:%d\r\n",
            state->ledNumber,
            state->currentState,
            state->currentCommandArray,
            state->currentCommandIndex,
            state->timeToWait);
}


void printGroupStateInfo( LedGroupState *group)
{
    Serial.printf( "Group info group:%d command:%d index:%d state%d timeToWait:%d time:%d\r\n",
            group->groupNumber,
            group->currentCommandArray,
            group->currentCommandIndex,
            group->currentState,
            group->timeToWait,
            millis());
}


unsigned int getNextGroupCommand ( LedGroupState *group)
// returns the next LED command for a particular state and updates pointers
{
    printGroupStateInfo( group);
    unsigned int command = group->currentCommandArray[ group->currentCommandIndex];
    Serial.printf("getNextGroupCommand %d\r\n", command);
    group->currentCommandIndex++;
    if (group->currentCommandArray[ group->currentCommandIndex] == GROUP_LOOP)
    {
        Serial.printf("getNextGroupCommand looping\r\n", group->currentCommandIndex);
        group->currentCommandIndex = 0;
    }
    //printStateInfo( state);
    return command;
}


unsigned int getNextLedCommand ( LedState *state)
// returns the next LED command for a particular state and updates pointers
{
    //printStateInfo( state);
    unsigned int command = state->currentCommandArray[ state->currentCommandIndex];
    //Serial.printf( "getNextLedCommand %d\r\n", command);
    state->currentCommandIndex++;
    if (state->currentCommandArray[ state->currentCommandIndex] == LED_LOOP)
    {
        state->currentCommandIndex = 0;
    }
    //printStateInfo( state);
    return command;
}


void interpretNextLedCommand ( LedState *state, TIME currentTime)
// also interprets commands and parameters
{
    state->currentState = ledNullSPF;
    while (state->currentState == ledNullSPF)
    //this won't work when you want to do a GROUP STOP
    // maybe should test for looping
    {
        //Serial.printf( "iNLC getting another command\r\n");
        ledAllStatesSPF (state, (ledEvent)getNextLedCommand( state), currentTime);
    }
}


int ledFadingSPF ( LedState *state, ledEvent event, TIME currentTime)
{
    // fade a bit more toward target
    int fadeFactor = (state->fadeSteps - state->fadeStepNumber)*
            255 / state->fadeSteps;
    if (state->toColor) // not black
    {
        leds[ state->ledNumber] = blend (state->fromColor, state->toColor,
                fadeFactor/255);
    }
    else // black
    {
        leds[ state->ledNumber].r = state->fromColor.r * fadeFactor/255;
        leds[ state->ledNumber].g = state->fromColor.g * fadeFactor/255;
        leds[ state->ledNumber].b = state->fromColor.b * fadeFactor/255;
    }
    //Serial.printf( "Fading %d %d\r\n", state->fadeStepNumber,
    //    fadeFactor);
    state->fadeStepNumber++;
    if (state->fadeStepNumber == state->fadeSteps)
    {
        interpretNextLedCommand( state, currentTime); // set up next state
    }
    else
    {
         ledStates[ state->ledNumber].startTime = currentTime;
        // go around again
    }
        return 0;
}


int ledWaitSPF ( LedState *state, ledEvent event, TIME currentTime)
{
    interpretNextLedCommand( state, currentTime); // set up next state
    return 0;
}

int ledNullSPF ( LedState *state, ledEvent event, TIME currentTime)
{
    return 0;
}


void ledAllStatesSPF( LedState *state, ledEvent event, TIME currentTime) // handles messages for all states
// also interprets commands and parameters
{
    //Serial.printf( "ledAllStatesSPF processing another event %d led:%d state:%d\r\n",
    //        (int)event, state->ledNumber, (int)state->currentState);
    switch( event)
    {
        case LED_ON:
            Serial.printf("LED ON command led:%d time%d\r\n", state->ledNumber, currentTime);
            leds[ state->ledNumber] = ledColor;
            state->timeToWait = 0; //disable timer
            state->currentState = ledNullSPF;
            break;

        case LED_ON_COLOR: // color
            //Serial.printf("LED ON COLOR command led:%d time%d\r\n", state->ledNumber, currentTime);
            leds[ state->ledNumber] = getNextLedCommand( state);
            state->timeToWait = 0; //disable timer
            state->currentState = ledNullSPF;
            break;

        case LED_OFF:
            Serial.printf("LED OFF command led:%d time%d\r\n", state->ledNumber, currentTime);
            leds[ state->ledNumber] = CRGB::Black;
            state->timeToWait = 0; //disable timer
            state->currentState = ledNullSPF;
            break;

        case LED_WAIT: // ms
            state->startTime = currentTime;
            state->timeToWait = getNextLedCommand( state);
            Serial.printf("LED LED command led:%d time:%d wait:%d\r\n",
                    state->ledNumber, currentTime, state->timeToWait);
            state->currentState = ledWaitSPF;
            break;

        case LED_FADE_TO_BLACK: // ms
        {
            //Serial.println("LED FADE TO BLACK command");
            state->startTime = currentTime;
            state->fadeStepNumber = 0;
            unsigned int duration = getNextLedCommand( state);
            state->fadeSteps = min( duration, 255);
            state->timeToWait = max( 1, state->fadeSteps/255);
            state->fromColor = leds[ state->ledNumber];
            state->toColor = CRGB::Black;
            state->currentState = &ledFadingSPF;
            break;
        }

        case LED_FADE_TO_COLOR: // ms color
        {
            //Serial.println("LED FADE TO COLOR command");
            state->startTime = currentTime;
            state->fadeStepNumber = 0;
            unsigned int duration = getNextLedCommand( state);
            state->fadeSteps = min( duration, 255);
            state->timeToWait = max( 1, state->fadeSteps/255);
            state->fromColor = leds[ state->ledNumber];
            state->toColor = getNextLedCommand( state);
            state->currentState = &ledFadingSPF;
            break;
        }

        case LED_STOP:
            //Serial.println("LED STOP command");
            state->currentState = ledNullSPF;
            state->timeToWait = 0; // disable timer
            break;
    }
    //Serial.printf( "ledAllStatesSPF bottom %d led:%d state:%d\r\n",
    //        (int)event, state->ledNumber, (int)state->currentState);
}


void startLedFSM( LedState *state, int *commandArray)
//this replaces LED_ENTRY and ledEntrySPF...
{
    //set state command array to commandArray
    state->currentCommandArray = commandArray;
    state->currentCommandIndex = 0;

    // exeucute the first command of that commandArray
    interpretNextLedCommand ( state, millis());
}


void groupRotateRight( LedGroupState *group)
{
    CRGB saveLed = leds[ group->members[ group->numLeds - 1] ];
    for (int i = group->numLeds - 1; i >= 1; i--)
    {
        leds[ group->members[i]] = leds[ group->members[i] - 1];
    }
    leds[ group->members[0]] = saveLed;
}

void groupRotateLeft( LedGroupState *group)
{
    int groupCount = group->numLeds;
    CRGB saveLed = leds[ group->members[ 0]];
    for (int i = 0; i < groupCount - 1; i++)
    {
        leds[ group->members[i]] = leds[ group->members[i] + 1];
    }
    leds [group->members[ groupCount - 1]] = saveLed;
}


void interpretNextGroupCommand (LedGroupState *group, TIME currentTime)
{
    group->currentState = &groupNullSPF;
    while (group->currentState == groupNullSPF)
//this won't work when you want to do a GROUP STOP
// maybe should test for looping
    {
        //Serial.printf( "groupSPF getting another command\r\n");
        groupEvent event = (groupEvent) getNextGroupCommand( group);
        groupAllStatesSPF( group, event, currentTime);
    }
}


//not referenced!
void startGroupFSM( LedGroupState *group, int *commandArray)
//this replaces GROUP_ENTRY and groupEntrySPF...
{
    //set state command array to commandArray
    group->currentCommandArray = commandArray;
    group->currentCommandIndex = 0;

    // exeucute the first command of that commandArray
    groupAllStatesSPF( group, (groupEvent)getNextGroupCommand( group), millis());
}


void interpretGroupCommand ( LedGroupState *group, int *commands, TIME currentTime)
{
    group->currentCommandArray = commands;
    group->currentCommandIndex = 0;
    interpretNextGroupCommand ( group, currentTime);
}


int groupFadingSPF ( LedGroupState *group, groupEvent event, TIME currentTime)
{
    // fade a bit more toward target
    int fadeFactor = (group->fadeSteps - group->fadeStepNumber) * 255 /
            group->fadeSteps;
    if (group->toColor) // not black
    {
        for (int j = 0; j < group->numLeds; j++)
        {
            leds[ group->members[j]] = blend (group->fromColor, group->toColor,
                    fadeFactor/255);
        }
    }
    else // black
    {
        for (int j = 0; j < group->numLeds; j++)
        {
            leds[ group->members[j]].r = group->fromColor.r * fadeFactor/255;
            leds[ group->members[j]].g = group->fromColor.g * fadeFactor/255;
            leds[ group->members[j]].b = group->fromColor.b * fadeFactor/255;
        }
    }
    //Serial.printf( "Fading %d %d\r\n", ledStates[i].fadeStepNumber,
    //    fadeFactor);
    group->fadeStepNumber++;
    if (group->fadeStepNumber == group->fadeSteps)
    {
        interpretNextGroupCommand( group, currentTime); // set up next state
    }
    else
    {
        group->startTime = currentTime;
        // go around again
    }
    return 0;
}


int groupClockSPF( LedGroupState *group, groupEvent event, TIME currentTime)
{
    float hour = ((Time.hour() % 12) + Time.minute() / 60 + .5) *
            group->numLeds / 12;
    float minute = (Time.minute() + Time.second() / 60 + .5) *
            group->numLeds / 60;
    float second = (Time.second() * group->numLeds + .5)  / 60;
    Serial.printf( "Clock %0.1f:%0.1f:%0.1f %d\r\n", hour, minute, second, (int) millis()%1000);
    Serial.printlnf( "groupClockSPF id:%d time%d", group->groupNumber, millis());
    //draw the hands
    fillLedColor( CRGB::Black);
    leds[ group->members[ ((int) hour + 1) % group->numLeds]] = CRGB::Red;
    leds[ group->members[ (int) hour]] = CRGB::Red;
    leds[ group->members[ ((int) hour - 1) % group->numLeds]] = CRGB::Red;
    leds[ group->members[ (int) minute]] = CRGB::Green;
    leds[ group->members[ ((int) minute - 1) % group->numLeds]] = CRGB::Green;
    // flash the seconds hand every second
    if (millis() % 1000 < 1000-2*CLOCK_EFFECT_TIME) // just look at the milliseconds part
    {
        leds[ group->members[ (int) second]] = CRGB::Blue;
    }
    group->startTime = currentTime;
    FastLED.show();
    return 0;
}


int groupTwinklingSPF( LedGroupState *group, groupEvent event, TIME currentTime)
{
    // turn on last LED
    leds[group->onLED] = group->fromColor;
    // select a new LED to turn off
    group->onLED = random( group->numLeds);
    group->fromColor = leds[ group->onLED];
    leds[ group->onLED] = CRGB::Black;

    group->timeToWait = random( 100, 500);
    group->startTime = currentTime;
    FastLED.show();
    return 0;
}


int groupBreathingSPF( LedGroupState *group, groupEvent event, TIME currentTime)
{
    //Serial.printf( "GROUP STATE BREATING index:%d\r\n", group->breatheIndex);
    FastLED.setBrightness( (int) min(255, ((ledBrightness *
            quadwave8( group->breatheIndex) / 100)+ledBias)));
    group->breatheIndex = group->breatheIndex + group->breathStep;
    group->startTime = currentTime;
    FastLED.show();
    return 0;
}


int groupRotatingRightSPF( LedGroupState *group, groupEvent event, TIME currentTime)
{
    groupRotateRight( group);
    group->startTime = currentTime;
    FastLED.show();
    return 0;
}


int groupRotatingLeftSPF( LedGroupState *group, groupEvent event, TIME currentTime)
{
    groupRotateLeft( group);
    group->startTime = currentTime;
    FastLED.show();
    return 0;
}

int groupChasePairRightSPF( LedGroupState *group, groupEvent event, TIME currentTime)
{
    // source, destination, count
    // first LED moved from source to destination, one pixel at a time
    // next LED moved from from source-1 to destination-1 one pixel at a time
    // ... until count LEDs are moved
    //Serial.printf( "Chase count: %d from:%d to:%d on:%d\r\n",
    //    group->count,
    //    group->fromLED,
    //    group->toLED,
    //    group->onLED
    //);
    //move one led on path toward destination
    if (group->onLED == group->toLED)
    {
        // done moving this LED
        group->count = group->count - 1;
        //Serial.printf( "Chasing i:%d\r\n", group->count);
        printGroupStateInfo( group);
        if (group->count == 0) //done moving
        {
            // execute the next group command
            group->startTime = currentTime;
            group->timeToWait = 0;
            group->currentState = groupNullSPF;
            printGroupStateInfo( group);
            interpretNextGroupCommand( group, currentTime); // set up next state
        }
        else
        {
            // back fromLED and toLED for the next LED to move
            group->fromLED = (group->fromLED - 1 + group->numLeds) %
                    group->numLeds;
            group->toLED = (group->toLED - 1 + group->numLeds) %
                    group->numLeds;
            //move the first LED of this group
            group->onLED = group->fromLED;
            group->fromColor = leds[group->onLED];
            leds[ group->onLED] = CRGB::Black;
            leds[ (group->onLED + (group->numLeds>>1)) %
                    group->numLeds] = CRGB::Black;
            group->onLED = (group->onLED + 1 + group->numLeds) % group->numLeds;
            leds[ group->onLED] = group->fromColor;
            leds[ (group->onLED + group->numLeds/2) % group->numLeds] =
                    group->fromColor;
        }
    }
    else // keep on moving the onLED
    {
        group->fromColor = leds[group->onLED];
        leds[ group->onLED] = CRGB::Black;
        leds[ (group->onLED + group->numLeds/2) % group->numLeds ] = CRGB::Black;

        group->onLED = (group->onLED + 1) % group->numLeds;
        leds[ group->onLED] = group->fromColor;
        leds[ (group->onLED + group->numLeds/2) % group->numLeds] =
                group->fromColor;
    }
    group->startTime = currentTime;
    FastLED.show();
    return 0;
}



int groupWaitSPF ( LedGroupState *group, groupEvent event, TIME currentTime)
{
    //Serial.printlnf("groupWaitSPF time:%d group:%d currentTime:%d",
    //        millis(), group->groupNumber, currentTime);
    interpretNextGroupCommand( group, currentTime); // set up next state
    return 0;
}


int groupStopSPF ( LedGroupState *group, groupEvent event, TIME currentTime)
{
    return 0;
}


int groupNullSPF ( LedGroupState *group, groupEvent event, TIME currentTime)
{
    return 0;
}


void groupAllStatesSPF( LedGroupState *group, groupEvent event, TIME currentTime)
{
    //Serial.printf( "groupSPF processing another command\r\n");
    switch (event)
    {
        case GROUP_ON:
        {
            //turn on all LEDs of group to the default color
            Serial.println("GROUP ON event");
            int groupCount = group->numLeds;
            for (int i = 0; i < groupCount; i++)
            {
                //Serial.printf("GROUP ON i:%d color:%X\r\n", i, (group->color.r<<16)+(group->color.g<<8)+group->color.b);
                leds[ group->members[i]] = group->color;
            }
            break;
        }

        case GROUP_ON_COLOR: // color
        {
            //turn on all LEDs of group to a specified color
            Serial.println("GROUP ON event");
            int groupCount = group->numLeds;
            unsigned int color = getNextGroupCommand( group);
            for (int i = 0; i < groupCount; i++)
            {
                //Serial.printf("GROUP ON i:%d color:%X\r\n", i, (group->color.r<<16)+(group->color.g<<8)+group->color.b);
                leds[ group->members[i]] = color;
            }
            break;
        }

        case GROUP_OFF:
        {
            //turn off all LEDs of group
            Serial.println("GROUP OFF event");
            int groupCount = group->numLeds;
            for (int i = 0; i < groupCount; i++)
            {
                //Serial.printf("GROUP ON i:%d color:%X\r\n", i, (group->color.r<<16)+(group->color.g<<8)+group->color.b);
                leds[ group->members[i]] = CRGB::Black;
            }
            break;
        }

        case GROUP_ON_MULTI:
        {
            //turn on all LEDs of group with random colors
            Serial.println("GROUP COLOR MULTI event");
            int groupCount = group->numLeds;
            CRGB groupLedColor;
            for (int i = 0; i < groupCount; i++)
            {
                groupLedColor.r = random (255);
                groupLedColor.g = random (255);
                groupLedColor.b = random (255);
                leds[ group->members[i]] = groupLedColor;
            }
            break;
        }

        case GROUP_COLOR: // color
            //set color of group
            Serial.println("GROUP COLOR event");
            group->color = getNextGroupCommand( group);
            break;

        case GROUP_COLOR_SEGMENT: // color, start, end
        {
            //set color of a segment
            Serial.println("GROUP COLOR SEGMENT event");
            //printGroupInfo( group);
            CRGB groupLedColor = getNextGroupCommand( group);
            unsigned int start =  getNextGroupCommand( group);
            unsigned int end = getNextGroupCommand( group);

            for (unsigned int i = start; i <= end; i++)
            {
                leds[ group->members[i]] = groupLedColor;
            }
            break;
        }

        case GROUP_LOAD_PATTERN: //count color...
        {
            Serial.println("GROUP LOAD PATTERN event");
            unsigned int groupCount = getNextGroupCommand( group);

            for (int i = 0; i < group->numLeds; i++)
            {
                if (groupCount > 0)
                {
                    leds[ group->members[i]] = getNextGroupCommand( group);
                    groupCount--;
                }
                else
                {
                    leds[ group->members[i]] = CRGB::Black;
                }
            }
            while (groupCount > 0) // in case more colors than group members
            {
                getNextGroupCommand( group);
                groupCount--;
            }
            break;
        }

        case GROUP_RAINBOW:
        //fill_rainbow(leds, NUM_LEDS, 0, 20);
        //*pFirstLED, int numToFill, uint8_t initialhue, uint8_t deltahue=5)
        {
            Serial.println("GROUP RAINBOW event");
            int groupCount = group->numLeds;
            CRGB scratchLEDs[ NUM_LEDS];
            group->count = 0;
            fill_rainbow( scratchLEDs, groupCount, group->count, 5);
            for (int i = 0; i < groupCount; i++)
            {
                leds[ group->members[ i]] = scratchLEDs[ i];
            }
            break;
        }

        case GROUP_WHEEL:
        // spreading a color wheel across all of the LEDS
        {
            Serial.println("GROUP WHEEL event");
            int groupCount = group->numLeds;
            for (int i = 0; i < groupCount; i++)
                {
                leds[i] = wheel( i * 255 / groupCount);
            }
            break;
        }

        // immediate group movements
        case GROUP_ROTATE_RIGHT: // speed?
            Serial.println("GROUP ROTATE RIGHT event");
            groupRotateRight( group);
            break;


        case GROUP_ROTATE_LEFT: // speed?
            Serial.println("GROUP ROTATE LEFT event");
            groupRotateLeft( group);
            break;

        case GROUP_ROTATE_RIGHT_COUNT: // count
        {
            Serial.println("GROUP ROTATE RIGHT COUNT event");
            unsigned int groupCount = getNextGroupCommand( group);
            for (unsigned int i = 0; i < groupCount; i++)
            {
                groupRotateRight( group);
            }
            break;
        }

        case GROUP_ROTATE_LEFT_COUNT: // count
        {
            Serial.println("GROUP ROTATE LEFT COUNT event");
            unsigned int groupCount = getNextGroupCommand( group);
            for (unsigned int i = 0; i < groupCount; i++)
            {
                groupRotateLeft( group);
            }
            break;
        }

        // delayed group movements
        case GROUP_TWINKLE:
            Serial.println("GROUP TWINKLE event");
            // overly simplistic... only works one LED at a time
            //prime the pump
            group->onLED = random( group->numLeds);
            group->fromColor = leds[ group->onLED];
            group->startTime = currentTime;
            group->timeToWait = 100;
            group->currentState = groupTwinklingSPF;
            break;

        case GROUP_BREATHE:
            Serial.println("GROUP BREATHE event");
            // this maybe should have a breaths/minute parameter
#define bpm 10 /* breaths per minute. 12 to 16 for adult humans */
#define numBreathSteps 255
            group->breatheIndex = 0;
            group->breathStep = 255/numBreathSteps;
            group->startTime = currentTime;
            group->timeToWait = 60000/ bpm / numBreathSteps;
            group->currentState = groupBreathingSPF;
            break;

        case GROUP_CHASE_CW_PAIR: //from, dest, count, delay
            Serial.println("GROUP CHASE CW PAIR event");
            group->fromLED = getNextGroupCommand( group);
            group->toLED = getNextGroupCommand( group);
            group->count = getNextGroupCommand( group);
            group->onLED = group->fromLED;
            group->startTime = currentTime;
            group->timeToWait = getNextGroupCommand( group);
            group->currentState = groupChasePairRightSPF;
            break;

        case GROUP_CLOCK: // colors?
            Serial.println("GROUP CLOCK event");
            group->startTime = currentTime;
            group->timeToWait = CLOCK_EFFECT_TIME;
            group->currentState = groupClockSPF;
            break;

        case GROUP_WAIT: // time
            Serial.println("GROUP WAIT event");
            FastLED.show();
            group->timeToWait = getNextGroupCommand( group);
            group->startTime = currentTime;
            //Serial.printlnf("GROUP WAIT id:%d time:%d wait:%d start:%d",
            //        group->groupNumber, millis(), group->timeToWait, group->startTime);
            group->currentState = groupWaitSPF;
            break;

        case GROUP_FADE_TO_BLACK: // time
        {
            Serial.println("GROUP FADE TO BLACK event");
            unsigned int duration = getNextGroupCommand( group);
            group->startTime = currentTime;
            group->fadeStepNumber = 0;
            group->fromColor = group->color; // may need to do individually
            group->toColor = CRGB::Black;
            group->fadeSteps = min( duration, 255);
            group->timeToWait = max( 1, group->fadeSteps/255);
            group->startTime = currentTime;
            group->currentState = groupFadingSPF;
            break;
        }

        case GROUP_FADE_TO_COLOR: // time, color
        {
            Serial.println("GROUP FADE TO COLOR event");
            int unsigned duration = getNextGroupCommand( group);
            group->startTime = currentTime;
            group->fadeStepNumber = 0;
            group->fromColor = leds[ group->members[0]];
            group->toColor = getNextGroupCommand( group);
            group->color = group->color;
            group->startTime = currentTime;
            group->fadeSteps = min( duration, 255);
            group->timeToWait = max( 1, group->fadeSteps/255);
            group->currentState = groupFadingSPF;
            break;
        }

        case GROUP_WAVE:
            {
                Serial.println("GROUP WAVE event");
                int groupCount = group->numLeds;
                CRGB scratchLEDs[ NUM_LEDS];
                group->waveCount = group->waveCount + 1;
                if (group->waveCount > 255)
                {
                    group->waveCount = 0;
                }
                Serial.printf("GROUP WAVE count:\r\n", group->waveCount);
                fill_rainbow( scratchLEDs, groupCount, group->waveCount, 5);
                for (int i = 0; i < groupCount; i++)
                {
                    leds[ group->members[ i]] = scratchLEDs[ i];
                }
                group->startTime = currentTime;
                break;
            }


        // control functions
/*
        case GROUP_SET_GROUP:
        {
            Serial.println("GROUP SET GROUP event");
            // no error testing. was it a good number?
            int groupNumber = getNextGroupCommand( group);
            group = &ledGroupStates[groupNumber];
        }
            Serial.println("group is now group: %d\r\n", group->groupNumber);
            break;
*/


        case GROUP_REPEAT_FOREVER:
            Serial.println("GROUP REPEAT FOREVER event");
            // no error testing. was it a good number?
            group->repeatIndex = group->currentCommandIndex;
            group->count = -1;
            break;

        case GROUP_REPEAT_COUNT: // number
            Serial.println("GROUP REPEAT COUNT event");
            group->count = getNextGroupCommand( group);
            // no error testing. was it a good number?
            group->repeatIndex = group->currentCommandIndex;
            break;

        case GROUP_REPEAT_END:
            Serial.println("GROUP REPEAT END event");
            // no error testing. was it initialized, was it forever
            if (group->count != -1)
            {
                group->count = group->count - 1;
            }
            if (group->count == -1 || group->count > 0)
            {
                group->currentCommandIndex = group->repeatIndex;
            }
            break;

        case GROUP_GCOLOR: //group, color
        {
            Serial.println("GROUP GCOLOR event");
            int groupNumber = getNextGroupCommand( group);
            ledGroupStates[groupNumber].color = (CRGB) getNextGroupCommand( group);
            break;
        }

        case GROUP_GFORK: // group, *commandArray
        {
            // this doesn't affect any other process, so it you want to stop
            // a group, you must GROUP_GFORK a commandArray with a GROUP_STOP
            Serial.println("GROUP GFORK event");
            int forkGroup = getNextGroupCommand( group);
            int *commandArray = (int*)getNextGroupCommand( group);
            ledGroupStates[ forkGroup].timeToWait = 0; // disable target timer
            interpretGroupCommand ( &ledGroupStates[ forkGroup], commandArray,
                    currentTime);
            break;
        }

        case GROUP_SET_ALL: //
        {
            Serial.println("GROUP SET ALL event");
            for (int i = 0; i < NUM_LEDS; i++)
            {
                group->members[i] = i;
            }
            group->numLeds = NUM_LEDS;
            break;
        }

        case GROUP_SET_MEMBERS: // count, members
        {
            Serial.println("GROUP SET MEMBERS event");
            int count = getNextGroupCommand( group);
            group->numLeds = count;
            for (int i = 0; i < count; i++)
            {
                group->members[i] = getNextGroupCommand( group);
            }
            break;
        }

        case GROUP_PHASE_DELAY: // millseconds
        {
            Serial.println("GROUP PHASE DELAY event");
            int delay = getNextGroupCommand( group);
            for (int i = 0; i < group->numLeds; i++)
            {
                ledStates[ group->members[i]].timeToWait = i * delay + 1;
                Serial.printlnf("GPD i:%d led:%d, delay:%d",
			i, group->members[i] ,(i+1)*delay);
            }
            break;
        }

        case GROUP_LFORK: // ledNumber, *commandArray
        {
            // this doesn't affect any other process, so it you want to stop
            // an LED, you must GROUP_LKILL or GROUP_LFORK a commandArray with a LED_STOP
            Serial.println("GROUP LFORK event");
            int ledNumber = getNextGroupCommand( group);
            int *commandArray = (int*)getNextGroupCommand( group);
            ledStates[ ledNumber].currentCommandArray = commandArray;
            ledStates[ ledNumber].currentCommandIndex = 0;
            if (ledStates[ ledNumber].timeToWait == 0)
            {
                ledStates[ ledNumber].timeToWait = 1;
            }
            ledStates[ ledNumber].currentState = ledWaitSPF;
            ledStates[ ledNumber].startTime = currentTime;
            break;
        }

        case GROUP_LFORK_ALL: // *commandArray
        {
            // this doesn't affect any other process, so it you want to stop
            // an LED, you must GROUP_LKILL or GROUP_LFORK a commandArray with a LED_STOP
            Serial.printlnf("GROUP LFORK ALL event num%d", group->numLeds);
            int *commandArray = (int*)getNextGroupCommand( group);
            for (int i = 0; i < group->numLeds; i++)
            {
                ledStates[ group->members[i]].currentCommandArray = commandArray;
                ledStates[ group->members[i]].currentCommandIndex = 0;
                if (ledStates[ group->members[i]].timeToWait == 0)
                {
                    ledStates[ group->members[i]].timeToWait = 1;
                }
                ledStates[ group->members[i]].currentState = ledWaitSPF;
                ledStates[ group->members[i]].startTime = currentTime;
                Serial.printlnf("GLK ALL i:%d led:%d start:%d wait:%d",
                        i,
                        group->members[i],
                        ledStates[ group->members[i]].startTime,
                        ledStates[ group->members[i]].timeToWait
                        );
            }
            break;
        }

        case GROUP_LKILL: // ledNumber
        {
            int ledNumber = getNextGroupCommand( group);
            Serial.printlnf("GROUP LKILL event led:%d", ledNumber);
            ledStates[ ledNumber].timeToWait = 0; // disable timer
            ledStates[ ledNumber].currentState = ledNullSPF;
            break;
        }

        case GROUP_LKILL_ALL: //
        {
            Serial.printlnf("GROUP LKILL ALL event num%d", group->numLeds);
            for (int i = 0; i < group->numLeds; i++)
            {
                ledStates[ group->members[i]].timeToWait = 0; // disable timer
                ledStates[ group->members[i]].currentState = ledNullSPF;
            }
            break;
        }

        case GROUP_STOP: // stop instead of looping
            Serial.println("GROUP STOP event");
            FastLED.show(); // show what lead up to the stop
            group->timeToWait = 0; // disable timer
            group->currentState = groupStopSPF;
            break;

        default:
            Serial.println("unexpected GROUP event");
            break;
    }
}


void setLedMode( int group, int mode)
/*
 * this function changes the current mode of the LED display and sets up
 * the effect(s) to be applied over time.
 * This is normally called initially and by a web page via a particle.function call


it sure looks like most of this function can be replaced with a table. index by mode or effect number and provide a pointer to an array. Question is how to extract the length of the array. Maybe that is just another column in the table...

a better simplification is to add a known marker to end of a command string
when you hit it, you either stop, LED_STOP, GROUP_STOP, or loop, LED_LOOP, GROUP_LOOP.
 */
{
    //Serial.println(ledModeNames[mode]);
    //Serial.println(ledModeNames[mode].length());
    char modeName [ledModeNames[mode].length() + 1]; //include terminating null
    ledModeNames[ mode].toCharArray(modeName, ledModeNames[mode].length()+1);\
    Serial.printf("attempting LED mode change %d %s\r\n", mode, modeName);

    //stopAllLedFSMs();
    //stopAllLedGroupFSMs(); // this isn't right, want to control each group independently
    ledGroupStates[group].timeToWait = 0; //cancel timer
    /*
       the long term stopping mechanism should have a scope of control.
       you get a scope of control to control either the group as a whole or individuals within
       that scope.

       not sure how the scope is defined and allocated, but maybe it is with a group mechanism.
     */
    TIME now = millis();

    switch(  mode)
    {
        case offMode:
            //Serial.println("LED mode changed to offEffect");
            interpretGroupCommand ( &ledGroupStates[ group], offGC, now);
            break;

        case demo1:
            interpretGroupCommand ( &ledGroupStates[ 1], demo1GC, now);
            break;

        case singleColor:
            //Serial.println("LED mode changed to singleColor");
            interpretGroupCommand ( &ledGroupStates[ group], singleColorGC, now);
            break;

        case staticMultiColor:
            //Serial.println("LED mode changed to staticMultiColor");
            interpretGroupCommand ( &ledGroupStates[ group], multiColorGC, now);
            break;

        case twinklingMultiColor:
            //Serial.println("LED mode changed to twinklingMultiColor");
            interpretGroupCommand ( &ledGroupStates[ group], multiColorTwinkleGC, now);
            break;

        case allFlash:
            //Serial.println("LED mode changed to allFlash");
            interpretGroupCommand ( &ledGroupStates[ group], allFlashGC, now);
            break;

        case quadFlasher:
            //Serial.println("LED mode changed to quadFlasher");
            interpretGroupCommand ( &ledGroupStates[ group], quadFlashGC, now);
            break;

        case quadFlasher2:
            interpretGroupCommand ( &ledGroupStates[ group], quadFlashGC2, now);
            break;

        case quadFlasher3:
            interpretGroupCommand ( &ledGroupStates[ group], quadFlashGC3, now);
            break;

        case quadFlasher4:
            interpretGroupCommand ( &ledGroupStates[ group], quadFlashGC4, now);
            break;

        case quadFlasher5:
            interpretGroupCommand ( &ledGroupStates[ group], quadFlashGC5, now);
            break;

        case quadFlasher6:
            interpretGroupCommand ( &ledGroupStates[ group], quadFlashGC6, now);
            break;

        case quadFlasher7:
            interpretGroupCommand ( &ledGroupStates[ group], quadFlashGC7, now);
            break;

        case octaFlasher:
            interpretGroupCommand ( &ledGroupStates[ group], octaFlashGC, now);
            break;

        case octaFlasher2:
            interpretGroupCommand ( &ledGroupStates[ group], octaFlashGC2, now);
            break;

        case circulator:
            //Serial.printf("LED mode changed to circulator %d\r\n", millis());
            interpretGroupCommand ( &ledGroupStates[ group], circulatorGC, now);
            break;

        case pendulum:
            interpretGroupCommand ( &ledGroupStates[ group], pendulumGC, now);
            break;

        case staticRainbow:
            interpretGroupCommand ( &ledGroupStates[ group], rainbowGC, now);
            break;


        case staticWheel:
            interpretGroupCommand ( &ledGroupStates[ group], wheelGC, now);
            break;

        case colorWave:
            interpretGroupCommand ( &ledGroupStates[ group], waveGC, now);
            break;

        case clockFace:
            interpretGroupCommand ( &ledGroupStates[ group], clockGC, now);
            break;

        case marqueeLeft:
            //Serial.println("LED mode changed to marquee left");
            interpretGroupCommand ( &ledGroupStates[ group], marqueeLeftGC, now);
            break;

        case marqueeRight:
            //Serial.println("LED mode changed to marquee right");
            interpretGroupCommand ( &ledGroupStates[ group], marqueeRightGC, now);
            break;

        case breathe:
            //Serial.println("LED mode changed to breathe");
            interpretGroupCommand ( &ledGroupStates[ group], breatheGC, now);
            break;

        case flashSynched:
            //Serial.println("LED mode changed to flashSynched");
            // use interpreted flash sequence
            for (int i = 0; i < NUM_LEDS; i++)
            {
                startLedFSM(&ledStates[i], flash250);
            }
            break;

        case flashPhased:
            //Serial.println("LED mode changed to flashPhased");
            for (int i = 0; i < NUM_LEDS; i++)
            {
                ledStates[i].currentState = ledWaitSPF;
                ledStates[i].currentCommandArray = flash250;
                ledStates[i].currentCommandIndex = 0;
                ledStates[i].timeToWait = i * 250/24 + 1;
                ledStates[i].startTime = now;
            }
            break;

        case winkSynched:
            //Serial.println("LED mode changed to winkSynched");
            for (int i = 0; i < NUM_LEDS; i++)
            {
                startLedFSM(&ledStates[i], wink);
            }
            break;

        case winkPhased:
            //Serial.println("LED mode changed to winkPhased");
            for (int i = 0; i < NUM_LEDS; i++)
            {
                ledStates[i].currentState = ledWaitSPF;
                ledStates[i].currentCommandArray = wink;
                ledStates[i].currentCommandIndex = 0;
                ledStates[i].timeToWait = i * 30 + 1;
                ledStates[i].startTime = now;
            }
            break;

        case blinkSynched:
            //Serial.println("LED mode changed to blinkSynched");
            for (int i = 0; i < NUM_LEDS; i++)
            {
                startLedFSM(&ledStates[i], blink);
            }
            break;

        case blinkPhased:
            //Serial.println("LED mode changed to blinkPhased");
            for (int i = 0; i < NUM_LEDS; i++)
            {
                ledStates[i].currentState = ledWaitSPF;
                ledStates[i].currentCommandArray = blink;
                ledStates[i].currentCommandIndex = 0;
                ledStates[i].timeToWait = i * 30 + 1;
                ledStates[i].startTime = now;
            }
            break;

        case meteorSynched:
            //Serial.println("LED mode changed to meteorSynched");
            for (int i = 0; i < NUM_LEDS; i++)
            {
                startLedFSM(&ledStates[i], meteor);
            }
            break;

        case meteorPhased:
            //Serial.println("LED mode changed to meteorPhased");
            for (int i = 0; i < NUM_LEDS; i++)
            {
                ledStates[i].currentState = ledWaitSPF;
                ledStates[i].currentCommandArray = meteor;
                ledStates[i].currentCommandIndex = 0;
                ledStates[i].timeToWait = i * 30 + 1;
                ledStates[i].startTime = now;
            }
            break;

        default:
            //Serial.println("bad attempt to change LED mode");
            break;
    }
}

/* try to use group logic instead...
void interpretDirectorCommand( directorCommand dirCommand)
{
    TIME startTime = millis(); // time upon entering the current state
    int count;
    int delay;
    int functionIndex;
    switch (dirCommand)
    {
        case GROUP_GCOLOR: //color
        {
            CRGB color = getNextGroupCommand( selectedGroup);
            ledGroupStates[selectedGroup]->color = color;
            break;
        }

        case GROUP_GFORK: // group, *commandArray
        {
            // this doesn't affect any other process, so it you want to stop
            // a group, you must GROUP_GFORK a commandArray with a GROUP_STOP
            int group = getNextDirectorCommand( selectedGroup);
            int *commandArray = getNextDirectorCommand( selectedGroup);
            interpretGroupCommand ( &ledGroupStates[ group], commandArray, now);
            break;
        }

        case GROUP_SET_ALL: //
        {
            for (int i = 0; i < NUM_LEDS; i++)
            {
                ledGroupStates[selectedGroup]->members[i] = i;
            }
            ledGroupStates[selectedGroup]->numLeds = NUM_LEDS;
            break;
        }

        case GROUP_SET_MEMBERS: // count, members
        {
            int count = getNextGroupCommand( selectedGroup);
            ledGroupStates[selectedGroup]->numLeds = count;
            for (int i = 0; i < count; i++)
            {
                ledGroupStates[selectedGroup]->members[i] =
                        getNextGroupCommand( selectedGroup);
            }
            break;
        }

        case GROUP_PHASE_DELAY: // count, millseconds
        {
            int count = getNextGroupCommand( selectedGroup);
            int delay = getNextGroupCommand( selectedGroup);
            for (int i = 0; i < selectedGroup->numLeds; i++)
            {
                ledStates[ ledGroupStates[selectedGroup]->members[i]].timeToWait = i * delay + 1;
            }
            break;
        }

        case GROUP_LFORK: // ledNumber, *commandArray
        {
            // this doesn't affect any other process, so it you want to stop
            // an LED, you must GROUP_LFORK a commandArray with a LED_STOP
            int ledNumber = getNextDirectorCommand( selectedGroup);
            int *commandArray = getNextDirectorCommand( selectedGroup);
            startLedFSM( &ledStates[ ledNumber], commandArray];
            break;
        }
            break;

        default:
            break;
    }
}
*/

int interpretLedCommandString( String commandString)
/*
 * this function interprets a command string to control LED mode, color,
 * speed, direction, and brightness.
 *
 * The command string is sent by a web page to Particle.io which invoke this function
 */
{
    int value;
    int speed;
    int direction;
    String mode;
    int ptr;
    bool offSelected = false;
    #define lenTempStr 40
    char tempStr [ lenTempStr];
    //split command string into words
    /*
    single letter command for now
    n on
    f off
    bxxx brightness
    sxxx speed
    dxxx direction
    crrrbbbggg color
    rxxxgxxxbxxx RGb color
    Gxxx group
    mstring mode -- must be at the end, string is the name of a command
    */
    Serial.print( "Interpreting commandString: ");
    Serial.println( commandString);
    ptr = 0;
    char command = commandString.charAt( ptr);
    CRGB color;

    while (command > 0)
    {
        if (command != 'n' && command != 'f' && command != 'm') //not one of unary commands
        {
            value = 0;
            for (int i = ptr + 1; i < ptr + 4; i++)
            {
                if ( (int) commandString.charAt(i) == 0 )
                {
                    break;

                }
                else
                {
                    value = 10 * value + (int) commandString.charAt(i) - (int) '0';
                }
            }
            ptr = ptr + 4; // may skip over null
        }
        else
        {
            ptr = ptr + 1;
        }

        switch (command)
        {
            case 'G': //grop (group number)
                //Serial.println("Group");
                selectedGroup = value;
                if (value >= NUM_GROUPS)
                {
                    selectedGroup = 0;
                }
                break;

            case 'n': //on
                //Serial.println("on");
                //functionOnOff( 3, true, "");
                break;

            case 'f': //off
                //Serial.println("off");
                //functionOnOff( 3, false, "");
                offSelected = true;
                break;

            case 'l': //luminence (brightness)%
                if (value > 100)
                {
                    value = 100;
                }
                ledBrightness = value;
                FastLED.setBrightness( (int) (ledBrightness*255/100));
                snprintf( tempStr, lenTempStr, "Brightness: %d", ledBrightness);
                //Serial.println(tempStr);
                break;

            case 'm': //mode  .. is this a string or a value --string for now
                /*
                if (value > 100)
                {
                    value = 100;
                }
                mode = value;
                */
                //result = snprintf( mode, sizeof(mode), "%s", commandString);
                mode = "Mode: " + commandString.substring(ptr); // just the rest of the string
                //snprintf( tempStr, lenTempStr, "Mode: %s", mode);
                //Serial.println(mode);
                for (int i = 0; i <  NUM_LED_MODES; i++)
                {
                    if ( commandString.substring(ptr).compareTo(ledModeNames[i]) == 0)
                    {
                        if (offSelected)
                        {
                            //Serial.println("Off selected");
                            ledBrightness = 0;
                            FastLED.setBrightness( (int) (ledBrightness*255/100));
                        }
                        //No error testing for the following??? in general this whole function sets a
                        //bunch of globals. Not exactly best practice.
                        ledGroupStates[selectedGroup].color = color;
                        ledColor = color; // default for individual LEDs
                        setLedMode( selectedGroup, i);
                        FastLED.show(); //is this necessary or desired?
                        break;

                    }
                }
                ptr = commandString.length();// mode must be the last command
                break;

            case 's': //speed 0..200: -100..100
                if (value > 200)
                {
                    value = 200;
                }
                speed = value-100;
                snprintf( tempStr, lenTempStr, "Speed: %d", speed);
                //Serial.println(tempStr);
                break;

            case 'd': //direction 0..359
                if (value > 359)
                {
                    value = 0;
                }
                direction = value;
                snprintf( tempStr, lenTempStr, "Direction: %d", direction);
                //Serial.println(tempStr);
                break;

            case 'r': //red 0..255
                if (value > 255)
                {
                    value = 255;
                }
                color.r = value;
                snprintf( tempStr, lenTempStr, "Red: %d", value);
                //Serial.println(tempStr);
                break;

            case 'b': //b 0..255
                if (value > 255)
                {
                    value = 255;
                }
                color.b = value;
                snprintf( tempStr, lenTempStr, "Blue: %d", value);
                //Serial.println(tempStr);
                break;

            case 'g': //green 0..255
                if (value > 255)
                {
                    value = 255;
                }
                color.g = value;
                snprintf( tempStr, lenTempStr, "Green: %d", value);
                //Serial.println(tempStr);
                break;

            // w -speed
            // x +speed
            // y -dir
            // z +dir
            default:
                return -1; //bad command
                break;
        }
        command = commandString.charAt( ptr);
    }

    //Serial.println("Done");
    return 0; // good command
}


void ledSetup ()
// setup of the LED module
{
    Time.zone(-5); //EST
    FastLED.addLeds<WS2812, FASTLED_PIN, GRB>(leds, NUM_LEDS);

    setupAllLedFSMs();
    stopAllLedFSMs();

    setupAllLedGroupFSMs();
    stopAllLedGroupFSMs();

    // setup for web controls
    Particle.function ("ledControl", interpretLedCommandString);

    // setup group 0
    ledGroupStates[0].numLeds = NUM_LEDS;
    for (int i = 0; i < NUM_LEDS; i++)
    {
        ledGroupStates[0].members[i] = i;
    }
    setLedMode( 0, clockFace);
}



void setup()
{
    Serial.begin(9600);
    Serial.println("Serial monitor started");


    ledBrightness = 1;
    FastLED.setBrightness( (int) (ledBrightness*255/100));

    ledSetup();
}

void loop()
{
    //groupFSM(); // handle group LED effects
    //ledFSM(); // handle individual LED effects


    LedState state;
    TIME now = millis();
    TIME last = millis();
    bool changed = false;
    for (int i = 0; i < NUM_LEDS; i++)
    {
        // the following is just to working around millis() jumping around
        // due to interference with FastLED
        now = millis();
        while (now - last > 1000) {
            delayMicroseconds (100);
            now = millis();
        }
        last = now;

        state = ledStates[i];
        if (state.timeToWait > 0 &&
                now - state.startTime > state.timeToWait )
        {
            state.currentState(&ledStates[i], LED_TIME_OUT, now);
            changed = true; // just assume that something changed on event
        }
    }

    LedGroupState group;
    for (int i = 0; i < NUM_GROUPS; i++)
    {
        // the following is just to working around millis() jumping around
        // due to interference with FastLED
        now = millis();
        while (now - last > 1000) {
            delayMicroseconds (100);
            now = millis();
        }
        last = now;

        group = ledGroupStates[i];
        if (group.timeToWait > 0 &&
                now - group.startTime > group.timeToWait )
        {
            //Serial.printlnf("loop time:%d id:%d now:%d",
            //        millis(), group.groupNumber, now);
            group.currentState(&ledGroupStates[i], GROUP_TIME_OUT, now);
            changed = true; // just assume that something changed on event
        }
    }

    if (changed)
    {
        FastLED.show();
    }
}
