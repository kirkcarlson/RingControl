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

//---- LED only stuff follows
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
    "singleColorBreath"
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


//**** GLOBALS ****

CRGB leds[NUM_LEDS];
CRGB targetColors[NUM_LEDS];
CRGB startColors[NUM_LEDS];


int LEDbrightness; // level of over all brightness desired 0..100 percent
int LEDPoint = 0; // 0..23 LED direction for selected modes
//int LEDSpeed = 0; // 0..100 speed of LED display mode
CRGB ledColor = CRGB::White; // color of the LEDs

LEDModes LEDMode = offMode; // LED display mode, see above
LEDModes lastLEDMode = offMode;

char charLedModes [100]; // this is referenced but not initiated?

int steps = 0;
int stepNumber = 0;
effects effect = steady;
int LEDDirection = 0;

#define bpm 10 /* breaths per minute. 12 to 16 for adult humans */
#define numBreathSteps 255
#define ledBias 2 /* minimum led value during sine wave */
int breathStep = 255/numBreathSteps;
//int breathSteps [] = { 0, 5, 10, 20, 35, 55, 85, 130, 255 }; // n/255
//int numBreathSteps = sizeof( breathSteps) / sizeof( int);
int breathStepTime = 60000/ bpm / numBreathSteps;
uint8_t breathIndex = 0; // state of the breath
int onLED = 0;
int endLED = 0;
int fromLED = 0;
int toLED = 0;
//int startLevel = 0; // not referenced

unsigned long ledTime = 0;
unsigned long lastBreathTime = 0;

// should the following be unsigned long??
// can some of them be combined??
int twinkleTime = 0;
int lastTwinkleTime = 0;
int effectTime = 0;
int lastEffectTime = 0;
int fadeTime = 0;
int lastFadeTime = 0;
int clockTime = 0;
int lastClockTime = 0;

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

// command arrays
// these loop unless directed not to with LED_STOP
int flash250 [] = { LED_ON_TIMED, 250, LED_OFF_TIMED, 250};
int wink [] = { LED_OFF_TIMED, 100, LED_STOP};
int blink [] = { LED_ON_TIMED, 100, LED_STOP};
int meteor [] = { LED_ON, 60, ledColor, LED_FADE_TO_BLACK, 200, LED_STOP};


typedef struct {
    int ledNumber;              // number of LED being controlled
    ledCommand currentCommand;  // current command being executed
    int *currentCommandArray;   // pointer to array of commands
    int currentCommandArraySize; // number of commands in current array
    int currentCommandIndex;    // index into current array of commands
    ledState currentState;      // current LED state
    byte fadeFactor;            // color is proportioned fadeFactor/256
    CRGB fromColor;
    CRGB toColor;
    unsigned long timeEntering; // time upon entering the current state
    int timeToRemain;           // time to remain in the current state
} LedState;


LedState ledStates [ NUM_LEDS];


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


// **** FUNCTIONS ****

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
        //Serial.printf ("fillLedColor %d R%d G%d B%d\r\n", i, color.r, color.g, color.b);
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
        Serial.printf("fillWheel %d %d R%d G%d B%d\r\n", i, i * 255 / NUM_LEDS, col.r, col.g, col.b);
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


void printStateInfo( LedState state)
{
    printf("Led:%d state:%d array:%d array size:%d arrayIndex:%d\r\n",
            state.ledNumber,
            state.currentState,
            state.currentCommandArray,
            state.currentCommandArraySize,
            state.currentCommandIndex);
}


