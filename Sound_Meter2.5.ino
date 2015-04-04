#include <LiquidCrystal.h>
#include <EEPROM.h>
#include <SD.h>

boolean debug=false;


LiquidCrystal lcd(9, 8, 7, 6, 5, 4);
const byte chipSelect = 10;       //for SD card

const byte buttonPin = 3;         // Analog pin - the number of the pushbutton pin
const byte signalPin = 4;        // Analog pin - the number of the db signal input
//const byte dbPowerPin = 2;       // Analog pin - the number of the pin that sends 5v to db Meter   NOTE: value not used, but pin 7 is used
const byte stayAlivePin = 3;     // Digital pin - the number of the pin to signal fast/slow switch on db meter
int buttonState = 0;             // variable for reading the pushbutton status
int stayAliveCounter = 0;

boolean signalDropped = false;
boolean programStarted = false;    //set to true will start program by default
boolean songStarted = false;
boolean badSD = true;           //Error flag = SD1
boolean cannotOpenFile = false; //Error flag = SD2

byte displayValues[6];         //Song-min, Song-Avg, Song-max, Service-min, Service-Avg, Service-max
int timerValuesSong[3];        //Minutes, seconds, millisecond counter
int timerValuesService[3];     //Minutes, seconds, millisecond counter

const byte index1secLen = 10;
const byte index5secLen = 5;
//const byte indexSongLen = 120;  //10 minutes (1 sample every 5 seconds, 12 samples per minute)

int array5sec[index5secLen];
//int arraySong[indexSongLen];
int summaryDetailsForSong[2];    //count, total
int summaryDetailsForService[2]; //count, total
byte summaryMin[21];
byte summaryMax[21];
byte summaryAvg[21];
byte summaryMinute[21];
byte summarySecond[21];


byte index1sec;
byte index5sec;
byte indexSong;

float total1sec;
float total5sec;
float totalSong;

byte counter5sec;
byte counterSong;

int average;
float voltage = 0;
byte myFileType = 0;
byte mySongCount;
int myServiceCount = 0;

String songFile;
String serviceFile;
File myFile;


String myPrintString[3];

byte myDelay = 73;    //This value should get the clock as close to 100ms as possible, lower values speed up the clock


int freeRam() {
  extern int __heap_start,*__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int) __brkval);  
}

void setup()
{
  lcd.begin(16, 2);

  pinMode(10, OUTPUT);
  pinMode(stayAlivePin, OUTPUT);

  //Make Analog Pin A2 output high to send 5v to db meter
  //analogWrite(A2, 255);

  Serial.begin(9600);

  Serial.print(F("Starting SD..."));
  lcd.print(F("Starting SD..."));
  lcd.setCursor(0,1);

  while(badSD) {
    buttonState = analogRead(buttonPin);

    if (buttonState > 650) { 
      //exit SD check
      Serial.println(F("SD Check Cancelled"));

      lcd.clear();
      lcd.print(F("SD Check"));
      lcd.setCursor(0,1);
      lcd.print(F("Cancelled"));

      delay(1000);

      badSD=true;
      return;
    }

    if (SD.begin(chipSelect)) {
      Serial.println(F("SD ok"));
      badSD=false; 
    }
    else if (debug) {
      Serial.println(F("Running in debug.  Aborting SD check."));
      badSD=false;
    }
    else {
      lcd.setCursor(0,1);
      Serial.println(F("SD check failed"));
      lcd.print(F("SD check failed"));
      delay(250);
      lcd.setCursor(0,1);
      lcd.print(F("                "));
      badSD=true;
    }

  }

  resetValues();              //Show values on screen (all set to 0)

  lcd.clear();

  buttonPress(1);  //start by default


  //for (int i=0;i<20;i++) {
  // summaryMin[i]=10;
  // summaryMax[i]=100;
  // summaryAvg[i]=100;
  // summaryTime[i]=1000;
  //}

}

