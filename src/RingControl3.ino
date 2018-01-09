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
*/



// **** INCLUDES ****

//Load Fast LED Library and set it's options.
#include <FastLED.h>
FASTLED_USING_NAMESPACE;
#define PARTICLE_NO_ARDUINO_COMPATIBILITY 1



//**** CONSTANTS ****

#define NUM_LEDS 24
#define FASTLED_PIN D6

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

const CRGB R = CRGB::Red;
const CRGB Y = CRGB::Yellow;
const CRGB W = CRGB::White;
const CRGB B = CRGB::Blue;
const CRGB G = CRGB::Green;
const CRGB O = CRGB::Orange;
const CRGB V = CRGB::Violet;
const CRGB K = CRGB::Black;

enum LEDModes {
    offMode,
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
    octoFlasher,
    circulator,
    singleColorBreath,
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
    "octoFlasher",
    "circulator",
    "singleColorBreath",
    "flashSynched",
    "flashPhased",
    "winkSynched",
    "winkPhased",
    "blinkSynched",
    "blinkPhased",
    "meteorSynched",
    "meteorPhased",
};

enum effects {
    offEffect,
    steady,
    fadeToBlack,
    fadeToTarget,
    rotateRightEffect,
    rotateLeftEffect,
    breathe,
    twinkle,
    flashing,
    quadFlash1,
    quadFlash2,
    quadFlashChase1,
    quadFlashChase2,
    quadFlashChase3,
    quadFlashChase4,
    octoFlash1,
    octoFlash2,
    circulator1,
    circulator2,
    clockEffect,
    huh
};


enum ledCommand {
    LED_ON,
    LED_OFF,
    LED_ON_TIMED, // color
    LED_OFF_TIMED, // duration
    LED_TIMED_COLOR, // duration, color
    LED_FADE_TO_BLACK, //duration
    LED_FADE_TO_COLOR, //duration, color
    LED_STOP
};


enum ledState {
    LED_STATE_ENTRY, // state when starting a FSM
    LED_STATE_ON,
    LED_STATE_OFF,
    LED_STATE_FADING,
    LED_STATE_NULL = 4     // LED is not being controlled, LED may be on or off
};


enum groupCommand {
    GROUP_ON,
    GROUP_OFF,
    GROUP_COLOR,
    GROUP_COLOR_SEGMENT,
    GROUP_LOAD_PATTERN,
    GROUP_ROTATE_RIGHT,
    GROUP_ROTATE_LEFT,
    GROUP_ROTATE_RIGHT_COUNT,
    GROUP_ROTATE_LEFT_COUNT,
    GROUP_ROTATE_RIGHT_REPEATING,
    GROUP_ROTATE_LEFT_REPEATING,
    GROUP_CLOCK,
    GROUP_CHASE_CW_PAIR,
    GROUP_WAIT,
    GROUP_FADE_TO_BLACK,
    GROUP_FADE_TO_COLOR,
    GROUP_STOP
};

enum groupState {
    GROUP_STATE_ENTRY,
    GROUP_STATE_NULL,
    GROUP_STATE_NO_ACTION,
    GROUP_STATE_FADING,
    GROUP_STATE_ROTATING_RIGHT,
    GROUP_STATE_ROTATING_LEFT,
    GROUP_STATE_CLOCK,
    GROUP_STATE_BREATHING,
    GROUP_STATE_CHASE_CW_PAIR
};

// individual LED command arrays
// these loop unless directed not to with LED_STOP
int flash250 [] = {
    LED_ON_TIMED, 250,
    LED_OFF_TIMED, 250
};

int wink [] = {
    LED_ON_TIMED, 2500,
    LED_OFF_TIMED, 100};

int blink [] = {
    LED_OFF_TIMED, 2500,
    LED_ON_TIMED, 100
};

int meteor [] = {
    LED_ON_TIMED, 30,
    LED_FADE_TO_BLACK, 80,
    LED_OFF_TIMED, 1000
};


// group LED command arrays
// these loop unless directed not to with GROUP_STOP
int singleColorGC [] = {
    GROUP_ON,
    GROUP_STOP
};

int allFlashGC [] = {
    GROUP_ON,
    GROUP_WAIT, 250,
    GROUP_OFF,
    GROUP_WAIT, 250
};


int quadFlashGC [] = {
    GROUP_LOAD_PATTERN, R,R,R,R,R,R,
                        K,K,K,K,K,K,
                        R,R,R,R,R,R,
                        K,K,K,K,K,K,
    GROUP_WAIT, 250,
    GROUP_LOAD_PATTERN, K,K,K,K,K,K,
                        R,R,R,R,R,R,
                        K,K,K,K,K,K,
                        R,R,R,R,R,R,
    GROUP_WAIT, 250
};


int quadFlashGC2 [] = {
    GROUP_LOAD_PATTERN, R,R,R,R,R,R,
                        K,K,K,K,K,K,
                        R,R,R,R,R,R,
                        K,K,K,K,K,K,
    GROUP_WAIT, 250,
    GROUP_ROTATE_RIGHT,
    GROUP_ROTATE_RIGHT,
    GROUP_ROTATE_RIGHT,
    GROUP_ROTATE_RIGHT,
    GROUP_ROTATE_RIGHT,
    GROUP_ROTATE_RIGHT,
    GROUP_ROTATE_RIGHT,
    GROUP_ROTATE_RIGHT,
    GROUP_WAIT, 250
};


int quadFlashGroup3 [] = {
    GROUP_LOAD_PATTERN, R,R,R,R,R,R,
                        K,K,K,K,K,K,
                        R,R,R,R,R,R,
                        K,K,K,K,K,K,
    GROUP_WAIT, 250,
    GROUP_ROTATE_RIGHT_COUNT, NUM_LEDS/4,
    GROUP_WAIT, 250
};



int quadFlashGroup4 [] = {
    GROUP_COLOR_SEGMENT, R, 0, NUM_LEDS/4-1,
    GROUP_COLOR_SEGMENT, K, NUM_LEDS/4, NUM_LEDS/2-1,
    GROUP_COLOR_SEGMENT, R, NUM_LEDS/2, 3*NUM_LEDS/4-1,
    GROUP_COLOR_SEGMENT, K, 3*NUM_LEDS/4, NUM_LEDS-1,
    GROUP_WAIT, 250,
    GROUP_COLOR_SEGMENT, K, 0, NUM_LEDS/4-1,
    GROUP_COLOR_SEGMENT, R, NUM_LEDS/4, NUM_LEDS/2-1,
    GROUP_COLOR_SEGMENT, K, NUM_LEDS/2, 3*NUM_LEDS/4-1,
    GROUP_COLOR_SEGMENT, R, 3*NUM_LEDS/4, NUM_LEDS-1,
    GROUP_WAIT, 250
};


int quadFlashGroup5 [] = {
    GROUP_COLOR_SEGMENT, R, 0, NUM_LEDS/4-1,
    GROUP_COLOR_SEGMENT, K, NUM_LEDS/4, NUM_LEDS/2-1,
    GROUP_COLOR_SEGMENT, R, NUM_LEDS/2, 3*NUM_LEDS/4-1,
    GROUP_COLOR_SEGMENT, K, 3*NUM_LEDS/4, NUM_LEDS-1,
    GROUP_WAIT, 250,
    GROUP_ROTATE_RIGHT_COUNT, NUM_LEDS/4,
    GROUP_WAIT, 250
};


