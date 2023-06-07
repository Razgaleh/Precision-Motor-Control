/*
    - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    Title : Ablation Source-Precision Motor Control
    Author : fatemeh.abbasirazgaleh@colorado.edu
    Last Modified : Nov 28, 2016
    - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 
  SOME NOTES:

    [1] To Upload a Program to Precision Motor Control's Arduino:
    Our Arduino Uno is not supposed to restart everytime we initiate serial communication. Therefore to disable arduino auto-reset, I have soldered
    a 10 μF capacitor between Reset and GND pins. This makes it difficult to upload a new program on it. So to upload a new program you need to hold the
    reset button on arduino and click on upload while watching the two RX/TX lEDs on the PCB. As soon as these two LEDs start blinking, release the reset
    button.

    [2] To Talk to Ablation Source through Arduino Serial Monitor:
    Example:
    If you want the pico-motor to take 10 steps per 40 triggers,
    write 's10' and press enter then separately do the same for 't40'(Order doesn't matter).
    If you saw no change in the number, enter the value one more time.

    [3] To Troubleshoot and Check Values:
    I have taken out all serial.print functions, If in any case you needed to check the number steps and triggers through serial use the following code:
    char buffer[100];
    sprintf(buffer,"%ld,%ld",triggers,steps);
    Serial.println(buffer);

    [4] To find description and schematic of the driver's pins: 
    Analoge Pico-motor Driver Model 8703 : https://www.newport.com/medias/sys_master/images/images/hd5/h73/8797029892126/8712-8703-User-Manual-Rev-B.pdf

*/

#include<String.h>

String string_input;          // String input of triggers or steps.

const int main_control = 2;   // Trigger pin: Receives the triggers from i.e. function generator.
const int pulse = 3;          // Pulse pin: By setting it to low and later to high gives a pulse to motor.
const int LIDLE = 4;          // LIDLE pin: Low input disables pulse generation and reduces power consumption. Can also be used as system interlock. (From motor ctrl manual)
const int DIR = 5;            // Direction pin: Sets the direction of motor. HIGH : move foreward , LOW : move backwards.
const int switch1 = 11;       // First optical interrupter pin:  The right one if you look at the ablation source from the piezo valve side.
const int switch2 = 13;       // Second optical interrupter pin: The left one if you look at the ablation source from the piezo valve side.

long int triggers;            // Number of triggers
long int steps;               // Number of steps
long int newtriggers;         // Number of new steps: Local variable for get_command function.
long int newsteps;            // Number of new triggers: Local variable for get_command function.
long int oldtriggers;         // Number of old triggers: Used to compare variables
long int oldsteps;            // Number of old steps: Used to compare variables

int triggers_count = 0;       // Counter for triggers

bool backward;                // Boolean for backwards direction: Initially set to false.
bool foreward;                // Boolean for forewards direction: Initially set to false.
bool motor_triggers = true;   // Boolean for triggers: Sets to true whenever there's a trigger.


void setup() {

  Serial.begin(9600);

  pinMode (DIR, OUTPUT);
  pinMode (pulse, OUTPUT);
  pinMode (LIDLE, OUTPUT);

  digitalWrite(DIR, HIGH);    // Sets direction to foreward
  digitalWrite(LIDLE, HIGH);  // Enables motor


  foreward = true;            // Default direction: foreward

  
}
void loop() {

  while (triggers == 0 || steps == 0 ) {         // Program waits until there's a nonzero input value of triggers and steps
    getcommand(&steps, &triggers);               // Defined-Function: Check end of the code.
  }
   
   /* 
     Attach Interrupt is a built-in function that in our case looks for triggers while other
     parts of the code is getting compiled by looking for change in main_control pin's voltage.
     In addition, everytime it receives a triggers, it runs the trigger_ISR defined-function here.
     More info: http://gammon.com.au/interrupts
     
  */
   attachInterrupt(digitalPinToInterrupt(main_control), triggers_ISR, RISING);
   
  if (motor_triggers == false) {                 // It still changes the number of steps and triggers even if it's not receiving any triggers. If not it gets out of this scope and starts the loop again until the requested number is achieved.
    getcommand(&steps, &triggers);
  }

  else if (motor_triggers == true) {
    detachInterrupt(main_control);
    motor_triggers = false;                      // So that it looks for triggers again after going through this scope.
    if (triggers_count < triggers) {             // It sees if the requested number of triggers have been received from main_control pin
      getcommand(&steps, &triggers);
    }
    else {
        
      triggers_count = 0;                        // Reset the counter for triggers

      for (int i = 0; i < steps; i++) {          // It takes steps until there's a new value of triggers/steps

        oldsteps = steps;                        // Old steps and old triggers are used to compare values later in case there was new input value for triggers/steps
        oldtriggers = triggers;
        getcommand(&steps, &triggers);

        if (digitalRead(switch1) == 0 && digitalRead(switch2) == 0) { // When the Hf bar hasn't passed through any of optical interruptors

          digitalWrite(DIR, HIGH);
          digitalWrite(pulse, HIGH);                                  // By setting Pulse pin to high and low it gives the motor a pulse to take a step
          digitalWrite(pulse, LOW);

          if (digitalRead(switch1) != digitalRead(switch2)) {         // Setting direction when Hf bar has only passed optical interruptor 1
            foreward = true;
            backward = false;
          }
        }

        else if ((digitalRead(switch1) != digitalRead(switch2)) && foreward == true) {  // When Hf bar has only passed optical interruptor 1 and is moving foreward

          digitalWrite(DIR, HIGH);
          digitalWrite(pulse, HIGH);
          digitalWrite(pulse, LOW);

          if (digitalRead(switch1) == 1 && digitalRead(switch2) == 1)  {                // Setting direction when Hf bar has passed through both optical interruptors.
            foreward = false;
            backward = true;
          }
        }

        else if (digitalRead(switch1) == 1 && digitalRead(switch2) == 1) {              // When Hf bar has passed through both optical interruptors.

          digitalWrite(DIR, LOW);
          digitalWrite(pulse, HIGH);
          digitalWrite(pulse, LOW);

          if (digitalRead(switch1) != digitalRead(switch2)) {                           // Setting direction when Hf bar has only passed through optical interruptor 1.
            foreward = false;
            backward = true;

          }
        }

        else if  ((digitalRead(switch1) != digitalRead(switch2)) && backward == true ) { // When Hf bar has only passed through optical interruptor 1 and is moving backwards.

          digitalWrite(DIR, LOW);
          digitalWrite(pulse, HIGH);
          digitalWrite(pulse, LOW);

          if (digitalRead(switch1) == 0 && digitalRead(switch2) == 0) {                  // Setting direction when Hf bar hasn't passed through any of optical interruptors.
            backward = false;
            foreward = true;
          }
        }

        if ( triggers != oldtriggers || steps != oldsteps) {                             // Break out of for loop if there's a new value for triggers/steps
          break;
        }
      }
    }
  }
}