void loop(void)
{


  printAll();
  if (debug) {
    lcd.setCursor(2,0);
    lcd.print(F("D")); 
  }


  delay(myDelay);


  switch(stayAliveCounter) {
  case 0:
    digitalWrite(stayAlivePin, LOW);   //Set stay alive pin is set to low

    break;
  case 250:
    digitalWrite(stayAlivePin, HIGH);   //Set stay alive pin is set to high to keep it alive
    break;

  case 252:
    digitalWrite(stayAlivePin, LOW);   //Set stay alive pin is set to low
    break;

  case 254:
    digitalWrite(stayAlivePin, HIGH);   //Set stay alive pin is set to high to keep it alive
    break;

  case 256:
    stayAliveCounter=-1;      //reset counter
    break;
  }
  stayAliveCounter++;




  if (!debug) {
    buttonState = analogRead(buttonPin);
  }
  else if (debug && timerValuesSong[1]==15) {
    buttonState=700;
  }
  else if (debug) {
    buttonState = 0;
  }

  if (buttonState > 900) { 
    //right button
    buttonPress(1);
  }
  else if (buttonState > 650) {
    //left button
    buttonPress(2);
  }

  if (programStarted==false) {
    return; 
  }


  //update timers for service
  timerValuesService[2] += myDelay;
  if (timerValuesService[2] >= (myDelay*index1secLen)) {
    timerValuesService[2] = 0;
    timerValuesService[1]++;
  }
  if (timerValuesService[1] > 59) {
    timerValuesService[1] = 0;
    timerValuesService[0]++;
  }

  //Check if song has started
  if (songStarted == true) {
    //update timers for song and proceed
    timerValuesSong[2] += myDelay;
    if (timerValuesSong[2] >= (myDelay*index1secLen)) {
      timerValuesSong[2] = 0;
      timerValuesSong[1]++;
    }
    if (timerValuesSong[1] > 59) {
      timerValuesSong[1] = 0;
      timerValuesSong[0]++;
    }
  }
  else {
    if(debug) {
      buttonPress(2);
    }   //Auto-start the next song
  }



  if (debug) {
    voltage+=0.06;
  }
  else {
    voltage = analogRead(signalPin);
    //printValue(4, (String)voltage);          //uncomment for calibration
    //voltage *= 0.4285608183;                 //USB from PC
    //voltage /= 2.848;                        //7v-12v using VIN
    voltage /= 2.223;                          //USB from wall-wart  5.1v 0.7A
    //printValue(4, (String)((int)(voltage + 0.5)));          //uncomment for calibration
  }




  //Check if signal dropped
  if (voltage < 10 && !debug) {
    signalDropped = true; 
    return;                    //don't do anything after this if the signal is dropped
  }
  else {
    signalDropped = false;
  }



  //Store value to array1sec
  total1sec += voltage;
  index1sec++;

  if (index1sec >= index1secLen) {
    //second has been populated, get average
    average = total1sec / index1secLen;    

    //reset 1sec
    index1sec = 0;
    total1sec = 0;

    displayValues[3] = average;                     //add 1sec value to displayValues array

    //write 1sec to array5sec
    total5sec -= array5sec[index5sec];              //remove this index value from total (this is needed for overwriting current value in array)
    array5sec[index5sec] = average;

    total5sec += array5sec[index5sec];

    if (songStarted == true) {
      //Print 1 second value to song file
      myPrintString[0] = (String)numToTwo(timerValuesService[0]) + ":" + (String)numToTwo(timerValuesService[1]);
      myPrintString[1] = (String)numToTwo(timerValuesSong[0]) + ":" + (String)numToTwo(timerValuesSong[1]);
      myPrintString[2] = (String)array5sec[index5sec];

      printToFile(songFile, myPrintString[0] + "," + myPrintString[1] + ","+myPrintString[2]); 



      //Check if myMax or myMin need to be updated for Song, based off array1sec value
      if (displayValues[3] > displayValues[2]) {
        displayValues[2] = displayValues[3];
      }
      if (displayValues[3] < displayValues[0]) {
        displayValues[0] = displayValues[3];
      }
      //Update min and max 
      summaryMin[mySongCount]=displayValues[0];
      summaryMax[mySongCount]=displayValues[2];

      //Check if myMax or myMin need to be updated for Service, based off array1sec value
      if (displayValues[3]>summaryMax[0]) {
        summaryMax[0] = displayValues[3];
      }
      if (displayValues[3] < summaryMin[0]) {
        summaryMin[0] = displayValues[3];
      }


      //add to song summary array
      summaryDetailsForSong[0]++;                     //update counter
      summaryDetailsForSong[1] += displayValues[4];            //add to total
      //add to total summary array
      summaryDetailsForService[0]++;                  //update counter
      summaryDetailsForService[1] += displayValues[4];         //add to total


      //Calculate averages for Service
      summaryAvg[0]=summaryDetailsForService[1] / summaryDetailsForService[0];
      summaryMinute[0]=timerValuesService[0];
      summarySecond[0]=timerValuesService[1];

      //Calculate averages for Song
      summaryAvg[mySongCount]=summaryDetailsForSong[1] / summaryDetailsForSong[0];
      summaryMinute[mySongCount]=timerValuesSong[0];
      summarySecond[mySongCount]=timerValuesSong[1];



      printToFile(serviceFile, "");

    }

    //calculate array5sec average
    displayValues[4] = total5sec / counter5sec;
    if (counter5sec < index5secLen) { 
      counter5sec++; 
    }

    //Increase count or start over
    index5sec++;

    //Check if array should start over and write to arraySong
    if (index5sec >= index5secLen) {



      index5sec = 0;

      //Start over if greater than the limit
      //      if (indexSong >= indexSongLen) {
      //        indexSong = 0;
      //      }

      //5second has been fully populated, use average that was calculated above
//      //add to song summary array
//      summaryDetailsForSong[0]++;                     //update counter
//      summaryDetailsForSong[1] += displayValues[4];            //add to total
//      //add to total summary array
//      summaryDetailsForService[0]++;                  //update counter
//      summaryDetailsForService[1] += displayValues[4];         //add to total
//
//
//      //Calculate averages for Service
//      summaryAvg[0]=summaryDetailsForService[1] / summaryDetailsForService[0];
//      summaryMinute[0]=timerValuesService[0];
//      summarySecond[0]=timerValuesService[1];
//
//      //Calculate averages for Song
//      summaryAvg[mySongCount]=summaryDetailsForSong[1] / summaryDetailsForSong[0];
//      summaryMinute[mySongCount]=timerValuesSong[0];
//      summarySecond[mySongCount]=timerValuesSong[1];

      displayValues[1]  = summaryAvg[mySongCount];

      //      totalSong -= arraySong[indexSong];              //remove this index value from total (this is needed for overwriting current value in array)
      //
      //      //arraySong[indexSong] = average;                  //This is the average calculated in array5sec if statement
      //      totalSong += arraySong[indexSong];
      //
      //      //calculate arraySong average
      //      if (songStarted == true) {
      //        displayValues[1] = totalSong / counterSong;
      //      }
      //      if (counterSong < indexSongLen) { 
      //        counterSong++; 
      //      }
      //      //Increase count
      //      indexSong++;

    }
  }
}

