// C++ code
// David Martinez

// Import Libraries
#include <ezButton.h> // DOCS: https://arduinogetstarted.com/tutorials/arduino-button-library

// Pin Constants
const int dataPin = 2;
const int latchPin = 3;
const int alarmButtonPin = 6;
const int setTimeButtonPin = 7;
const int clockPin = 9;
const int piezoPin = 8;
const int AMPin = 11;
const int PMPin = 12;
const byte digitPin[4] = {A0,A1,A2,A3};

// Other Constants
const long noonMillis = 43200000; // 12:00
const long midnightMillis = 86400000; // 24:00
const long setTimeStep = 120000; // 1 Minute
const long maxButtonHold = 1000;
const int maxFlickerLoop = 1000;

// ezButton
ezButton alarmButton(alarmButtonPin);
ezButton setTimeButton(setTimeButtonPin);

// Keep variables for time control
long previousMillis = 0;
long currentMillis = 0; // Maximum should be 86,400,000 for 24 hours
int minutes = 0;
int hours = 0;
int tempMinutes = minutes;
int tempHours = hours;
int allDigits = 0;
bool AM = true;
bool PM = false;
long timeSetMillis = 0;
bool waitingForCycle = false;
bool cycleToggled = false;

// Alarm vars
long alarmMillis = 0;
long wantedAlarmMillis = 0; // 12:01 AM
bool alarmPlaying = false;
bool waitingForAlarmCycle = false;
bool settingAlarm = false;
bool isAlarmSet = false;
int alarmDigits = 0;
bool countingLengthPressed = false;
long lengthPressed = 0; // Amount of time the set alarm button has been pressed in its interval
int flickerLoop = 0;

// Byte patterns
const byte patterns[] = {
  B11111100, // 0
  B01100000, // 1
  B11011010, // 2
  B11110010, // 3
  B01100110, // 4
  B10110110, // 5
  B10111110, // 6
  B11100000, // 7
  B11111110, // 8
  B11110110  // 9
};


int digitUpdating = 0; // The digit we're currently updating


void setup()
{
  // Debugging Purposes
  Serial.begin(9600);
 
  // Pin setup
  pinMode(dataPin, OUTPUT);
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(piezoPin, OUTPUT);
  pinMode(PMPin, OUTPUT);
  pinMode(AMPin, OUTPUT);
 
  for(int j=0;j<4;j++){
    pinMode(digitPin[j], OUTPUT);    
    digitalWrite(digitPin[j], HIGH);
  }
 
  // Set button debounce time
  alarmButton.setDebounceTime(100);
  setTimeButton.setDebounceTime(100);
 
  // mark previous millis at
  // current millis to start counting time
  previousMillis = timeSetMillis + millis();

  // Update Time variables to match inputted time
  updateTime(previousMillis);
 
  // AM / PM
  // This needs to be above the alarmMillis statement
  // because it could override waitingForAlarmCycle
  setPM();
  if (previousMillis > noonMillis) {
    toggleCycle(true);
  }
 
  // Set alarm time
  // setAlarm(1201, false);
}