int quadFlashGroup6 [] = {
    GROUP_COLOR_SEGMENT, K, 0, NUM_LEDS-1,
    GROUP_COLOR_SEGMENT, R, 0, NUM_LEDS/4-1,
    GROUP_COLOR_SEGMENT, R, NUM_LEDS/2, 3*NUM_LEDS/4-1,
    GROUP_WAIT, 250,
//  GROUP_CHASE, NUM_LEDS/4-1, NUM_LEDS/2-1,
//  GROUP_CHASE, NUM_LEDS/2-1, 3*NUM_LEDS/4-1,
//  GROUP_CHASE, NUM_LEDS/4-2, NUM_LEDS/2-2,
//  GROUP_CHASE, NUM_LEDS/2-2, 3*NUM_LEDS/4-2,
//  GROUP_CHASE, NUM_LEDS/4-3, NUM_LEDS/2-3,
//  GROUP_CHASE, NUM_LEDS/2-3, 3*NUM_LEDS/4-3,
//  GROUP_CHASE, NUM_LEDS/4-4, NUM_LEDS/2-4,
//  GROUP_CHASE, NUM_LEDS/2-4, 3*NUM_LEDS/4-4,
//  GROUP_CHASE, NUM_LEDS/4-5, NUM_LEDS/2-5,
//  GROUP_CHASE, NUM_LEDS/2-5, 3*NUM_LEDS/4-5,
//  GROUP_CHASE, NUM_LEDS/4-6, NUM_LEDS/2-6,
//  GROUP_CHASE, NUM_LEDS/2-6, 3*NUM_LEDS/4-6,
//above doesn't work well because pairs are now done sequentially rather than simultaneously
//  GROUP_CHASE_CW_PAIR, NUM_LEDS/4-1, 0, NUM_LEDS/2-1, 30 //fromState, fromEnd, dest, delay
    GROUP_WAIT, 250

};


int octaFlashGroup [] = {
    GROUP_LOAD_PATTERN, R,R,R,K,K,K,
                        R,R,R,K,K,K,
                        R,R,R,K,K,K,
                        R,R,R,K,K,K,
    GROUP_WAIT, 250,
    GROUP_ROTATE_RIGHT_COUNT, NUM_LEDS/8,
    GROUP_WAIT, 250
};


int octaFlashGroup2 [] = {
    GROUP_COLOR_SEGMENT, K, 0, NUM_LEDS-1,
    GROUP_COLOR_SEGMENT, R, 0,              NUM_LEDS/8-1,
    GROUP_COLOR_SEGMENT, R, 2*NUM_LEDS/8, 3*NUM_LEDS/8-1,
    GROUP_COLOR_SEGMENT, R, 4*NUM_LEDS/8, 5*NUM_LEDS/8-1,
    GROUP_COLOR_SEGMENT, R, 6*NUM_LEDS/8, 7*NUM_LEDS/8-1,
    GROUP_WAIT, 250,
    GROUP_ROTATE_RIGHT_COUNT, NUM_LEDS/8,
    GROUP_WAIT, 250
};


//**** TYPEDEFs ****

// LedState is the state information of indivually controlled LEDs
typedef struct LedState {
    int ledNumber;              // number of LED being controlled
    int *currentCommandArray;   // pointer to array of commands
    int currentCommandArraySize; // number of commands in current array
    int currentCommandIndex;    // index into current array of commands
    ledState currentState;  // current LED state
    uint8_t fadeStepNumber; // color is proportioned 255*(fadeSteps - fadeStepNumber)/fadeSteps
    uint8_t fadeSteps;      // total number of steps in fade
    CRGB fromColor;
    CRGB toColor;
    unsigned long timeEntering; // time upon entering the current state
    unsigned timeToRemain;           // time to remain in the current state
};


typedef struct LedGroupState {
    int groupNumber;        // number of LED group being controlled
    int *currentCommandArray;   // pointer to array of commands
    int currentCommandArraySize; // number of commands in current array
    int currentCommandIndex;    // index into current array of commands
    int firstLED;//obsolete
    int lastLED;//obsolete
    int fromLED;
    int toLED;
    int onLED;
    int count;//within group effect
    uint8_t fadeStepNumber; // color is proportioned 255*(fadeSteps - fadeStepNumber)/fadeSteps
    uint8_t fadeSteps;      // total number of steps in fade
    CRGB fromColor; // fading from color
    CRGB toColor;   // fading to color
    CRGB color;
    CRGB pattern [NUM_LEDS]; // array of colors
    int members [NUM_LEDS];    // ordered array of LED numbers in group
    int numLeds;            // number of LED members in group
    groupState currentState;
    unsigned long timeEntering; // time upon entering the current state
    unsigned timeToRemain;           // time to remain in the current state
};


LedGroupState ledGroupStates [NUM_GROUPS];



//**** GLOBALS ****

CRGB leds[NUM_LEDS]; // main LED array which is directly written to the LEDs

int LEDbrightness; // level of over all brightness desired 0..100 percent
int LEDPoint = 0; // 0..23 LED direction for selected modes
//int LEDSpeed = 0; // 0..100 speed of LED display mode
CRGB ledColor = CRGB::White; // color of the LEDs
int selectedGroup = 0; // currently selected group

LEDModes LEDMode = offMode; // LED display mode, see above
LEDModes lastLEDMode = offMode;

char charLedModes [100]; // this is referenced but not initiated?

// these variable are part of the group LED control state information
int steps = 0;
int stepNumber = 0;
effects effect = steady;
int LEDDirection = 0;

// special variables for the group breathing effect
#define bpm 10 /* breaths per minute. 12 to 16 for adult humans */
#define numBreathSteps 255
#define ledBias 2 /* minimum led value during sine wave */
int breathStep = 255/numBreathSteps;
int breathStepTime = 60000/ bpm / numBreathSteps;
uint8_t breathIndex = 0; // state of the breath

// special variable for the LED chase effect
int onLED = 0;
int endLED = 0;
int fromLED = 0;
int toLED = 0;

unsigned long ledTime = 0; // used only the the HUH? transition

unsigned effectTime = 0;
unsigned long lastEffectTime = 0;

LedState ledStates [ NUM_LEDS];



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
    ledStates[i].currentState = LED_STATE_NULL;
    ledStates[i].timeToRemain = 0;
  }
}