void printAll() {

  if (!programStarted) {
    lcd.clear();
    lcd.print(F("  Press button  "));
    lcd.setCursor(0,1);
    lcd.print(F("    to start    "));
    return;
  }


  //Check for errors
  if (signalDropped) { 
    printValue(4, "***");
  }
  else if (badSD) { 
    printValue(4, "SD1");
  }
  else if (cannotOpenFile) {
    printValue(4, "SD2");
  }
  else {
    printValue(4, "   ");       //Comment this during calibration
    printValue(4, (String)((int)(voltage + 0.5)));          //uncomment for calibration
  }

  //if min is over 100, just display 2 digits
  if (displayValues[0]>=100) {
    printValue(0," 0");
  }
  else { 
    printValue(0,numToTwo(displayValues[0])); 
  }

  printValue(1,numToThree(displayValues[1]));
  printValue(2,numToThree(displayValues[2]));
  printValue(3,numToTwo(timerValuesSong[0]) + ":" + numToTwo(timerValuesSong[1])); 
  printValue(5,numToThree(displayValues[3]));
  printValue(6,numToThree(displayValues[4]));
  printValue(7,numToTwo(timerValuesService[0]) + ":" + numToTwo(timerValuesService[1])); 
}

void printValue(int myPosition, String myValue) {

  switch (myPosition) {
  case 0:
    lcd.setCursor(0, 0);
    lcd.print(myValue);
    break;
  case 1:
    lcd.setCursor(3, 0);
    lcd.print(myValue);
    break;
  case 2:
    lcd.setCursor(7, 0);
    lcd.print(myValue);
    break;
  case 3:
    lcd.setCursor(11, 0);
    lcd.print(myValue);
    break;
  case 4:
    lcd.setCursor(0, 1);
    lcd.print(myValue);
    break;
  case 5:
    lcd.setCursor(3, 1);
    lcd.print(myValue);
    break;
  case 6:
    lcd.setCursor(7, 1);
    lcd.print(myValue);
    break;
  case 7:
    lcd.setCursor(11, 1);
    lcd.print(myValue);
    break;
  case 20:
    //Clear entire screen and display value
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(myValue);

    break;
  }



}

