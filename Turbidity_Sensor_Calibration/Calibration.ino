#include <Arduino.h>
#include <Preferences.h>

const int TBD_PIN = 35;
Preferences storage;

const int numVar = 100;
float TBD_Voltage[numVar];
float voltage_sum = 0;
float avg_Vol;
float calibVal;
float highBuff = 0;

void setup()
{
    Serial.begin(115200);

    while (!Serial)
    {
    }

    storage.begin("phValues", false);
    //storage.begin("newCalib",false);

    for (int j = 0; j < 100; j++)
    {
        // 5 Volts Input. 0 - 4.5 V Output
        // TBD_Voltage[j] = analogRead(TBD_PIN)*(4.5/4095.)*(0.733/0.719);

        // 3.3 Volts Input
        TBD_Voltage[j] = analogRead(TBD_PIN)*(3.3/4095.);
    }

    /*for (int k = 2; k < 48; k++)
    {
        voltage_sum = voltage_sum + TBD_Voltage[k];
    }

    avg_Vol = voltage_sum / (numVar - 4);*/

    for (int n=0; n<100; n++)
    {
        if(TBD_Voltage[n] > highBuff)
        {
            highBuff = TBD_Voltage[n];
        }
    }

    //storage.putFloat("TBD_Value",highBuff);
    //calibVal = storage.getFloat("TBD_Value", 0.0);
    //storage.putFloat("TBD_Val",highBuff);
    //storage.remove("TBD_Value");
    calibVal = storage.getFloat("TBD_Value",0.0);
    storage.end();
}

void loop()
{

    /*Serial.print("The voltage is:");
    //Serial.println(avg_Vol);
    Serial.println(calibVal);
    Serial.println(highBuff);*/

    for(int n = 0; n<20; n++)
    {
       TBD_Voltage[n] = analogRead(TBD_PIN)*(3.3/4095.); 
       Serial.println(TBD_Voltage[n]);  
    }

}