void stopAllLedGroupFSMs()
{
  for (int i = 0; i < NUM_GROUPS; i++)
  {
    ledGroupStates[i].currentState = GROUP_STATE_NULL;
    ledGroupStates[i].timeToRemain = 0;
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
    Serial.printf("Led:%d state:%d commandArray:%d commandArraySize:%d commandIndex:%d timeToRemain:%d\r\n",
            state->ledNumber,
            state->currentState,
            state->currentCommandArray,
            state->currentCommandArraySize,
            state->currentCommandIndex,
            state->timeToRemain);
}


int getNextGroupCommand ( LedGroupState *group)
// returns the next LED command for a particular state and updates pointers
{
    //printGroupStateInfo( group);
    int command = group->currentCommandArray[ group->currentCommandIndex];
    //Serial.printf("getNextGroupCommand %d\r\n", command);
    group->currentCommandIndex++;
    if (group->currentCommandIndex >= group->currentCommandArraySize)
    {
        group->currentCommandIndex = 0;
    }
    //printStateInfo( state);
    return command;
}


int getNextLedCommand ( LedState *state)
// returns the next LED command for a particular state and updates pointers
{
    //printStateInfo( state);
    int command = state->currentCommandArray[ state->currentCommandIndex];
    //Serial.printf("getNextLedCommand %d\r\n", command);
    state->currentCommandIndex++;
    if (state->currentCommandIndex >= state->currentCommandArraySize)
    {
        state->currentCommandIndex = 0;
    }
    //printStateInfo( state);
    return command;
}


void calcFadeSteps( LedState *state, int time)
/*
  need to calulate the number of steps in the fade and the
  amount to change the fade factor each step.
  given: time to fade
  minimum step size = 1 ms.
  if less than 256 ms, need to use a fractional change fade
  256 * (total steps - step Number) / total steps
 */
{
    if (time < 256)
    {
        state->fadeSteps = time;
        state->timeToRemain = 1;
    }
    else
    {
        state->fadeSteps = 255;
        state->timeToRemain = time/255; // makes fades a multiple of 256 ms...
    }
}


void calcGroupFadeSteps( LedGroupState *groupState, int time)
/*
  need to calulate the number of steps in the fade and the
  amount to change the fade factor each step.
  given: time to fade
  minimum step size = 1 ms.
  if less than 256 ms, need to use a fractional change fade
  256 * (total steps - step Number) / total steps
 */
{
    if (time < 256)
    {
        groupState->fadeSteps = time;
        groupState->timeToRemain = 1;
    }
    else
    {
        groupState->fadeSteps = 255;
        groupState->timeToRemain = time/255; // makes fades a multiple of 256 ms...
    }
}


void interpretNextLedCommand ( LedState *state, unsigned long entryTime)
{
    Serial.printf( "interpretNextLedCommand %d LED:%d address:%d time:%d\r\n",
            state->currentCommandArray[state->currentCommandIndex],
            state->ledNumber,
            state,
            entryTime);
    //printStateInfo( state);
    switch (getNextLedCommand( state))
    {
        case LED_ON:
            Serial.println("LED ON command");
            //printStateInfo( state);
            //turn LED on without timer
            leds[ state->ledNumber] = ledColor;
            state->timeToRemain = 0;
            state->currentState = LED_STATE_NULL;
            break;

        case LED_OFF:
            Serial.println("LED OFF command");
            //printStateInfo( state);
            //turn LED off without timer
            leds[ state->ledNumber] = CRGB::Black;
            state->timeToRemain = 0;
            state->currentState = LED_STATE_NULL;
            break;

        case LED_ON_TIMED:
            Serial.printf("LED ON TIMED command LED:%d\r\n", state->ledNumber);
            //turn LED on with timer
            leds[ state->ledNumber] = ledColor;
            state->timeEntering = entryTime;
            state->timeToRemain = getNextLedCommand( state);
            state->currentState = LED_STATE_ON;
            //printStateInfo( state);
            break;

        case LED_OFF_TIMED:
            Serial.printf("LED OFF TIMED command LED:%d\r\n", state->ledNumber);
            leds[ state->ledNumber] = CRGB::Black;
            state->timeEntering = entryTime;
            state->timeToRemain = getNextLedCommand( state);
            state->currentState = LED_STATE_OFF;
            //printStateInfo( state);
            break;

        case LED_TIMED_COLOR:
            Serial.println("LED TIMED COLOR command");
            leds[ state->ledNumber] = getNextLedCommand( state);
            state->timeEntering = entryTime;
            state->timeToRemain = getNextLedCommand( state);
            state->currentState = LED_STATE_ON;
            break;

        case LED_FADE_TO_BLACK:
            Serial.println("LED FADE TO BLACK command");
            state->timeEntering = entryTime;
            state->fadeStepNumber = 0;
            calcFadeSteps( state, getNextLedCommand( state));
            state->fromColor = leds[ state->ledNumber];
            state->toColor = CRGB::Black;
            state->currentState = LED_STATE_FADING;
            break;

        case LED_FADE_TO_COLOR:
            Serial.println("LED FADE TO COLOR command");
            state->timeEntering = entryTime;
            state->fadeStepNumber = 0;
            calcFadeSteps( state, getNextLedCommand( state));
            state->fromColor = leds[ state->ledNumber];
            state->toColor = getNextLedCommand( state);
            state->currentState = LED_STATE_FADING;
            break;

        case LED_STOP:
            Serial.println("LED STOP command");
            state->currentState = LED_STATE_NULL;
            state->timeToRemain = 0;
            break;

        default:
            break;
    }
    //printStateInfo( state);
}


void ledFSM ()
// process timing events for individual LEDs and maintain their state
{
    unsigned long now = millis();
    //for all LEDs
    //Serial.printf( "top of ledFSM time:%d\r\n", now);
    bool changed = false;
    for (int i = 0; i< NUM_LEDS; i++)
    {
        //switch to current state of LED
        switch (ledStates[i].currentState)
        {
            case LED_STATE_ENTRY:
                // should only get here on FSM start up
                if (ledStates[i].timeToRemain > 0 &&
                        now - ledStates[i].timeEntering > ledStates[i].timeToRemain)
                {
                    interpretNextLedCommand( &ledStates[i], now); // set up next state
                    changed = true;
                }
                break;

            case LED_STATE_ON:
            case LED_STATE_OFF:
                // if timer has expired
                if (ledStates[i].timeToRemain > 0 &&
                        now - ledStates[i].timeEntering > ledStates[i].timeToRemain)
                {
                    interpretNextLedCommand( &ledStates[i], now); // set up next state
                    changed = true;
                }
                break;

            case LED_STATE_FADING:
                if (ledStates[i].timeToRemain > 0 &&
                        now - ledStates[i].timeEntering > ledStates[i].timeToRemain)
                {
                    // fade a bit more toward target
                    int fadeFactor = (ledStates[i].fadeSteps - ledStates[i].fadeStepNumber)*
                    255 / ledStates[i].fadeSteps;
                    if (ledStates[i].toColor) // not black
                    {
                        leds[i] = blend (ledStates[i].fromColor, ledStates[i].toColor,
                            fadeFactor/255);
                    }
                    else // black
                    {
                        leds[i].r = ledStates[i].fromColor.r * fadeFactor/255;
                        leds[i].g = ledStates[i].fromColor.g * fadeFactor/255;
                        leds[i].b = ledStates[i].fromColor.b * fadeFactor/255;
                    }
                    //Serial.printf( "Fading %d %d\r\n", ledStates[i].fadeStepNumber,
                    //    fadeFactor);
                    ledStates[i].fadeStepNumber++;
                    if (ledStates[i].fadeStepNumber == ledStates[i].fadeSteps)
                    {
                        interpretNextLedCommand( &ledStates[i], now); // set up next state
                    }
                    else
                    {
                        ledStates[i].timeEntering = now;
                        // go around again
                    }
                    changed = true;
                }
                break;

            case LED_STATE_NULL: // no timer should be running, do nothing
                break;

            default:
                Serial.printf("ledFSM got an unexpected state %d\r\n",
                        ledStates[i].currentState);
                break;
        }
    }
    if (changed)
    {
        FastLED.show();
    }
    //Serial.println( "bottom of ledFSM");
}


/* GROUP STUFF UNDER CONSTRUCTION */
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
            CRGB saveLed = leds[ group->members[ 0]];
            for (int i = 0; i < group->lastLED - 1; i++)
            {
                leds[ group->members[i]] = leds[ group->members[i] + 1];
            }
            leds [group->members[ group->numLeds - 1]] = saveLed;
}