String numToThree(int myNumber) {
  String myString = "   ";

  if (myNumber < 10) { 
    myString = "  " + (String)myNumber + " ";
  }
  else if (myNumber < 100) {
    myString = " " + (String)myNumber + " ";
  }
  else {
    myString = "" + (String)myNumber + " ";
  }

  return myString; 
}

String minuteToThree(int myNumber) {
  String myString = "   ";

  if (myNumber < 10) { 
    myString = "  " + (String)myNumber;
  }
  else if (myNumber < 100) {
    myString = " " + (String)myNumber;
  }
  else {
    myString = "" + (String)myNumber;
  }

  return myString; 
}

String seqNumFormat(int myNumber) {
  String myString = "   ";

  if (myNumber < 10) { 
    myString = "00" + (String)myNumber;
  }
  else if (myNumber < 100) {
    myString = "0" + (String)myNumber;
  }
  else {
    myString = (String)myNumber;
  }

  return myString; 
}

String numToTwo(int myNumber) {
  String myString = "  ";
  if (myNumber>100) {
    myNumber -= 100;
  }

  if (myNumber < 10) { 
    myString = "0" + (String)myNumber;
  }
  else {
    myString = (String)myNumber;
  }

  return myString; 
}


void buttonPress(int myButton) {

  if (!programStarted) {
    myButton=1;
  }

  delay(100);
  lcd.clear();
  delay(100);

  switch (myButton) {
  case 1:            //Right button

    //initialize file
    if (!programStarted) {
      //start the program
      programStarted=true;

      //create a new Service file
      newServiceFile();

      //restart song values
      buttonPress(2);
    }
    else {
      //Check if song file is still running
      //if (songStarted==true) {
      //  buttonPress(2);
      //}

      //end the program
      programStarted=false;

      //reset values for song and service
      resetValues();
      printAll();
    }
    break;
  case 2:            //Left button

    //create a new Song file if the program is started
    if (myServiceCount>0 && programStarted==true) { 
      if (songStarted==false) {
        //start a new song
        newSongFile();
        songStarted=true;
      }
      else if (displayValues[0] != 200) {
        //stop the current song
        resetValues();
        songStarted=false;
        buttonPress(2);    //Auto-start next song
      }
      else {
        //buttonPress(2);  //Auto-start next song
        return;
      }
    }

    break;
  }
}

void newServiceFile() {
  //read eeprom to get last sequence number
  myServiceCount = EEPROM.read(0)+1;
  if (myServiceCount==512) {
    EEPROM.write(0, 1);
  }
  else {
    EEPROM.write(0, myServiceCount);
  }
  mySongCount = 0;
  serviceFile = seqNumFormat(myServiceCount) + "_SUM";



}

void newSongFile() {
  mySongCount++;
  Serial.println();
  myPrintString[0] = "";
  myPrintString[1] = "";
  myPrintString[2] = "";
  songFile = seqNumFormat(myServiceCount) + "_" + seqNumFormat(mySongCount);
  printToFile(songFile, "");

}