/*
  FUNCTION: triggers_ISR()

  DESCRIPTION:  Interrupt handler or ISR is a function that gets executed by  reception of a trigger.
                More info: https://en.wikipedia.org/wiki/Interrupt_handler
  INPUT:    None.

  OUTPUT:   PARAMETERS:
            GLOBAL: motor_triggers,trigger_count
            RETURN: None(void)

*/

void triggers_ISR(void) {

  motor_triggers = true;
  triggers_count++;
}

/*
  FUNCTION: getcommand()

  DESCRIPTION: receives the number of triggers/steps via serial and changes
             the value of triggers/steps as soon as its new value is given.

  INPUT:   PARAMETERS:
           LOCAL:  char_input, new_string_input,inputlen
           GLOBAL: string_input

  OUTPUT:  PARAMETERS:
            LOCAL: output_1,output_2
            GLOBAL: *newsteps, *newtriggers.
            RETURN: None (void).

  OTHER PARAMETERS: LOCAL: endstring, inputlen

  PROCESS:
         [1] If there's a serial communication, receive the input in char
         [2] Convert serial input to string array
         [3] Divides string array to starting letter (output_1) and all else (output_2)
         [4] It updates the value of steps and triggers 

*/
void getcommand (long int *newsteps, long int *newtriggers) {

  char   char_input;               // Serial input in char
  String new_string_input;         // Converted serial input in string
  String output_1;                 // Output String 1: First letter of input
  String output_2;                 // Output String 2: Every letter except the first letter of input and termination character(\n)
  int inputlen;                    // Length of serial input string
  bool endstring = false;          // Boolean for end of input: Sets to true when it receives the termination character(\n)


  while (Serial.available()) {

    char_input = Serial.read();
    string_input += char_input;    // It adds the serial input to string array char by char.

    if (char_input == '\n') {      // \n (newline) is termination character which we don't have to manually input: we can just press enter key

      new_string_input = string_input;
      string_input = "";           // Clear received buffer string and prepare for the next input
      Serial.end();                // Ends the serial communication to make sure buffer is cleared. We restart it soon later.
      endstring = true;
    }
  }

  if (endstring == true) {
    Serial.begin(9600);
  }

  inputlen = new_string_input.length();

  output_1 = new_string_input[0];
  for ( int i = 1; i < inputlen; i++ ) {
    output_2 += new_string_input[i];
  }

  /* Use this for troubleshooting
    if ( output_2.toInt() < 0 && inputlen > 0 ) {
    Serial.println("Error with input number");
    delay(10000);
    Serial.println("Reading the input string");
    Serial.println(Serial.readString());
    Serial.println("Done reading the input string");
    Serial.println("Please enter the input number again.");
    } */

  if (output_1 == "s") {           //  "s"=steps: since our notation for steps is : s10\n
    *newsteps = output_2.toInt();
  }

  else if (output_1 == "t") {      //  "t"=triggers: since our notation for triggers is : t10\n
    *newtriggers = output_2.toInt();
  }
}