void interpretNextGroupCommand ( LedGroupState *group, unsigned long entryTime)
{
    /* questions...
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
	we want to be able to assinn a group of LEDs to be the surrounding marquee
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
                GROUP_ON,
            GROUP_REPEAT_END
        or
            GROUP_REPEAT, 6,
                GROUP_ON,
                GROUP_WAIT, 200,
                GROUP_OFF,
                GROUP_ON,
            GROUP_REPEAT_END
        to prevent multiple instances, GROUP_REPEATx could check the loop count
            this would require zeroing the counter, it the process was interupted (or restarted)
            should not allow interpretation within the loop

    some commands are for immediate execution (GROUP_COLOR, GROUP_LOAD_PATTERN)
    some commands wait for a timer (GROUP_WAIT)
    some commands pass control to the FSM (e.g. GROUP_CLOCK)
    */

    Serial.printf( "interpretNextGroupCommand %d Group:%d address:%d time:%d\r\n",
            group->currentCommandArray[group->currentCommandIndex],
            group->groupNumber,
            group,
            entryTime);
    //printStateInfo( state);
    switch (getNextGroupCommand( group))
    {
        case GROUP_ON:
            //turn on all LEDs of group
            Serial.println("GROUP ON command");
            //printGroupInfo( group);
            for (int i = 0; i <= group->numLeds-1; i++)
            {
                int lc =  (ledColor.r<<16) + (ledColor.g<<8) + ledColor.b;
                Serial.printf( "GROUP ON i:%d led:%d color:%d\r\n", i, group->members[i], lc);
                //leds[ group->members[i]] = group->color;
                leds[ group->members[i]] = ledColor;
            }
            group->timeToRemain = 0;
            group->currentState = GROUP_STATE_NULL;
            break;

        case GROUP_OFF:
            //turn off all LEDs of group
            Serial.println("GROUP OFF command");
            //printGroupInfo( group);
            for (int i = 0; i <= group->numLeds-1; i++)
            {
                leds[ group->members[i]] = CRGB::Black;
            }
            group->timeToRemain = 0;
            group->currentState = GROUP_STATE_NULL;
            break;

        case GROUP_COLOR: // color
            //set color of group
            Serial.println("GROUP COLOR command");
            //printGroupInfo( group);
            //group->color = getNextGroupCommand( group);
            ledColor = getNextGroupCommand( group);
            break;

        case GROUP_COLOR_SEGMENT: // color, start, end
            {
                //set color of group
                Serial.println("GROUP COLOR SEGMENT command");
                //printGroupInfo( group);
                CRGB ledGroupColor = getNextGroupCommand( group);
                for (int i = getNextGroupCommand( group); i <= getNextGroupCommand( group); i++)
                {
                    leds[ group->members[i]] = ledGroupColor;
                }
            }
            break;

        case GROUP_LOAD_PATTERN: //count color...
            {
                int groupCount = getNextGroupCommand( group);
                for (int i = 0; i <= group->numLeds; i++)
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
            }
            break;

        // immediate group movements
        case GROUP_ROTATE_RIGHT: // speed?
            Serial.println("GROUP ROTATE RIGHT command");
            groupRotateRight( group);
            break;


        case GROUP_ROTATE_LEFT: // speed?
            Serial.println("GROUP ROTATE LEFT command");
            groupRotateLeft( group);
            break;

        case GROUP_ROTATE_RIGHT_COUNT: // count
            Serial.println("GROUP ROTATE RIGHT COUNT command");
            for (int i = 0; i <= getNextGroupCommand( group); i++)
            {
                groupRotateRight( group);
            }
            break;

        case GROUP_ROTATE_LEFT_COUNT: // count
            Serial.println("GROUP ROTATE LEFT command");
            for (int i = 0; i <= getNextGroupCommand( group); i++)
            {
                groupRotateLeft( group);
            }
            break;

        // delayed group movements
        case GROUP_ROTATE_RIGHT_REPEATING: // delay
            Serial.println("GROUP ROTATE LEFT REPEATING command");
            group->timeEntering = entryTime;
            group->timeToRemain = getNextGroupCommand( group);
            group->currentState = GROUP_STATE_ROTATING_RIGHT;
            break;

        case GROUP_ROTATE_LEFT_REPEATING: // delay
            Serial.println("GROUP ROTATE LEFT REPEATING command");
            group->timeEntering = entryTime;
            group->timeToRemain = getNextGroupCommand( group);
            group->currentState = GROUP_STATE_ROTATING_LEFT;
            break;

	case GROUP_CHASE_CW_PAIR: //from, dest, count, delay
            Serial.println("GROUP CHASE CW PAIR command");
            group->fromLED = getNextGroupCommand( group);
            group->toLED = getNextGroupCommand( group);
            group->count = getNextGroupCommand( group);
            group->timeEntering = entryTime;
            group->timeToRemain = getNextGroupCommand( group);
            group->currentState = GROUP_STATE_CHASE_CW_PAIR;
            break;

        case GROUP_CLOCK: // colors?
            Serial.println("GROUP CLOCK command");
            group->timeEntering = entryTime;
            group->currentState = GROUP_STATE_CLOCK;
            break;

        case GROUP_WAIT: // time
            Serial.println("GROUP WAIT command");
            FastLED.show();
            group->timeToRemain = getNextGroupCommand( group);
            group->currentState = GROUP_STATE_ENTRY;

        case GROUP_FADE_TO_BLACK: // time
            Serial.println("GROUP FADE TO BLACK command");
            group->timeEntering = entryTime;
            group->fadeStepNumber = 0;
            calcGroupFadeSteps( group, getNextGroupCommand( group));
            group->fromColor = leds[ group->members[0]]; // may need to load group
            group->toColor = CRGB::Black;
            group->currentState = GROUP_STATE_FADING;
            break;

        case GROUP_FADE_TO_COLOR: // time, color
            Serial.println("GROUP FADE TO COLOR command");
            group->timeEntering = entryTime;
            group->fadeStepNumber = 0;
            calcGroupFadeSteps( group, getNextGroupCommand( group));
            group->fromColor = leds[ group->members[0]];
            group->toColor = getNextGroupCommand( group);
            group->currentState = GROUP_STATE_FADING;
            break;

        case GROUP_STOP: // stop instead of looping
            Serial.println("GROUP STOP command");
            FastLED.show();
            group->timeToRemain = 0;
            break;

        default:
            Serial.println("Got an unexpected GROUP command");
            break;
    }
}