void setLEDMode( int mode)
/*
 * this function changes the current mode of the LED display and sets up
 * the effect(s) to be applied over time.
 */
{
    Serial.println(ledModeNames[mode]);
    Serial.println(ledModeNames[mode].length());
    char modeName [ledModeNames[mode].length() + 1]; //include terminating null
    ledModeNames[ mode].toCharArray(modeName, ledModeNames[mode].length()+1);\
    //modeName[sizeof(modeName) -1] = 0;
    Serial.printf("attempting LED mode change %d %s\r\n", mode, modeName);

    switch(  mode)
    {
        case offMode:
            Serial.println("LED mode changed to offEffect");
            effect = offEffect;
            break;

        case singleColor:
            Serial.println("LED mode changed to singleColor");
            Serial.printf("R%d G%d B%d\r\n", ledColor.r, ledColor.g, ledColor.b);
            fillLedColor( ledColor);
            effect = steady;
            FastLED.show();
            break;

        case staticMultiColor:
            Serial.println("LED mode changed to staticMultiColor");
            fillLedMultiColor();
            effect = steady;
            FastLED.show();
            break;

        case twinklingMultiColor:
            Serial.println("LED mode changed to twinklingMultiColor");
            fillLedMultiColor();
            effect = twinkle;
            FastLED.show();
            break;

        case allFlash:
            Serial.println("LED mode changed to allFlash");
            effect = flashing;
            effectTime = 250;
            lastEffectTime = millis();
            break;

        case quadFlasher:
            Serial.println("LED mode changed to quadFlasher");
            effect = quadFlash1;
            effectTime = 500;
            lastEffectTime = 0; // make it due Now! sort of
            break;

        case quadFlasher2:
            Serial.println("LED mode changed to quadflasher2");
            effect = quadFlashChase1;
            effectTime = 250;
            lastEffectTime = 0; // make it due Now!  sort of
            break;

        case octoFlasher:
            Serial.println("LED mode changed to octoFlasher");
            effect = octoFlash1;
            effectTime = 250;
            lastEffectTime = millis();
            break;

        case circulator:
            Serial.printf("LED mode changed to circulator %d\r\n", millis());
            effect = circulator1;
            fillLedColor ( CRGB::Black);
            partialFillLEDcolor( (NUM_LEDS>>1) - 3, (NUM_LEDS>>1) + 2, ledColor);
            FastLED.show();
            fromLED = (NUM_LEDS>>1) + 3; // gateLED for starting fast movement
            toLED = (NUM_LEDS>>1) - 3; // gateLED for starting fast movement
            effectTime = 100;
            lastEffectTime = millis();
            break;

        case staticRainbow:
            Serial.println("LED mode changed to staticRainbow");
            fill_rainbow(leds, NUM_LEDS, 0, 20);
            //*pFirstLED, int numToFill, uint8_t initialhue, uint8_t deltahue=5)
            effect = steady;
            FastLED.show();
            break;

        case staticWheel:
            Serial.println("LED mode changed to staticWheel");
            fillWheel();
            FastLED.show();
            break;

        case colorWave:
            Serial.println("LED mode changed to colorwave");
            fillWheel();
            FastLED.show();
            break;

        case clockFace:
            Serial.println("LED mode changed to clockface");
            effect = clockEffect;
            clockTime = 30;
            lastClockTime = millis();
            break;

        case marqueeLeft:
            Serial.println("LED mode changed to marquee");
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
            break;

        case marqueeRight:
            Serial.println("LED mode changed to marquee");
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
            break;

        case singleColorBreath:
            Serial.println("LED mode changed to singleColorBreath");
            fillLedColor( ledColor);
            FastLED.show();
            effect = breathe;
            lastBreathTime = millis();
            //breathStepTime = 30;
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
                printf("state: %d array:%d array size:%d arrayIndex:%d\r\n",
                    ledStates[i].currentState,
                    ledStates[i].currentCommandArray,
                    ledStates[i].currentCommandArraySize,
                    ledStates[i].currentCommandIndex);
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
              ledStates[i].timeToRemain = i * 30 + 1;
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
            Serial.println("bad attempt to change LED mode");
            break;
    }
}