void resetValues() {

  if (!programStarted) {
    //set all values to 0
    displayValues[3]=0;
    displayValues[4]=0;
    mySongCount=0;

    //Reset values for service timer
    for (int i=0;i<2;i++) {
      timerValuesService[i]=0;
    }
    for (int i=0;i<2;i++) {
      summaryDetailsForService[i]=0;
    }

    //reset values for summary file
    for (int i=0; i<21; i++) {
      summaryMin[i]=200;
      summaryMax[i]=0;
      summaryAvg[i]=0;
      summaryMinute[i]=0;
      summarySecond[i]=0;
    }

  }
  //Reset values for song
  for (int i=0;i<2;i++) {
    timerValuesSong[i]=0;
  }
  for (int i=0;i<index5secLen;i++) {
    array5sec[i]=0;
  }
  //  for (int i=0;i<indexSongLen;i++) {
  //    arraySong[i]=0;
  //  }

  for (int i=0;i<2;i++) {
    summaryDetailsForSong[i]=0;
  }

  index1sec = 0;
  index5sec = 0;
  indexSong = 0;
  total1sec = 0;
  total5sec = 0;
  totalSong = 0;
  counter5sec = 1;
  counterSong = 1;
  average=0;
  displayValues[0]=200;
  displayValues[1]=0;
  displayValues[2]=0;
}




void printToFile(String fileName, String myString) {

  //check for badSD card
  if(badSD) {
    Serial.println(F("Bad SD.  Abort writing to file."));
    return;
  }



  fileName.trim(); 
  fileName.concat(".TXT");

  // prepare the character array
  char fileNameType[fileName.length()];
  fileName.toCharArray(fileNameType, fileName.length()+1);

  if (fileName==serviceFile+".TXT") {
    SD.remove(fileNameType);
  }

  myFile = SD.open(fileNameType, FILE_WRITE);

  if (myFile) {
    if (fileName!=serviceFile+".TXT" && myString=="") {
      if (!debug) {
        Serial.print(F("Free SRAM = "));
        Serial.print(freeRam());
      };

      Serial.print(F("    File: "));
      Serial.print(fileNameType);
      Serial.println(F("    Value: Service Time,Song Time,dB Value"));
    }
    else if (fileName!=serviceFile+".TXT"){
      if (!debug) {
        Serial.print(F("Free SRAM = "));
        Serial.print(freeRam());
      };

      Serial.print(F("    File: "));
      Serial.print(fileNameType);
      Serial.print(F("    Value: "));
      Serial.println(myString);
    }

    if (fileName!=serviceFile+".TXT") {

      if (myFile.size()==0) {
        myFile.println(F("Service Time,Song Time,dB Value"));
      }
      if (myString!="") {
        myFile.println(myString);
      }

      cannotOpenFile=false;
    }
    else {
      myFile.println(F("------------------------------------------------------------------"));
      myFile.print(F("---------------------------SERVICE #"));
      myFile.print(seqNumFormat(myServiceCount));
      myFile.println(F("---------------------------"));
      myFile.println(F("------------------------------------------------------------------"));


      for (byte i=1;i<mySongCount+1;i++) {
        myFile.print(F("Song #"));
        myFile.print(seqNumFormat(i));
        myFile.print(F(" -     Min:"));
        myFile.print(numToThree(summaryMin[i]));
        myFile.print(F("     Avg:"));
        myFile.print(numToThree(summaryAvg[i]));
        myFile.print(F("     Max:"));
        myFile.print(numToThree(summaryMax[i]));
        myFile.print(F("     Time: "));
        myFile.print(numToTwo(summaryMinute[i]));
        myFile.print(F(":"));
        myFile.println(numToTwo(summarySecond[i]));
      }
      myFile.println(F("=================================================================="));
      myFile.print(F("Summary   -     Min:"));
      myFile.print(numToThree(summaryMin[0]));
      myFile.print(F("     Avg:"));
      myFile.print(numToThree(summaryAvg[0]));
      myFile.print(F("     Max:"));
      myFile.print(numToThree(summaryMax[0]));
      myFile.print(F("     Time:"));
      myFile.print(minuteToThree(summaryMinute[0]));
      myFile.print(F(":"));
      myFile.println(numToTwo(summarySecond[0]));
    }


  } 
  else {
    Serial.print(F(" !error opening "));
    Serial.println(fileNameType);

    cannotOpenFile=true;
  }

  myFile.close();


}








