void groupFSM ()
// process timing events for LED groups and maintain their state
{
    unsigned long now = millis();
    //for all LEDs
    //Serial.printf( "top of ledFSM time:%d\r\n", now);
    bool changed = false;
    for (int i = 0; i< NUM_GROUPS; i++)
    {
        //switch to current state of LED
        switch (ledGroupStates[i].currentState)
        {
            case GROUP_STATE_ENTRY:
                // should only get here on FSM start up
                if (ledGroupStates[i].timeToRemain > 0 &&
                        now - ledGroupStates[i].timeEntering > ledGroupStates[i].timeToRemain)
                {
                    interpretNextGroupCommand( &ledGroupStates[i], now); // set up next state
                    changed = true;
                }
                break;

            case GROUP_STATE_NO_ACTION: // just wait for time to expire
                // if timer has expired
                if (ledGroupStates[i].timeToRemain > 0 &&
                        now - ledGroupStates[i].timeEntering > ledGroupStates[i].timeToRemain)
                {
                    interpretNextGroupCommand( &ledGroupStates[i], now); // set up next state
                    changed = true;
                }
                break;

            case GROUP_STATE_FADING:
            if (ledGroupStates[i].timeToRemain > 0 &&
                    now - ledGroupStates[i].timeEntering > ledGroupStates[i].timeToRemain)
                {
                    // fade a bit more toward target
                    int fadeFactor = (ledGroupStates[i].fadeSteps -
                            ledGroupStates[i].fadeStepNumber) * 255 /
                            ledGroupStates[i].fadeSteps;
                    if (ledGroupStates[i].toColor) // not black
                    {
                        for (int j = 0; j < ledGroupStates[j].numLeds; j++)
                        {
                            leds[ ledGroupStates[i].members[j]] = blend (ledGroupStates[i].fromColor,
                                    ledGroupStates[i].toColor,
                                    fadeFactor/255);
                        }
                    }
                    else // black
                    {
                        for (int j = 0; j < ledGroupStates[j].numLeds; j++)
                        {
                            leds[ ledGroupStates[i].members[j]].r =
                                    ledGroupStates[i].fromColor.r * fadeFactor/255;
                            leds[ ledGroupStates[i].members[j]].g =
                                    ledGroupStates[i].fromColor.g * fadeFactor/255;
                            leds[ ledGroupStates[i].members[j]].b =
                                    ledGroupStates[i].fromColor.b * fadeFactor/255;
                        }
                    }
                    //Serial.printf( "Fading %d %d\r\n", ledStates[i].fadeStepNumber,
                    //    fadeFactor);
                    ledGroupStates[i].fadeStepNumber++;
                    if (ledGroupStates[i].fadeStepNumber == ledGroupStates[i].fadeSteps)
                    {
                        interpretNextGroupCommand( &ledGroupStates[i], now); // set up next state
                    }
                    else
                    {
                        ledGroupStates[i].timeEntering = now;
                        // go around again
                    }
                    changed = true;
                }
                break;

            case GROUP_STATE_CLOCK:
                if (ledGroupStates[i].timeToRemain > 0 &&
                        now - ledGroupStates[i].timeEntering > ledGroupStates[i].timeToRemain)
                {
                    float hour = ((Time.hour() % 12) + Time.minute() / 60 + .5) *
                            ledGroupStates[i].numLeds / 12;
                    float minute = (Time.minute() + Time.second() / 60 + .5) *
                            ledGroupStates[i].numLeds / 60;
                    float second = (Time.second() * ledGroupStates[i].numLeds + .5)  / 60;
                    //draw the hands
                    fillLedColor( CRGB::Black);
                    leds[ ledGroupStates[i].members[ ((int) hour + 1) %
                            ledGroupStates[i].numLeds]] = CRGB::Red;
                    leds[ ledGroupStates[i].members[ (int) hour]] = CRGB::Red;
                    leds[ ledGroupStates[i].members[ ((int) hour - 1) %
                            ledGroupStates[i].numLeds]] = CRGB::Red;
                    leds[ ledGroupStates[i].members[ (int) minute]] = CRGB::Green;
                    leds[ ledGroupStates[i].members[ ((int) minute - 1) %
                            ledGroupStates[i].numLeds]] = CRGB::Green;
                    // flash the seconds hand every second
                    if (millis() % 1000 < 900) // just look at the milliseconds part
                    {
                        leds[ ledGroupStates[i].members[ (int) second]] = CRGB::Blue;
                    }
                    ledGroupStates[i].timeEntering = now;
                    FastLED.show();
                }
                break;

            case GROUP_STATE_BREATHING:
                // as written this breathes all LEDs, not just the group
                if (ledGroupStates[i].timeToRemain > 0 &&
                        now - ledGroupStates[i].timeEntering >
                        ledGroupStates[i].timeToRemain)
                    {
                        FastLED.setBrightness( (int) min(255, ((LEDbrightness *
                                quadwave8( breathIndex) / 100)+ledBias)));
                        breathIndex = breathIndex + breathStep;
                        ledGroupStates[i].timeEntering = now;
                        FastLED.show();
                    }
                    break;


            case GROUP_STATE_ROTATING_RIGHT:
                if (ledGroupStates[i].timeToRemain > 0 &&
                        now - ledGroupStates[i].timeEntering >
                        ledGroupStates[i].timeToRemain)
                {
                    groupRotateRight( &ledGroupStates[i]);
                    FastLED.show();
                }
                break;

            case GROUP_STATE_ROTATING_LEFT:
                if (ledGroupStates[i].timeToRemain > 0 &&
                        now - ledGroupStates[i].timeEntering >
                        ledGroupStates[i].timeToRemain)
                {
                    ledGroupStates[i].timeEntering = now;
                    groupRotateLeft( &ledGroupStates[i]);
                    FastLED.show();
                }
                break;

	    case GROUP_CHASE_CW_PAIR: //source, dest, delay
                //from ... onLED ... dest
                // count
                //                       destEndLED       destStartLED
                // fromLED ... onLED ... toLED
                // first LED moved from source to dest one pixel at a time
                // second LED moved from fromStart-1 to dest-1 one pixel at a time
                // ..
                // last LED moved from fromEnd to dest-n+1 one pixel at a time
                // onLED is the moving LED between fromLED and toLED
                if (ledGroupStates[i].timeToRemain > 0 &&
                        now - ledGroupStates[i].timeEntering >
                        ledGroupStates[i].timeToRemain)
                {
                    //move one led on path toward destination
                    if (ledGroupStates[i].onLED == ledGroupStates[i].toLED)
                    {
                        // done moving this LED
                        ledGroupStates[i].count = ledGroupStates[i].count -1;
                        if (ledGroupStates[i].count == 0) //done moving
                        {
                            ledGroupStates[i].timeEntering = millis();
                            ledGroupStates[i].timeToRemain = 250;
                            effect = quadFlashChase3;
                        }
                        else
                        {
                            // back fromLED and toLED for the next LED to move
                            ledGroupStates[i].fromLED = (ledGroupStates[i].fromLED - 1 +
                                     ledGroupStates[i].numLeds) %
                                     ledGroupStates[i].numLeds;
                            ledGroupStates[i].toLED = (ledGroupStates[i].toLED -
                                     1 + ledGroupStates[i].numLeds) %
                                     ledGroupStates[i].numLeds;
                        }
                    }
                    else
                    {
                        leds[ ledGroupStates[i].onLED] = CRGB::Black;
                        leds[ (ledGroupStates[i].onLED +
                                (ledGroupStates[i].numLeds>>1)) %
                                ledGroupStates[i].numLeds] = CRGB::Black;
                        ledGroupStates[i].onLED = (ledGroupStates[i].onLED + 1) %
                                ledGroupStates[i].numLeds;
                    }
                    if (ledGroupStates[i].onLED != ledGroupStates[i].fromLED)
                    {
                        ////Serial.printf( "turning on %d %d\r\n", ledGroupStates[i].onLED, (ledGroupStates[i].onLED + ledGroupStates[i].numLeds/2) % ledGroupStates[i].numLeds);
                        ////Serial.println("");
                        leds[ ledGroupStates[i].onLED] = ledColor;
                        leds[ (ledGroupStates[i].onLED +
                                ledGroupStates[i].numLeds/2) %
                                ledGroupStates[i].numLeds ] = ledColor;
                        FastLED.show();
                    }
                    ledGroupStates[i].timeEntering = millis();
                }
                break;

            case GROUP_STATE_NULL: // no timer should be running, do nothing
                FastLED.show(); // show whatever LEDs have changed, if any`
                break;

            default:
                Serial.printf("groupFSM got an unexpected state %d\r\n",
                        ledStates[i].currentState);
                break;
        }
    }
    if (changed)
    {
        FastLED.show();
    }
    //Serial.println( "bottom of ledFSM");
}


void interpretGroupCommand ( LedGroupState *group, int *commands, int time)
{
            group->currentCommandArray = singleColorGC;
            group->currentCommandArraySize = sizeof( singleColorGC) / sizeof(int);
            group->currentCommandIndex = 0;
            interpretNextGroupCommand ( group, time);
}