int getNextCommand ( LedState state)
// returns the next command for a particular state and updates pointers
{
    int *commands = state.currentCommandArray;
    int command = commands[ state.currentCommandIndex];
    state.currentCommandIndex++;
    if (state.currentCommandIndex >= state.currentCommandArraySize);
    {
        state.currentCommandIndex = 0;
    }
    return command;
}


void interpretNextCommand ( LedState state, unsigned long entryTime)
{
    switch (getNextCommand( state))
    {
        case LED_ON:
            Serial.println("LED ON command");
            printStateInfo( state);
            //turn LED on without timer
            leds[ state.ledNumber] = ledColor;
            state.timeToRemain = 0;
            state.currentState = LED_STATE_NULL;
            break;

        case LED_OFF:
            Serial.println("LED OFF command");
            printStateInfo( state);
            //turn LED off without timer
            leds[ state.ledNumber] = CRGB::Black;
            state.timeToRemain = 0;
            state.currentState = LED_STATE_NULL;
            break;

        case LED_ON_TIMED:
            Serial.println("LED ON TIMED command");
            //turn LED on with timer
            leds[ state.ledNumber] = ledColor;
            state.timeEntering = entryTime;
            state.timeToRemain = getNextCommand( state);
            state.currentState = LED_STATE_ON;
            break;

        case LED_OFF_TIMED:
            Serial.println("LED OFF TIMED command");
            leds[ state.ledNumber] = CRGB::Black;
            state.timeEntering = entryTime;
            state.timeToRemain = getNextCommand( state);
            state.currentState = LED_STATE_OFF;
            break;

        case LED_TIMED_COLOR:
            Serial.println("LED TIMED COLOR command");
            state.timeEntering = entryTime;
            state.timeToRemain = getNextCommand( state);
            leds[ state.ledNumber] = getNextCommand( state);
            state.currentState = LED_STATE_ON;
            break;

        case LED_FADE_TO_BLACK:
            Serial.println("LED FADE TO BLACK command");
            state.timeEntering = entryTime;
            state.timeToRemain = getNextCommand( state)/256; // always 255 steps for now
            state.fadeFactor = 255;
            state.currentState = LED_STATE_FADING;
            break;

        case LED_FADE_TO_COLOR:
            Serial.println("LED FADE TO COLOR command");
            state.timeEntering = entryTime;
            state.timeToRemain = getNextCommand( state)/255; // always 255 steps for now
            state.fadeFactor = 255;
            state.fromColor = leds[ state.ledNumber];
            state.toColor = getNextCommand( state);
            state.currentState = LED_STATE_FADING;
            break;

        case LED_STOP:
            Serial.println("LED STOP command");
            state.currentState = LED_STATE_NULL;
            state.timeToRemain = 0;
            break;

        default:
            break;
    }
}