void loop()
{
  // button.loop() must be called in order to use the isPressed() method later
  // I really hope this doesn't yield the code
  // I put it above the currentMillis statement to avoid its inaccuracy in the case of yield
  alarmButton.loop();
  setTimeButton.loop();

  // Button Control
  // Update the time that the button has been pressed
  if (countingLengthPressed) {
    if (lengthPressed > maxButtonHold) {
      countingLengthPressed = false;
      if (settingAlarm) {
        settingAlarm = false;
      } else {
        settingAlarm = true;
        alarmDigits = 1200;
        setAM();
      }
    } else {
      lengthPressed++;
    }
  }
  // Button Checks
  if (setTimeButton.isPressed()) { // Toggle setting time
      timeSetMillis += setTimeStep;
      previousMillis = timeSetMillis + millis();
      updateTime(previousMillis);
  } else if (alarmButton.isPressed()) { // Make set alarm state
    countingLengthPressed = true; // Start counting length
  } else if (alarmButton.isReleased()){
    long currentLengthPressed = lengthPressed; // Save lengthPressed
    countingLengthPressed = false;
    lengthPressed = 0;
   
    if (settingAlarm) {
      alarmDigits++;
      if ((alarmDigits%100)==60) {
        if ((alarmDigits/100)==12) {
          alarmDigits = 100;
          toggleCycle(false);
        } else {
          alarmDigits = (((alarmDigits/100)+1)*100);
        }
      }
    } else if (currentLengthPressed > maxButtonHold) {
      // Set the cycle back to real time and save the alarm cycle
      long millisToCompare = (hours * 3600000) + (minutes * 60000);
      bool savedCycleIsAM = (AM == true);
      if (millisToCompare > noonMillis) {
        setPM();
      } else {
        setAM();
      }
      // Second Parameter checks if the alarm cycle is the same as the current cycle
      // If it is not, it will wait for a new cycle to sound the alarm
      setAlarm(alarmDigits, (!(AM==savedCycleIsAM)));
    } else if (alarmPlaying) {
      alarmPlaying = false;
      noTone(piezoPin);
    }
  }

  // Time Control
  // int currentSeconds = seconds (Might need this)
  currentMillis = timeSetMillis + millis(); // Save time
 
  // Check if one minute has passed
  if (currentMillis - previousMillis >= 60000) {
    previousMillis = currentMillis;
    minutes += 1;
    tempMinutes +=1;
    // restart minutes and increase hour
    if (minutes%60 == 0) {
      tempMinutes = 0;
      hours += 1;
      tempHours +=1;
      if (tempHours > 12) {
        tempHours = tempHours - 12; // This should usually be 1
      }
    }
  } else {
    //digitalWrite(digitPin[0], HIGH);
  }
 
  // 12 hour time
  if (hours%12 == 0) {
    if (!(cycleToggled)) { // Toggle the cycle if it wasn't already toggled
      toggleCycle(true);
      cycleToggled = true;
    }
    if (!(tempHours == 12)) { tempHours = 12; } // Make tempHours 12 if it isn't already.
  } else {
    cycleToggled = false; // Reset the debounce
  }

  if (minutes%60 == 0) {
    if (!(tempMinutes == 0)) { tempMinutes = 0; } // Make tempMinutes 0 if it isn't already
  }
 
  // Update allDigits
  allDigits = (tempHours * 100) + tempMinutes;
 
  if (settingAlarm) {
    // Override allDigits if the user is trying to set the alarm
    allDigits = alarmDigits;
  }

  if (settingAlarm || alarmPlaying) {
    // Flicker effect
    flickerLoop++;
    if (flickerLoop > maxFlickerLoop) {
      flickerLoop = 0;
    }
  } else {
    flickerLoop = 0;
  }

  // Update Digits
  digitUpdating++; // Switch digit being updated each loop
  if (digitUpdating > 4) { // Stay in bounds
    digitUpdating = 0;
  }
  updateDigit(digitUpdating);

  // Alarm
  // CONDITIONS:
  //  - There must be an alarm set
  //  - The user must not be trying to set the alarm
  //  - The 4 digits on the display must exactly match the 4 digits for the alarm time
  //  - The AM/PM cycle must be correct
  //  - The alarm should not already be playing
  if ((isAlarmSet) && (!(settingAlarm)) && (alarmDigits == allDigits) && (!(waitingForAlarmCycle)) && (!(alarmPlaying))) {
    alarmPlaying = true;
    tone(piezoPin, 523);
    isAlarmSet = false;
  }
}


void updateShiftRegister(int val) {
 
  // latch goes low first to signal to the
  // shift register that we are about to
  // send it some data
 
  digitalWrite(latchPin, LOW);
  shiftOut(dataPin, clockPin, LSBFIRST, val);
  digitalWrite(latchPin, HIGH);
}

void updateDigit(int digit) {
  if (flickerLoop < (maxFlickerLoop/2)) {
    // Calculate Number that needs to be displayed
    int digitNumber = (allDigits/int((pow(10, digit))))%10;
                       
    // Prepare to display number
    updateShiftRegister(patterns[digitNumber]);
   
    // Update Correct Digit
    for (int i = 0; i<4; i++) { // Turn all the digits off
      if (i == digit) { // Except for the wanted digit
        digitalWrite(digitPin[i], LOW);
      } else {
        digitalWrite(digitPin[i], HIGH);
        //analogWrite(digitPin[i], 127.9999965);
      }
    }
  } else {
    reset();
  }
}

void reset() {
  digitalWrite(latchPin, LOW);
  shiftOut(dataPin, clockPin, MSBFIRST, B00000000);
  digitalWrite(latchPin, HIGH);
}

void updateTime(long newMillis) { // Will need to call this when changing time
  minutes = newMillis/60000;
  tempMinutes = minutes%60;
  hours = newMillis/3600000;
  Serial.println(hours);
  if (hours%12 == 0) {
    tempHours = 12;
  } else {
    tempHours = hours%12;
  }
}

void toggleCycle(bool fixCycle) {
  if (fixCycle) {
    waitingForCycle = false; // Fix cycle
    waitingForAlarmCycle = false; // Fix alarm cycle
  }

  if (AM) {
    setPM();
  } else {
    setAM();
  }
}

void setAM() {
  AM = true;
  PM = false;
  digitalWrite(AMPin, HIGH);
  digitalWrite(PMPin, LOW);
}

void setPM() {
  AM = false;
  PM = true;
  digitalWrite(AMPin, LOW);
  digitalWrite(PMPin, HIGH);
}

void setAlarm(int digits, bool wait) {
  Serial.println(wait);
  alarmDigits = digits;
  waitingForAlarmCycle = wait; // Play in next cycle if its already passed
  isAlarmSet = true;
}