void setLEDMode( int group, int mode)
/*
 * this function changes the current mode of the LED display and sets up
 * the effect(s) to be applied over time.
 * This is normally called initially and by a web page via a particle.function call
 */
{
    //Serial.println(ledModeNames[mode]);
    //Serial.println(ledModeNames[mode].length());
    char modeName [ledModeNames[mode].length() + 1]; //include terminating null
    ledModeNames[ mode].toCharArray(modeName, ledModeNames[mode].length()+1);\
    Serial.printf("attempting LED mode change %d %s\r\n", mode, modeName);

    stopAllLedFSMs();
    stopAllLedGroupFSMs(); // this isn't right, want to control each group independently
    /*
       the long term stopping mechanism should have a scope of control.
       you get a scope of control to control either the group as a whole or individuals within
       that scope.

       not sure how the scope is defined and allocated, but maybe it is with a group mechanism.
     */
    unsigned long now = millis();

    switch(  mode)
    {
        case offMode:
            //Serial.println("LED mode changed to offEffect");
            //fillLedColor( CRBG::Black); // need a group function
            ledGroupStates[group].currentState = GROUP_STATE_NO_ACTION;
            break;

        case singleColor:
            //Serial.println("LED mode changed to singleColor");
            //Serial.printf("R%d G%d B%d\r\n", ledColor.r, ledColor.g, ledColor.b);
            //to apply this to a group, need to know that group or its groupState number
            // group 0 could default to all LEDs
            //want to interpret the GROUP_ON command.
            interpretGroupCommand ( &ledGroupStates[ group], singleColorGC, now);
            ledGroupStates[group].currentState = GROUP_STATE_ENTRY;
            break;

        case staticMultiColor:
            //Serial.println("LED mode changed to staticMultiColor");
            fillLedMultiColor(); // need a group function
            FastLED.show();
            ledGroupStates[group].currentState = GROUP_STATE_NO_ACTION;
            break;

        case twinklingMultiColor:
            //Serial.println("LED mode changed to twinklingMultiColor");
            //fillLedMultiColor(); // need a group function
            //effect = twinkle; // need a group state
            ledGroupStates[group].currentState = GROUP_STATE_NO_ACTION;
            FastLED.show();
            break;

        case allFlash:
            //Serial.println("LED mode changed to allFlash");
            interpretGroupCommand( &ledGroupStates[ group], allFlashGC, now);
            break;

        case quadFlasher:
            //Serial.println("LED mode changed to quadFlasher");
            interpretGroupCommand( &ledGroupStates[ group], quadFlashGC, now);
            break;

        case quadFlasher2:
            //Serial.println("LED mode changed to quadflasher2");
            //interpretGroupCommand ( &ledGroupStates[ group], quadFlashGC2, now)
            //effect = quadFlashChase1;
            ledGroupStates[group].timeToRemain = 250;
            ledGroupStates[group].timeEntering = now - ledGroupStates[group].timeToRemain; // make it due now
            ledGroupStates[group].currentState = GROUP_STATE_NO_ACTION;
            break;

        case octoFlasher:
            //Serial.println("LED mode changed to octoFlasher");
            //effect = octoFlash1;
            //effectTime = 250;
            //lastEffectTime = millis();
            ledGroupStates[group].timeToRemain = 250;
            ledGroupStates[group].timeEntering = now - ledGroupStates[group].timeToRemain; // make it due now
            ledGroupStates[group].currentState = GROUP_STATE_NO_ACTION;
            break;

        case circulator:
            //Serial.printf("LED mode changed to circulator %d\r\n", millis());
            //effect = circulator1;
            //fillLedColor ( CRGB::Black);
            //partialFillLEDcolor( (NUM_LEDS>>1) - 3, (NUM_LEDS>>1) + 2, ledColor);
            //FastLED.show();
            //fromLED = (NUM_LEDS>>1) + 3; // gateLED for starting fast movement
            //toLED = (NUM_LEDS>>1) - 3; // gateLED for starting fast movement
            //effectTime = 100;
            //lastEffectTime = millis();
            ledGroupStates[group].timeToRemain = 0;
            ledGroupStates[group].timeEntering = now - ledGroupStates[group].timeToRemain; // make it due now
            ledGroupStates[group].currentState = GROUP_STATE_NO_ACTION;
            break;

        case staticRainbow:
            //Serial.println("LED mode changed to staticRainbow");
            //fill_rainbow(leds, NUM_LEDS, 0, 20);
            //*pFirstLED, int numToFill, uint8_t initialhue, uint8_t deltahue=5)
            //effect = steady;
            //FastLED.show();
            ledGroupStates[group].timeToRemain = 0;
            ledGroupStates[group].timeEntering = now - ledGroupStates[group].timeToRemain; // make it due now
            ledGroupStates[group].currentState = GROUP_STATE_NO_ACTION;
            break;

        case staticWheel:
            //Serial.println("LED mode changed to staticWheel");
            //fillWheel();
            //FastLED.show();
            ledGroupStates[group].timeToRemain = 0;
            ledGroupStates[group].timeEntering = now - ledGroupStates[group].timeToRemain; // make it due now
            ledGroupStates[group].currentState = GROUP_STATE_NO_ACTION;
            break;

        case colorWave:
            //Serial.println("LED mode changed to colorwave");
            //fillWheel();
            //FastLED.show();
            ledGroupStates[group].timeToRemain = 0;
            ledGroupStates[group].timeEntering = now - ledGroupStates[group].timeToRemain; // make it due now
            ledGroupStates[group].currentState = GROUP_STATE_NO_ACTION;
            break;

        case clockFace:
            //Serial.println("LED mode changed to clockface");
            //effect = clockEffect;
            //effectTime = 30;
            //lastEffectTime = millis();
            ledGroupStates[group].timeToRemain = 0;
            ledGroupStates[group].timeEntering = now - ledGroupStates[group].timeToRemain; // make it due now
            ledGroupStates[group].currentState = GROUP_STATE_NO_ACTION;
            break;

        case marqueeLeft:
            //Serial.println("LED mode changed to marquee");
/*
            fillLedColor( CRGB::Black);
            for (int i=0; i< NUM_LEDS; i=i+4)
            {
                leds[i] = ledColor;
                leds[i+1] = ledColor;
            }
            FastLED.show();
            effect = rotateLeftEffect;
            effectTime = 250;
            lastEffectTime = 0; // make it due Now!  sort of
*/
            ledGroupStates[group].timeToRemain = 0;
            ledGroupStates[group].timeEntering = now - ledGroupStates[group].timeToRemain; // make it due now
            ledGroupStates[group].currentState = GROUP_STATE_NO_ACTION;
            break;

        case marqueeRight:
            //Serial.println("LED mode changed to marquee");
/*
            fillLedColor( CRGB::Black);
            for (int i = 0; i < NUM_LEDS; i = i + 4)
            {
                leds[i] = ledColor;
                leds[i+1] = ledColor;
            }
            FastLED.show();
            effect = rotateRightEffect;
            effectTime = 250;
            lastEffectTime = 0; // make it due Now!  sort of
*/
            ledGroupStates[group].timeToRemain = 0;
            ledGroupStates[group].timeEntering = now - ledGroupStates[group].timeToRemain; // make it due now
            ledGroupStates[group].currentState = GROUP_STATE_NO_ACTION;
            break;

        case singleColorBreath:
            //Serial.println("LED mode changed to singleColorBreath");
/*
            fillLedColor( ledColor);
            FastLED.show();
            effect = breathe;
            lastEffectTime = millis();
            effectTime = breathStepTime;
*/
            ledGroupStates[group].timeToRemain = 0;
            ledGroupStates[group].timeEntering = now - ledGroupStates[group].timeToRemain; // make it due now
            ledGroupStates[group].currentState = GROUP_STATE_NO_ACTION;
            break;

        case flashSynched:
            Serial.println("LED mode changed to flashSynched");
            effect = steady;
            // use interpreted flash sequence
            for (int i = 0; i < NUM_LEDS; i++)
            {
                ledStates[i].currentState = LED_STATE_ENTRY;
                ledStates[i].currentCommandArray = flash250;
                ledStates[i].currentCommandArraySize = sizeof(flash250)/sizeof(int);
                ledStates[i].currentCommandIndex = 0;
                ledStates[i].timeToRemain = 1;
                //printStateInfo( &ledStates[i]);
            }
            break;

        case flashPhased:
            Serial.println("LED mode changed to flashPhased");
            effect = steady;
            for (int i = 0; i < NUM_LEDS; i++)
            {
              ledStates[i].currentState = LED_STATE_ENTRY;
              ledStates[i].currentCommandArray = flash250;
              ledStates[i].currentCommandArraySize = sizeof(flash250)/sizeof(int);
              ledStates[i].currentCommandIndex = 0;
              ledStates[i].timeToRemain = i * 250/24 + 1;
            }
            break;

        case winkSynched:
            Serial.println("LED mode changed to winkSynched");
            effect = steady;
            for (int i = 0; i < NUM_LEDS; i++)
            {
              ledStates[i].currentState = LED_STATE_ENTRY;
              ledStates[i].currentCommandArray = wink;
              ledStates[i].currentCommandArraySize = sizeof(wink)/sizeof(int);
              ledStates[i].currentCommandIndex = 0;
              ledStates[i].timeToRemain = 1;
            }
            break;

        case winkPhased:
            Serial.println("LED mode changed to winkPhased");
            effect = steady;
            for (int i = 0; i < NUM_LEDS; i++)
            {
              ledStates[i].currentState = LED_STATE_ENTRY;
              ledStates[i].currentCommandArray = wink;
              ledStates[i].currentCommandArraySize = sizeof(wink)/sizeof(int);
              ledStates[i].currentCommandIndex = 0;
              ledStates[i].timeToRemain = i * 30 + 1;
            }
            break;

        case blinkSynched:
            Serial.println("LED mode changed to blinkSynched");
            effect = steady;
            for (int i = 0; i < NUM_LEDS; i++)
            {
              ledStates[i].currentState = LED_STATE_ENTRY;
              ledStates[i].currentCommandArray = blink;
              ledStates[i].currentCommandArraySize = sizeof(blink)/sizeof(int);
              ledStates[i].currentCommandIndex = 0;
              ledStates[i].timeToRemain = 1;
            }
            break;

        case blinkPhased:
            Serial.println("LED mode changed to blinkPhased");
            effect = steady;
            for (int i = 0; i < NUM_LEDS; i++)
            {
              ledStates[i].currentState = LED_STATE_ENTRY;
              ledStates[i].currentCommandArray = blink;
              ledStates[i].currentCommandArraySize = sizeof(blink)/sizeof(int);
              ledStates[i].currentCommandIndex = 0;
              ledStates[i].timeToRemain = i * 30 + 1;
            }
            break;

        case meteorSynched:
            Serial.println("LED mode changed to meteorSynched");
            effect = steady;
            for (int i = 0; i < NUM_LEDS; i++)
            {
              ledStates[i].currentState = LED_STATE_ENTRY;
              ledStates[i].currentCommandArray = meteor;
              ledStates[i].currentCommandArraySize = sizeof(meteor)/sizeof(int);
              ledStates[i].currentCommandIndex = 0;
              ledStates[i].timeToRemain = 1;
            }
            break;

        case meteorPhased:
            Serial.println("LED mode changed to meteorPhased");
            effect = steady;
            for (int i = 0; i < NUM_LEDS; i++)
            {
              ledStates[i].currentState = LED_STATE_ENTRY;
              ledStates[i].currentCommandArray = meteor;
              ledStates[i].currentCommandArraySize = sizeof(meteor)/sizeof(int);
              ledStates[i].currentCommandIndex = 0;
              ledStates[i].timeToRemain = i * 30 + 1;
            }
            break;

        default:
            //Serial.println("bad attempt to change LED mode");
            break;
    }
}


