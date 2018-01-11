# Ring Controller

## What This Is
The ring controller is a controller for an LED ring, specifically a Adafruit particle LED ring.
The controller uses a Particle Photon, but probably could be used by Arduino processors. The lights use a NeoPixel ring, but may be any array or RGB LEDs that can be controlled by the FastLED library.

At the moment control is performed on two levels: at the individual LED level and at a group level.

Individual LED control allows for a light to be flashed or dimmed independently of all other LEDs. This can be handy in some scenarios with complicated timing requirements, like meteor or drip effects.

Group control allows for a group of LEDs to be controlled as a unit. This is mose useful when all of the LEDs of a group are to be controlled in a similar fashion.

The basic control mechanism is a combined interpreter and finite state machine. Some commands are executed immediately and some commands are executed over time. This approach allows for a declarative approach for defining light patterns.

At the moment, there are two separate mechanisms to control individual or groups of LEDs.

Eventually there will be a third mechanism that controls the other two. It can be used to apply a particular effect for a period of time and then apply another effect.

Presently a crude web interface is provided to select colors, brightness or one of the predefined effects.

This is a work in progress. It is a prototype for experimenting with effects and chaining them together. This should not be taken as a parts of a final program. Things may be added or deleted from this prototype over time.
## To Use This
This assumes that a Particle Photon is installed on an Adafruit particle LED ring board #2268 and that the Photon is connected to the Particle.io server.

You need to supply your own creditials in the html/Config.js file:

```
// this file contains the access parameters for the Parical.io
var accessToken = "Your Token Here";
var deviceId = "Your Device Id Here";
```

Bring up the html/RingControl.html file in your browser. Selecting the various controls sends a command to the Photon.

## What Isn't Working

(The speed and direction controls are not fully implemented. 
No transitions have been implemented, nor have individual light controllers.
The HTML page has its own state that does not reflect the current state of the
LED ring as it should.
The code has not been fully tested, so it contains many bugs.)

Amazon Alexa controls and HTML controls are not integrated.

No .css code has been written, so the HTML control page is very plain and
uninteresting. The HTML goes to another page. You must return to the original
page to access another control.

Timing control has a problem. On the Photon things that should be staggered will tend to become synchronized (e.g., phased flashing. This is because of firmware overhead. To account for this timers should use their OLD values instead of millis() or now. This is a fairly pervasive change.

LED commands should have a LED_WAIT instead of LED_xx_TIMED. Just makes it a bit simpler (although less compact).

# BEYOND HERE ARE NOTES!!

### Control Communications

Amazon Alexa devices send voice commands to the Alexa sever via a local WiFi
connection to the internet. This server interprets the spoken commands to
appropriate bridge command which is relayed to the Particle.io server. In turn
Particle sends the command to the appropriate particle device via the internet
and another local WiFi connection.

An alternative way to control the LED ring is to use an HTML page which sends
commands to a Particle.io RESTful server which in turn forwards the commands to
the Particle Photon device. This allows some controls that are currently not
possible with Alexa itself.

The controller also controls a fan, a TFT display, and a thermostat with voice commands.

The Fast LED light control has the ability to control a set of FastLEDs. While
developed for an AdaFruit NeoPixel Ring,
it can be used with any array of LEDs supported by FastLED.

It is starting to look like the way to control this is by using MQTT and a program called Home Automation. The former provides an easy two-way communications. The latter provides a nice set of control widgets to make it easy to control the light.

### Light Control The light has a general on-off control as well as a brightness control. It also has a scene control for controlling light special effects.

In general, the light scene control has three layers: a mode that selects and
controls an overall scene, a transition for moving between scenes,
and a set of effects to control individual LED behaviors. Each of these three
layers is implemented as an independent finite state machine. Selecting a mode
also selects a transition from the current mode and the desired effect. A given
mode can have any number of effects running simultaneously and independently.

[the director should interpret a list of commands (scene, duration, transition.
Likewise individual LEDs should interpret a list of commands (on, off,
brightness, color, fade to black, fade to color, etc.) This interpreter can
determine whether the individual overrides the whole or is merged into it
somehow.)

For example:
-  mode: starry night -- black background with random stars

--  transition: slowly bring up stars on a black background

--  effect1: occasionally twinkle each star

--  effect2: at a low frequency start a brighter shooting star (meteor) effect3
--  effect3: move meteor with fading tail through the array, when done wait for effect2

- mode: quadFlasher -- black background divided into four quadrants. Initially odd quadrants are bright red.
-- transition: none
-- effect1: odd quadrants active, after time-out go to effect2
-- effect2: even quadrants active, after time-out go to effect1


- mode: on -- turn on a set of LEDs with a predefined set of colors
-- transition: cross fade to the defined colors
-- effect1: steady, no change


- mode: off -- turn off all LEDs
-- transition: fade LEDs to black
-- effect1: steady, no change

### History

This code was originally written by Jeremy Proffitt including the basic light control and Alexa bridge interface code.
Kirk Carlson added most of the Light controls.



## Welcome to your project!

Every new Particle project is composed of 3 important elements that you'll see have been created in your project directory for MyProject.

#### ```/src``` folder:  
This is the source folder that contains the firmware files for your project. It should *not* be renamed.
Anything that is in this folder when you compile your project will be sent to our compile service and compiled into a firmware binary for the Particle device that you have targeted.

If your application contains multiple files, they should all be included in the `src` folder. If your firmware depends on Particle libraries, those dependencies are specified in the `project.properties` file referenced below.

#### ```.ino``` file:
This file is the firmware that will run as the primary application on your Particle device. It contains a `setup()` and `loop()` function, and can be written in Wiring or C/C++. For more information about using the Particle firmware API to create firmware for your Particle device, refer to the [Firmware Reference](https://docs.particle.io/reference/firmware/) section of the Particle documentation.

#### ```project.properties``` file:  
This is the file that specifies the name and version number of the libraries that your project depends on. Dependencies are added automatically to your `project.properties` file when you add a library to a project using the `particle library add` command in the CLI or add a library in the Desktop IDE.

#### ```html``` folder
This folder contains the source files for the HTML control page.

#### ```RingControl.html``` file
This is the main HTML file. This is a plain HTML file and does not require any
server side include mechanisms other than that provided by standard HTML.

#### ```RingControl.js``` file
This handles button and control element clicks and sends a POST to the Particle.io
server.

#### ```Config.js``` file
This contains the access variable for accessing the Particle.io server. Replace the
"your..." fields with the appropriate identifiers

```
// this file contains the access parameters for the Parical.io addEchoDeviceV3VolumePercent
var accessToken = "your-access-token-here";
var deviceId = "your-device-id-here";
```

#### ```RingControl.css``` file
This formats the elements on the HTML page.


## Adding additional files to your project

#### Projects with multiple sources
If you would like add additional files to your application, they should be added to the `/src` folder. All files in the `/src` folder will be sent to the Particle Cloud to produce a compiled binary.

#### Projects with external libraries
If your project includes a library that has not been registered in the Particle libraries system, you should create a new folder named `/lib/<libraryname>/src` under `/<project dir>` and add the `.h` and `.cpp` files for your library there. All contents of the `/lib` folder and subfolders will also be sent to the Cloud for compilation.

## Compiling your project

When you're ready to compile your project, make sure you have the correct Particle device target selected and run `particle compile <platform>` in the CLI or click the Compile button in the Desktop IDE. The following files in your project folder will be sent to the compile service:

- Everything in the `/src` folder, including your `.ino` application file
- The `project.properties` file for your project
- Any libraries stored under `lib/<libraryname>/src`
