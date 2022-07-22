#include <Preferences.h>
#include <OneWire.h>
#include <DallasTemperature.h>

const int BUS_PIN = D1;
uint8_t PH_PIN = 39;


Preferences storage;
OneWire SensorBus(BUS_PIN);
DallasTemperature Sensor(&SensorBus);

const int readingsNo = 10;
float tempBuff[readingsNo];
float phBuff[readingsNo];

// The values set by DFRobot
float TempC = 25.0;
float phValue = 7.0;
float acidVoltage = 2032.44;
float neutralVoltage = 1500.0;
float phVol = 1500.0;

//Calibration Values
const int receivedBuffLength = 10;
char receivedBuff[receivedBuffLength];
byte receivedBuffIndex;

void setup()
{
    Serial.begin(115200);
    Sensor.begin();
     
}

void loop()
{
    static unsigned long timepoint = millis();

    if (timepoint - millis() > 1000U)
    {
        timepoint = millis();

        Sensor.requestTemperatures();
        TempC = Sensor.getTempCByIndex(0);
        analogSetPinAttenuation(PH_PIN,ADC_11db);
        phVol = analogRead(PH_PIN)*(3.3/4095.)*1000.;
        phValue = readPH(phVol);
        Serial.print("Temp: ");
        Serial.print(TempC);
        Serial.print(" pH: ");
        Serial.print(phValue);
        Serial.print(" voltage: ");
        Serial.println(phVol);
    }
    calibration(phVol,TempC);
}

float readPH(float voltage)
{
    float slope = (7.0 - 4.0) / ((neutralVoltage - 1500.0) / 3.0 - (acidVoltage - 1500.0) / 3.0) / 3.0;
    float intercept = 7.0 - slope * (neutralVoltage - 1500.0) / 3.0;
    float phvalue = slope * (voltage - 1500.0) / 3.0 + intercept;
    return phvalue;
}

void calibration(float voltage, float temperature)
{
    TempC = temperature;
    phVol = voltage;

    if (SerialDataAvailable() > 0)
    {
        phCalibration(cmdparse());
    }
}

boolean SerialDataAvailable()
{
    char receivedChar;
    static unsigned long receivedTimeOut = millis();
    while(Serial.available()>0){
        if(millis() - receivedTimeOut > 500U){
            receivedBuffIndex = 0;
            memset(receivedBuff,0,(receivedBuffLength));
        }
        receivedTimeOut = millis();
        receivedChar = Serial.read();
        if (receivedChar == '\n' || receivedBuffIndex==receivedBuffLength-1){
            receivedBuffIndex = 0;
            strupr(receivedBuff);
            return true;
        }else{
            receivedBuff[receivedBuffIndex] = receivedChar;
            receivedBuffIndex++;
        }
    }
    return false;
    
}

byte cmdparse()
{
    byte modeValue = 0;
    if(strstr(receivedBuff,"ENTERPH") != NULL)
    {
        modeValue = 1;
    }
    else if(strstr(receivedBuff,"EXITPH") != NULL)
    {
        modeValue = 2;
    }
    else if (strstr(receivedBuff,"CALPH") != NULL)
    {
        modeValue = 3;
    }
    return modeValue;
}

void phCalibration(byte mode)
{
    static bool calibrationFlag = 0;
    static bool calibrationFinish = 0;

    switch (mode)
    {
        case 0: 
        if(calibrationFlag)
        {
            Serial.print("Error! Enter Correct Value!");
        }
        break;

        case 1:
        calibrationFlag = 1;
        calibrationFinish = 0;
        if(calibrationFlag)
        {
            Serial.println("This is the PH Calibration Mode");
            Serial.println("Place the probe inside one of the standard solutions");
        }
        break;

        case 2:
        if(calibrationFlag)
        {
            if((phVol > 1322)&&(phVol < 1678))
            {
                Serial.println("The Buffer Solution is 7.0");
                Serial.println("Enter ENTERPH");
                neutralVoltage = phVol;
                calibrationFinish = 1;
            }
            else if((phVol > 1854)&&(phVol < 2210))
            {
                Serial.println("The Buffer Solution is 4.0");
                Serial.println("Enter ENTEROH");
                acidVoltage = phVol;
                calibrationFinish = 1;
            }
            else
            {
                Serial.println("Error, Try Again!");
                calibrationFinish = 0;
            }

        
        }
        break;

        case 3:
        if(calibrationFlag)
        {
            Serial.println();
            storage.begin("phValues", false);

            if(calibrationFinish)
            {
                if((phVol > 1322)&&(phVol < 1678))
                {
                    storage.putFloat("neutralVoltage",neutralVoltage); 
                }
                else if((phVol > 1854)&&(phVol < 2210))
                {
                    storage.putFloat("acidVoltage",acidVoltage);
                }
                Serial.println("Calibration is Successful!");
            }
            else
            {
                Serial.println("Calibration has failed!");
            }
            Serial.println("Exit Calibration Mode!");
            calibrationFlag = 0;
            calibrationFinish = 0;
        }
        break;
    }

}