int interpretLedCommandString( String commandString)
/*
 * this function interprets a command string to control LED mode, color,
 * speed, direction, and brightness.
 */
{
    int value;
    int speed;
    int direction;
    String mode;
    int ptr;
    #define lenTempStr 40
    char tempStr [ lenTempStr];
    //split command string into words
    /*
    single letter command for now
    1 on
    0 off
    bxxx brightness
    mxxx mode
    sxxx speed
    dxxx direction
    crrrbbbggg color
    rxxxgxxxbxxx RGb color
    */
    Serial.print( "Interpreting commandString:");
    Serial.println (commandString);
    ptr = 0;
    char command = commandString.charAt( ptr);

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
                break;

            case 'l': //luminence (brightness)%
                if (value > 100)
                {
                    value = 100;
                }
                LEDbrightness = value;
                FastLED.setBrightness( (int) (LEDbrightness*255/100));
                FastLED.show();
                snprintf( tempStr, lenTempStr, "Brightness: %d", LEDbrightness);
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
                        setLEDMode( selectedGroup, i);
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
                ledColor.r = value;
                snprintf( tempStr, lenTempStr, "Red: %d", value);
                //Serial.println(tempStr);
                break;

            case 'b': //b 0..255
                if (value > 255)
                {
                    value = 255;
                }
                ledColor.b = value;
                snprintf( tempStr, lenTempStr, "Blue: %d", value);
                //Serial.println(tempStr);
                break;

            case 'g': //green 0..255
                if (value > 255)
                {
                    value = 255;
                }
                ledColor.g = value;
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

    // setup for web controls
    Particle.function ("ledControl", interpretLedCommandString);
    //Particle.function ("ledColor", ledColorFunction);
    //Particle.function ("ledMode", ledModeFunction);
    //Particle.function ("ledSpeed", ledSpeedFunction);
    //Particle.function ("ledDirection", ledDirectionFunction);
    //setLEDMode( quadFlasher);
    //setLEDMode( quadFlasher2);

    // setup group 1
    ledGroupStates[0].numLeds = NUM_LEDS;
    for (int i = 0; i < NUM_LEDS; i++)
    {
        ledGroupStates[0].members[i] = i;
    }
    setLEDMode( 0, clockFace);
}