void ledFSM ()
// process timing events for individual LEDs and maintain their state
{
    unsigned long now = millis();
    //for all LEDs
    Serial.printf( "top of ledFSM %d\r\n", now);
    for (int i = 0; i< NUM_LEDS; i++)
    {
        //switch to current state of LED

        switch (ledStates[i].currentState)
        {
            case LED_STATE_ENTRY:
                // should only get here on FSM start up
                // get first command and off we go
                interpretNextCommand (ledStates[i], now); // set up next state
                break;

            case LED_STATE_ON:
            case LED_STATE_OFF:
                // if timer has expired
                if (ledStates[i].timeToRemain > 0 &&
                        now - ledStates[i].timeEntering > ledStates[i].timeToRemain)
                {
                    interpretNextCommand (ledStates[i], now); // set up next state
                }
                break;

            case LED_STATE_FADING:
                if (ledStates[i].timeToRemain > 0 &&
                        now - ledStates[i].timeEntering > ledStates[i].timeToRemain)
                {
                    // fade a bit more toward target
                    leds[i].nscale8( ledStates[i].fadeFactor);
                    ledStates[i].fadeFactor--;
                    if (ledStates[i].fadeFactor == 0)
                    {
                        interpretNextCommand (ledStates[i], now); // set up next state
                    }
                    else
                    {
                        ledStates[i].timeEntering = now;
                        // go around again
                    }
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
    Serial.println( "bottom of ledFSM");
}


int interpretLedStringCommand(String commandString)
/*
 * this function interprets a command string to control LED mode, color,
 * speed, direction, and brightness.
 */
{
    int value;
    int speed;
    int direction;
    String mode;
    int red;
    int green;
    int blue;
    int ptr;
    int result;
    const int lenTempStr = 40;
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
    Serial.println(commandString);
    ptr = 0;
    char command = commandString[ ptr];

    while (command > 0)
    {
        if (command != 'n' && command != 'f' && command != 'm')
        {
            value = 0;
            for (int i = ptr + 1; i < ptr + 4; i++)
            {
                if ( commandString[i] == 0 )
                {
                    break;

                }
                else
                {
                    value = 10* value + (int) commandString[i] - (int) '0';
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
            case 'n': //on
                Serial.println("on");
                functionOnOff( 3, true, "");
                break;

            case 'f': //off
                Serial.println("off");
                functionOnOff( 3, false, "");
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
                Serial.println(tempStr);
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
                Serial.println(mode);
                for (int i = 0; i <  NUM_LED_MODES; i++)
                {
                    if ( commandString.substring(ptr).compareTo(ledModeNames[i]) == 0)
                    {
                        setLEDMode( i);
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
                Serial.println(tempStr);
                break;

            case 'd': //direction 0..359
                if (value > 359)
                {
                    value = 0;
                }
                direction = value;
                snprintf( tempStr, lenTempStr, "Direction: %d", direction);
                Serial.println(tempStr);
                break;

            case 'r': //red 0..255
                if (value > 255)
                {
                    value = 255;
                }
                ledColor.r = value;
                snprintf( tempStr, lenTempStr, "Red: %d", value);
                Serial.println(tempStr);
                break;

            case 'b': //b 0..255
                if (value > 255)
                {
                    value = 255;
                }
                ledColor.b = value;
                snprintf( tempStr, lenTempStr, "Blue: %d", value);
                Serial.println(tempStr);
                break;

            case 'g': //green 0..255
                if (value > 255)
                {
                    value = 255;
                }
                ledColor.g = value;
                snprintf( tempStr, lenTempStr, "Green: %d", value);
                Serial.println(tempStr);
                break;

            // w -speed
            // x +speed
            // y -dir
            // z +dir
            default:
                return -1; //bad command
                break;
        }
        command = commandString[ ptr];
    }
    Serial.println("Done");
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
    Particle.function ("ledControl", interpretLedStringCommand);
    //Particle.function ("ledColor", ledColorFunction);
    //Particle.function ("ledMode", ledModeFunction);
    //Particle.function ("ledSpeed", ledSpeedFunction);
    //Particle.function ("ledDirection", ledDirectionFunction);
    //setLEDMode( quadFlasher);
    //setLEDMode( quadFlasher2);
    setLEDMode( clockFace);
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
            if( millis() - lastFadeTime >= fadeTime)
            {
                if (FastLED.getBrightness() >0)
                {
                    FastLED.setBrightness( FastLED.getBrightness() - 1);
                }
                /* why not just reduce brightness until 0
                for(int i = 0; i < NUM_LEDS; i++) {
                    leds[i].r = startColors[i].r * stepNumber/steps;
                    leds[i].g = startColors[i].g * stepNumber/steps;
                    leds[i].b = startColors[i].b * stepNumber/steps;
                }
                if (stepNumber == 0)
                {
                    effect = steady;
                }
                else
                {
                    stepNumber = stepNumber - 1;
                }
                */
                lastFadeTime = millis();
                FastLED.show();
            }
            break;

        case fadeToTarget:
            if( millis() - lastFadeTime >= fadeTime)
            {
                if (FastLED.getBrightness() < LEDbrightness)
                {
                    FastLED.setBrightness( FastLED.getBrightness() + 1);
                }
                /* why not just increase brightness until at desired level
                for(int i = 0; i < NUM_LEDS; i++) {
                    // this addition should be different...
                    leds[i].r = startColors[i].r * stepNumber/steps + targetColors[i].r * (steps - stepNumber)/steps;
                    leds[i].g = startColors[i].g * stepNumber/steps + targetColors[i].g * (steps - stepNumber)/steps;
                    leds[i].b = startColors[i].b * stepNumber/steps + targetColors[i].b * (steps - stepNumber)/steps;
                }
                if (stepNumber == 0)
                {
                    effect = steady;
                }
                else
                {
                    stepNumber = stepNumber - 1;
                }
                */
                lastFadeTime = millis();
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
            if( millis() - lastBreathTime >= breathStepTime)
            {
                FastLED.setBrightness( (int) min(255, ((LEDbrightness * quadwave8( breathIndex) / 100)+ledBias)));
                breathIndex = breathIndex + breathStep;
                FastLED.show();
                lastBreathTime = millis(); //update lastBreathTime to current millis()
            }
            break;

        case twinkle:
            if( millis() - lastTwinkleTime >= twinkleTime)
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
                twinkleTime = rand() % 2000 + 500;
                lastTwinkleTime = millis();
                FastLED.show();
            }
            break;

        case flashing:
            if( millis() - lastEffectTime >= effectTime)
            {
                Serial.printf("executing flashing %d %s %d\r\n",
                        millis(), leds[0] ? "1" : "0", effectTime);
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
                Serial.printf("executing quadFlash1 %d\r\n", millis());
                //Serial.printf("executing quadFlash1 %d %d", NUM_LEDS>>2); // 1/4
                //Serial.printf("executing quadFlash1 %d %d", NUM_LEDS>>1); // 1/2
                //Serial.printf("executing quadFlash1 %d %d", (NUM_LEDS>>1) + (NUM_LEDS>>2)); //  3/4
                //Serial.printf("executing quadFlash1 %d %d", NUM_LEDS);  // 1
                //Serial.println();
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
                Serial.printf("executing quadFlash2 %d\r\n", millis());
                //Serial.println();
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
                Serial.printf("executing quadFlashChase1 %d %d %d %d %0.3f\r\n",
                        onLED, fromLED, toLED, endLED, millis());
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
                //Serial.printf("executing quadFlashChase2 %d %d %d %d %0.3f\r\n", onLED, fromLED, toLED, endLED, millis());
                //Serial.println("");
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
                    //Serial.printf("turning on %d %d\r\n", onLED, (onLED + NUM_LEDS_Q3) % NUM_LEDS);
                    //Serial.println("");
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
                Serial.printf("executing quadFlashChase3 %d %d %d %d %0.3f\r\n",
                        onLED, fromLED, toLED, endLED, millis());
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
                //Serial.printf("executing quadFlashChase4 %d %d %d %d %0.3f\r\n", onLED, fromLED, toLED, endLED, millis());
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
                    //Serial.printf("turning on %d %d\r\n", onLED, (onLED + NUM_LEDS_Q3) % NUM_LEDS);
                    //Serial.println("");
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
                Serial.printf("executing octoFlash1 %d\r\n", millis());
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
                Serial.printf("executing octoFlash2 %d\r\n", millis());
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
            if(millis() - clockTime > 1000)
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
                clockTime = millis();
                FastLED.show();
            }
            break;

        case circulator1:
            // move dots at a slow rate
            if( millis() - lastEffectTime >= effectTime)
            {
                Serial.printf("executing circulator1 %d\r\n", millis());
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
                Serial.printf("executing circulator2 %d\r\n", millis());
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

// --- other control stuff
// **** INCLUDES ****
#include <OneWire.h>
#include <DS18.h>
#include <Adafruit_ST7735.h>
#include <EchoPhotonBridge.h>


// **** CONSTANTS ****

#define FAN D0
#define ONEWIRE D2
#define BL  D1
#define CS  A2
#define DC  D4
#define RST A1

#define TEMPLOCKTIME 30 //30 minutes

enum devices {
    fanDevice,
    lcdBacklightDevice,
    thermostatDevice,
    ledLightDevice,
    ledModeDevice,
    ledDirectionDevice,
    ledSpeedDevice
};


// **** GLOBALS ****

int setTemp = 80;
int currentTemp;
String lastUpdate = "";
unsigned long oldTime = 0;
unsigned long tempLockTime = 0;
bool thermostat = true;


EchoPhotonBridge epb;
Adafruit_ST7735 tft = Adafruit_ST7735(CS, DC, RST);
DS18 sensor(ONEWIRE);


// **** FUNCTIONS ****

int functionOnOff(int device, bool onOff, String rawParameters)
{
    tft.setTextColor((onOff) ? ST7735_GREEN : ST7735_RED, ST7735_BLACK);
    tft.setTextSize(1);
    tft.setCursor(0,140);
    tft.fillRect(0,140,128,160,ST7735_BLACK);
    switch (device)
    {
        case fanDevice:
            analogWrite(FAN, (onOff) ? 250 : 0, 500);
            tft.println((onOff) ? "Fan On" : "Fan Off");
            tempLockTime = millis() + (1000 * 60 * TEMPLOCKTIME);
            updateThermostatDisplay("Paused");
            break;

        case lcdBacklightDevice:
            analogWrite(BL, (onOff) ? 250 : 0, 500);
            tft.println((onOff) ? "Backlight On" : "Backlight Off");
            break;

        case thermostatDevice:
            thermostat = onOff;
            updateThermostatDisplay((onOff) ? "On" : "Off");
            break;

        case ledLightDevice:
            if (onOff)
            {
                LEDMode = lastLEDMode;
            }
            else
            {
                if (lastLEDMode != offMode) // make sure that two offs don't wipe out lastNode
                {
                    lastLEDMode = LEDMode;
                }
                LEDMode = offMode;
            }
            /*
            for (int i = 0; i < NUM_LEDS; i++)
            {
                leds[i] = (onOff) ? CRGB::White : CRGB::Black;
            }
            if (onOff) {
                FastLED.setBrightness( (int) (LEDbrightness*255/100));
            }
            */
            tft.println((onOff) ? "Light On" : "Light Off");
            break;

    }
    return 0;
}


int functionPercent(int device, int percent, int changeAmount, String rawParameters)
{
    tft.setTextColor(ST7735_BLUE, ST7735_BLACK);
    tft.setTextSize(1);
    tft.setCursor(0,140);
    tft.fillRect(0,140,128,160,ST7735_BLACK);
    switch (device)
    {
        case fanDevice:
            analogWrite(FAN, percent * 2.5, 500);
            tft.println("Fan to " + String(percent) + "%");
            tempLockTime = millis() + (1000 * 60 * TEMPLOCKTIME);
            updateThermostatDisplay("Paused");
            break;

        case lcdBacklightDevice:
            analogWrite(BL, percent * 2.5, 500);
            tft.println("LCD BL to " + String(percent) + "%");
            break;

        case ledLightDevice:
            LEDbrightness = percent;
            breathIndex = 0; // should test light state before changing
            FastLED.setBrightness( (int) (LEDbrightness*255/100));
            FastLED.show();
            break;

        case ledModeDevice:
            Serial.printf("Set LED Mode as percentage %d", percent);
            Serial.println("");
            setLEDMode( percent);
            break;

        case ledDirectionDevice:
		//0-23 points around a circle: 0 north, 6 east, 12 south, 18 west
            if (percent < 24) // wrap around not supported by Alexa
            {
                LEDPoint = percent;
            } else {
                LEDPoint = 0;
            }
            break;

        case ledSpeedDevice: //LED speed
            //LEDSpeed = percent;
            break;

    }

    return 0;
}


int functionColor(int device, int R, int G, int B, String rawParameters)
{
    ledColor.r = R;
    ledColor.g = G;
    ledColor.b = B;
    fillLedColor( ledColor);
    FastLED.show();

    return device * 1000000;
}



int functionTemp(int device, bool requestCurrentTemp, bool requestSetTemp, int temperature, int changeAmount, String rawParameters)
{
    if (requestCurrentTemp) return currentTemp;
    if (requestSetTemp) return setTemp;
    setTemp = temperature;
    thermostat = true;
    updateThermostatDisplay("On");
    return 0;
}


int functionChannel(int device, int channel, int changeAmount, String unknown, String rawParameters)
{
    switch (device)
    {
        case ledLightDevice:
            LEDMode = (LEDModes) channel;
            break;

        default:
            break;
    }
    return 0;
}


int functionVolumePercent(int device, bool requestCurrentVolume, int volume, int changeAmount, String rawParameters)
{
    switch (device)
    {
        case ledLightDevice:
            if (requestCurrentVolume) return LEDDirection;
            LEDDirection = volume;
            break;

        default:
            break;
    }
    return 0;
}


void updateThermostatDisplay(String state)
{
    tft.setCursor(0,80);
    tft.println("Thermostat " + String(setTemp) + " F");
    tft.fillRect(0,80,128,120,ST7735_BLACK);
    tft.setCursor(0,100);
    tft.println(state);
}


void setup()
{
    pinMode(FAN, OUTPUT);
    analogWrite(FAN, 0, 500);

    pinMode(BL, OUTPUT);
    analogWrite(BL, 250, 500);

    Serial.begin(9600);
    Serial.println("Serial monitor started");

    /* Add echo devices for echo bridge
       These devices and capabilities are transferred to Alexa with the discover command
       The capabilities may be on or more of the following:
            functionOnOff           on, off
            functionPercent         0..100, up 0..100, down 0..100
            functionTemp            0..100+
            functionLightTemp       0..30000
            functionVolumePercent   0..100
            functionVolumeStep      0..100
            functionChannel         0..100?
            functionMediaControl    0..??
            functionInputControl    0..??
    */
    String deviceName = "Cool Breeze";
    String deviceType = "Fan";
    epb.addEchoDeviceV2OnOff(deviceName + " " + deviceType, &functionOnOff);
    epb.addEchoDeviceV2Percent(deviceName + " " + deviceType, &functionPercent);
    epb.addEchoDeviceV2Temp(deviceName + " " + deviceType, &functionTemp);

    deviceType = "BackLight";
    epb.addEchoDeviceV2OnOff(deviceName + " " + deviceType, &functionOnOff);
    epb.addEchoDeviceV2Color(deviceName + " " + deviceType, &functionColor);
    epb.addEchoDeviceV2Percent(deviceName + " " + deviceType, &functionPercent);

    deviceType = "Thermostat";
    epb.addEchoDeviceV2OnOff(deviceName + " " + deviceType, &functionOnOff);

    deviceType = "Light";
    epb.addEchoDeviceV2OnOff(deviceName + " " + deviceType, &functionOnOff);
    epb.addEchoDeviceV2Color(deviceName + " " + deviceType, &functionColor);
    epb.addEchoDeviceV2Percent(deviceName + " " + deviceType, &functionPercent); //brightness 0..100
    //epb.addEchoDeviceV3VolumePercent( deviceName + " " + deviceType, &functionVolumePercent);  // 0..100 speed, 0 max rev, 50 stop, 100 max fwd
    epb.addEchoDeviceV2Temp( deviceName + " " + deviceType, &functionTemp);    // direction  0 north, 90 east, 180 south, 270, west
    //epb.addEchoDeviceV2LightTemp( deviceName + " " + deviceType, &functionLightTemp);    // scene number 0..100+

/*
void EchoPhotonBridge::addEchoDeviceV3(String deviceName,
                    functionOnOff fOnOff,
                    functionVolumePercent fVolumePercent,
                    functionVolumeStep fVolumeStep,
                    functionChannel fChannel,
                    String commaSeperatedChannelList,
                    functionMediaControl fMediaControl,
                    functionInputControl fInputControl,
                    String commaSeperatedInputList)
    deviceType = "Light";
    epb.addEchoDeviceV3OnOff( deviceName + " " + deviceType, charLedModes, &functionChannel);
    epb.addEchoDeviceV3VolumePercent(deviceName + " " + deviceType, &functionOnOff); // brightness 0..100
    epb.addEchoDeviceV3VolumeStep(deviceName + " " + deviceType, &functionOnOff); // speed 0..100
    epb.addEchoDeviceV3Channel( deviceName + " " + deviceType, charLedModes, &functionChannel); // scene number 0..100+
    epb.addEchoDeviceV3MediaControl( deviceName + " " + deviceType, charLedModes, &functionChannel); // direction 0..360
*/

    //deviceType = "Scene";
    //epb.addEchoDeviceV2Percent( deviceName + " " + deviceType, &functionPercent);  // scene number 0..100 channel may be better
    //epb.addEchoDeviceV2Channel( deviceName + " " + deviceType, &functionChannel); // scene number 0..100+
    //epb.addEchoDeviceV2Percent( deviceName + " " + deviceType, &functionPercent); // 0..100 speed, 0 max rev, 50 stop, 100 max fwd
    //epb.addEchoDeviceV2Volume(  deviceName + " " + deviceType, &functionVolume);  // direction  0 north, 90 east, 180 south, 270, west

    //deviceType = "Pointer";
    //epb.addEchoDeviceV2Percent(deviceName + " " + deviceType, &functionPercent);

    deviceType = "Speed";
    epb.addEchoDeviceV2Percent(deviceName + " " + deviceType, &functionPercent);

    tft.initG();
    tft.fillScreen(ST7735_BLACK);
    //tft.setFont(TIMESNEWROMAN8);
    tft.setTextSize(1);
    tft.setTextColor(ST7735_WHITE);
    tft.setCursor(10,120);
    tft.setTextColor(ST7735_WHITE);
    tft.setTextWrap(true);
    tft.println(deviceName);

    functionOnOff(2,true,""); //Turn on Thermostat.
    oldTime = millis();
    lastBreathTime = millis();
    LEDbrightness = 1;
    FastLED.setBrightness( (int) (LEDbrightness*255/100));
    breathIndex = 0;

    ledSetup();
}

void loop()
{
    static unsigned long waitClock;

    if(millis() - oldTime >= 1000)
    {
        if (sensor.read())
        {
            tft.setTextColor(ST7735_BLACK);
            tft.setCursor(20, 20);
            tft.setTextSize(4);
            tft.print(lastUpdate);
            tft.setTextColor(ST7735_WHITE);
            tft.setCursor(20,20);
            lastUpdate = String::format("%1.1f",sensor.fahrenheit());
            tft.print(lastUpdate);
            //Particle.publish("sumologic", lastUpdate);

            currentTemp = sensor.fahrenheit();
            if (tempLockTime < millis() && thermostat)
            {
                if (currentTemp > setTemp + 1)
                {
                    functionOnOff(0,true,"");
                }
                else if (currentTemp < setTemp - 1)
                {
                    functionOnOff(0,false,"");
                }
            }
        }
        oldTime = millis(); //update old_time to current millis()
    }

    ledLoop(); // handle LED group effects
    ledFSM(); // handle individual LED effects
}