void ledLoop() // handle LED effects
{

    //effect state machine -- handle various effects (flashing, fading, etc.)
    switch (effect)
    {
        case offEffect:
        case steady:
            break;

        case fadeToBlack:
            if( millis() - lastEffectTime >= effectTime)
            {
                if (FastLED.getBrightness() >0)
                {
                    FastLED.setBrightness( FastLED.getBrightness() - 1);
                }
                lastEffectTime = millis();
                FastLED.show();
            }
            break;

        case fadeToTarget:
            if( millis() - lastEffectTime >= effectTime)
            {
                if (FastLED.getBrightness() < LEDbrightness)
                {
                    FastLED.setBrightness( FastLED.getBrightness() + 1);
                }
                lastEffectTime = millis();
                FastLED.show();
            }
            break;

        case rotateLeftEffect:
            if( millis() - lastEffectTime >= effectTime)
            {
                rotateLeft();
                lastEffectTime = millis();
                FastLED.show();
            }
            break;

        case rotateRightEffect:
            if( millis() - lastEffectTime >= effectTime)
            {
                rotateRight();
                lastEffectTime = millis();
                FastLED.show();
            }
            break;

        case breathe:
            if( millis() - lastEffectTime >= effectTime)
            {
                FastLED.setBrightness( (int) min(255, ((LEDbrightness *
                        quadwave8( breathIndex) / 100)+ledBias)));
                breathIndex = breathIndex + breathStep;
                FastLED.show();
                lastEffectTime = millis();
            }
            break;

        case twinkle:
            if( millis() - lastEffectTime >=effectTime)
            {
                /*
                slow to start
                slow to change
                eventually loses the black dot??
                */
                int saveLed = rand () % NUM_LEDS;
                leds[ saveLed] = CRGB::Black;
                FastLED.show();
                delay(rand() % 1000 + 500);
                leds[ saveLed] = rand();
                effectTime = rand() % 2000 + 500;
                lastEffectTime = millis();
                FastLED.show();
            }
            break;

        case flashing:
            if( millis() - lastEffectTime >= effectTime)
            {
                //Serial.printf("executing flashing %d %s %d\r\n",
                //        millis(), leds[0] ? "1" : "0", effectTime);
                if (leds[0])
                {
                    fillLedColor ( CRGB::Black);
                }
                else
                {
                    fillLedColor ( ledColor);
                }
                FastLED.show();
                //lastEffectTime = millis();
                lastEffectTime += effectTime; // keep time from slipping
            }
            break;

        case quadFlash1:
            if( millis() - lastEffectTime >= effectTime)
            {
                //Serial.printf("executing quadFlash1 %d\r\n", millis());
                ////Serial.printf("executing quadFlash1 %d %d", NUM_LEDS>>2); // 1/4
                ////Serial.printf("executing quadFlash1 %d %d", NUM_LEDS>>1); // 1/2
                ////Serial.printf("executing quadFlash1 %d %d", (NUM_LEDS>>1) + (NUM_LEDS>>2)); //  3/4
                ////Serial.printf("executing quadFlash1 %d %d", NUM_LEDS);  // 1
                ////Serial.println();
                fillLedColor ( CRGB::Black);
                partialFillLEDcolor( NUM_LEDS>>2, NUM_LEDS>>1, ledColor); // 1/4..1/2
                partialFillLEDcolor( (NUM_LEDS>>1) + (NUM_LEDS>>2), NUM_LEDS, ledColor); // 3/4..1
                lastEffectTime = millis();
                effect = quadFlash2;
                FastLED.show();
             }
             break;

        case quadFlash2:
            if( millis() - lastEffectTime >= effectTime)
            {
                //Serial.printf("executing quadFlash2 %d\r\n", millis());
                ////Serial.println();
                fillLedColor ( CRGB::Black);
                partialFillLEDcolor( 0, NUM_LEDS>>2, ledColor); // 0..1/4
                partialFillLEDcolor( NUM_LEDS>>1, (NUM_LEDS>>1) + (NUM_LEDS>>2), ledColor); // 1/2..3/4
                lastEffectTime = millis();
                effect = quadFlash1;
                FastLED.show();
            }
            break;

        case quadFlashChase1:
            if( millis() - lastEffectTime >= effectTime)
            {
                //Serial.printf("executing quadFlashChase1 %d %d %d %d %0.3f\r\n",
                //        onLED, fromLED, toLED, endLED, millis());
                fillLedColor ( CRGB::Black);
                partialFillLEDcolor( NUM_LEDS_Q2, NUM_LEDS_Q3, ledColor);
                partialFillLEDcolor( NUM_LEDS_Q4, NUM_LEDS_Q5, ledColor);
                endLED = NUM_LEDS_Q2;
                fromLED = NUM_LEDS_Q3 - 1;
                toLED = NUM_LEDS_Q4 - 1;
                onLED = fromLED;
                lastEffectTime = millis();
                effectTime = 30;
                effect = quadFlashChase2;
            }
            break;

        case quadFlashChase2:
            if( millis() - lastEffectTime >= effectTime)
            {
                ////Serial.printf("executing quadFlashChase2 %d %d %d %d %0.3f\r\n", onLED, fromLED, toLED, endLED, millis());
                ////Serial.println("");
                //move one led on path to nirva
                if (onLED == toLED)
                {
                    // done moving this LED
                    onLED = fromLED;
                    if (fromLED == endLED) //done moving
                    {
                        lastEffectTime = millis();
                        effectTime = 250;
                        effect = quadFlashChase3;
                    }
                    else
                    {
                        // back fromLED and toLED for the next LED to move
                        fromLED = (fromLED - 1 + NUM_LEDS) % NUM_LEDS;
                        toLED = (toLED - 1 + NUM_LEDS) % NUM_LEDS;
                    }
                }
                else
                {
                    leds[ onLED] = CRGB::Black;
                    leds[ (onLED + (NUM_LEDS>>1)) % NUM_LEDS] = CRGB::Black;
                    onLED = (onLED + 1) % NUM_LEDS;
                }
                if (onLED != fromLED) {
                    ////Serial.printf("turning on %d %d\r\n", onLED, (onLED + NUM_LEDS_Q3) % NUM_LEDS);
                    ////Serial.println("");
                    leds[ onLED] = ledColor;
                    leds[ (onLED + NUM_LEDS_Q3) % NUM_LEDS] = ledColor;
                    FastLED.show();
                }
                lastEffectTime = millis();
            }
            break;

        case quadFlashChase3:
            if( millis() - lastEffectTime >= effectTime)
            {
                //Serial.printf("executing quadFlashChase3 %d %d %d %d %0.3f\r\n",
                //        onLED, fromLED, toLED, endLED, millis());
                fillLedColor ( CRGB::Black);
                partialFillLEDcolor( NUM_LEDS_Q1, NUM_LEDS_Q2, ledColor);
                partialFillLEDcolor( NUM_LEDS_Q3, NUM_LEDS_Q4, ledColor);
                endLED = NUM_LEDS_Q1;
                fromLED = NUM_LEDS_Q2 - 1;
                toLED = NUM_LEDS_Q3 - 1;
                onLED = fromLED;
                lastEffectTime = millis();
                effectTime = 30;
                effect = quadFlashChase4;
            }
            break;

        case quadFlashChase4:
            if( millis() - lastEffectTime >= effectTime)
            {
                ////Serial.printf("executing quadFlashChase4 %d %d %d %d %0.3f\r\n", onLED, fromLED, toLED, endLED, millis());
                //move one led on path to nirva
                if (onLED == toLED)
                {
                    // done moving this LED
                    onLED = fromLED;
                    if (fromLED == endLED)
                    {
                        lastEffectTime = millis();
                        effectTime = 250;
                        effect = quadFlashChase1;
                    }
                    else
                    {
                        // back fromLED and toLED for the next LED to move
                        fromLED = (fromLED - 1 + NUM_LEDS) % NUM_LEDS;
                        toLED = (toLED - 1 + NUM_LEDS) % NUM_LEDS;
                    }
                }
                else
                {
                    leds[ onLED] = CRGB::Black;
                    leds[ (onLED + (NUM_LEDS>>1)) % NUM_LEDS] = CRGB::Black;
                    onLED = (onLED + 1) % NUM_LEDS;
                }
                if (onLED != fromLED) {
                    ////Serial.printf("turning on %d %d\r\n", onLED, (onLED + NUM_LEDS_Q3) % NUM_LEDS);
                    ////Serial.println("");
                    leds[ onLED] = ledColor;
                    leds[ (onLED + NUM_LEDS_Q3) % NUM_LEDS] = ledColor;
                    FastLED.show();
                }
                lastEffectTime = millis();
            }
            break;

        case octoFlash1:
            if( millis() - lastEffectTime >= effectTime)
            {
                //Serial.printf("executing octoFlash1 %d\r\n", millis());
                fillLedColor ( CRGB::Black);
                partialFillLEDcolor( NUM_LEDS_82, NUM_LEDS_83, ledColor); // 1/8..1/4
                partialFillLEDcolor( NUM_LEDS_84, NUM_LEDS_85, ledColor); // 3/8..1/2
                partialFillLEDcolor( NUM_LEDS_86, NUM_LEDS_87, ledColor); // 5/8..3/4
                partialFillLEDcolor( NUM_LEDS_88, NUM_LEDS_89, ledColor); // 7/8..1
                FastLED.show();
                lastEffectTime += effectTime;
                effect = octoFlash2;
             }
             break;

        case octoFlash2:
            if( millis() - lastEffectTime >= effectTime)
            {
                //Serial.printf("executing octoFlash2 %d\r\n", millis());
                fillLedColor ( CRGB::Black);
                partialFillLEDcolor( NUM_LEDS_81, NUM_LEDS_82, ledColor); // 0..1/8
                partialFillLEDcolor( NUM_LEDS_83, NUM_LEDS_84, ledColor); // 1/4..3/8
                partialFillLEDcolor( NUM_LEDS_85, NUM_LEDS_86, ledColor); // 1/2..5/8
                partialFillLEDcolor( NUM_LEDS_87, NUM_LEDS_88, ledColor); // 3/4..7/8
                lastEffectTime += effectTime;
                effect = octoFlash1;
                FastLED.show();
            }
            break;

        case clockEffect:
            if(millis() - effectTime > 1000)
            {
                float hour = ((Time.hour() % 12) + Time.minute() / 60 + .5) * NUM_LEDS / 12;
                float minute = (Time.minute() + Time.second() / 60 + .5) * NUM_LEDS / 60;
                float second = (Time.second() * NUM_LEDS  + .5)  / 60;
                //draw the hands
                fillLedColor ( CRGB::Black);
                leds[ ((int) hour + 1) % NUM_LEDS] = CRGB::Red;
                leds[ (int) hour] = CRGB::Red;
                leds[ ((int) hour - 1) % NUM_LEDS] = CRGB::Red;
                leds[ (int) minute] = CRGB::Green;
                leds[ ((int) minute - 1) % NUM_LEDS] = CRGB::Green;
                leds[ (int) second] = CRGB::Blue;
                effectTime = millis();
                FastLED.show();
            }
            break;

        case circulator1:
            // move dots at a slow rate
            if( millis() - lastEffectTime >= effectTime)
            {
                //Serial.printf("executing circulator1 %d\r\n", millis());
                lastEffectTime = millis();
                rotateRight();
                if ( leds[ fromLED])
                {
                    effect = circulator2;
                    //effectTime = effectTime>>1;
                    effectTime = 30;
                }
                FastLED.show();
            }
            break;

        case circulator2:
            // move dots at at faster rate
            if( millis() - lastEffectTime >= effectTime)
            {
                //Serial.printf("executing circulator2 %d\r\n", millis());
                lastEffectTime = millis();
                rotateRight();
                if ( leds[ toLED])
                {
                    effect = circulator1;
                    //effectTime = effectTime<<1;
                    effectTime = 120;
                }
                FastLED.show();
            }
            break;

        case huh:
            //this doesn't do what I would like it to do, it just quickly turns light off
            if( false && millis() - ledTime >= 300)
            {
                for(int dot = 0; dot < NUM_LEDS; dot++)
                {
                    int tempDot = leds[dot];
                    leds[dot] = CRGB::Black;
                    FastLED.show();
                    // clear this led for the next time around the loop
                    leds[dot] = tempDot;
                }
                ledTime = millis(); //update ledTime to current millis()
                FastLED.show();
            }
            break;

        default:
            break;
    }
}

void setup()
{
    Serial.begin(9600);
    Serial.println("Serial monitor started");


    lastEffectTime = millis();
    LEDbrightness = 1;
    FastLED.setBrightness( (int) (LEDbrightness*255/100));
    breathIndex = 0;

    ledSetup();
}

void loop()
{
    ledLoop(); // handle LED group effects
    groupFSM(); // handle group LED effects
    ledFSM(); // handle individual LED effects
}
