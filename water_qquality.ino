#include <Arduino.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <WiFi.h>
#include <ThingSpeak.h>
#include <EEPROM.h>
#include <Preferences.h>

Preferences storage;

const int BUS_PIN = D1;
const int TDS_PIN = A0;
const int PH_PIN = 39;
const int TBD_PIN = 35;
OneWire Sensor_Bus(BUS_PIN);
DallasTemperature Sensor(&Sensor_Bus);

const int numVar = 100;

// Variables for Turbidity Values
float tbdVol[numVar];
float tbdvolSum = 0;
float tbdvolAvg;
float tbdCalibVal;
float tbdBuff;
float relativeVal;
float prevVal = 0;

// Variables for Turbidity Outliers
float medianBuff[numVar];
float modZscore[numVar];
float medianVal;
float medianDeviation;
int nonOutlierCount = 0;
int outlierCount = 0;
float tbdmeansum = 0;
float tbdavg;
float meanBuff[numVar];
float tbdmeanD;

// Variables for TDS Values
float volVal[numVar];
float tdsBuff;
float tdsvolSum = 0;
float tdsvolAvg;
float tdsValue;
float tempComp;
float volComp;
float tdsMedian;
float tdsMedBuff[numVar];
float tdsMAD;
float tdsZScore[numVar];
float tdsOutlier = 0;
float tdsNonOutlier = 0;
float tdsmeansum = 0;
float tdsavg;
float tdsMeanBuff[numVar];
float tdsMeanD;

// Variable for Temperature Value
float TempC;

// Variables for PH Values
float phValue;
float phValue4;
float phValue7;
float phVol;

const int WIFI_DELAY = 5000;

const char *ssid = "geviton_2";
const char *password = "1234ninye#";

WiFiClient client;

unsigned long channelNumber = 4;
const char *myWriteAPIKey = "UUA8Y6Y3P65ETFAL";

unsigned long Time = 0;
unsigned long publishTime = 300000;

void setup()
{
  Serial.begin(115200);
  Sensor.begin();
  WiFi.mode(WIFI_STA);
  ThingSpeak.begin(client);

  storage.begin("phValues", false);
  phValue7 = storage.getFloat("neutralVoltage", 0.0);
  phValue4 = storage.getFloat("acidVoltage", 0.0);
  storage.putFloat("TBD",2.15);
  tbdCalibVal = storage.getFloat("TBD", 0.0);
}

void loop()
{

  if (millis() - Time > publishTime)
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.print("Attempting to connect.");
      while (WiFi.status() != WL_CONNECTED)
      {
        WiFi.begin(ssid, password);
        delay(WIFI_DELAY);
      }
      Serial.println("\nConnected");
    }

    // Obtaining temperature values
    Sensor.requestTemperatures();
    TempC = Sensor.getTempCByIndex(0);
    Serial.print("The temperature is:");
    Serial.println(TempC);

    // Obtaining TDS values
    for (int a = 0; a < 100; a++)
    {
      volVal[a] = analogRead(TDS_PIN) * 3.3 / 4095.;
    }

    for (int b = 0; b < 100; b++)
    {
      tdsmeansum += volVal[b];
      for (int d = b + 1; d < 100; d++)
      {
        if (volVal[d] < volVal[b])
        {
          tdsBuff = volVal[d];
          volVal[d] = volVal[b];
          volVal[b] = tdsBuff;
        }
      }
    }

    tdsavg = tdsmeansum / numVar;
    tdsMedian = ((volVal[numVar / 2]) + volVal[(numVar / 2) + 1]) / 2;

    for (int g = 0; g < 100; g++)
    {
      tdsMedBuff[g] = abs(volVal[g] - tdsMedian);
      tdsMeanBuff[g] = abs(volVal[g] - tdsavg);
    }

    for (int j = 0; j < 100; j++)
    {
      for (int k = j + 1; k < 100; k++)
      {
        if (tdsMedBuff[k] < tdsMedBuff[j])
        {
          tdsBuff = tdsMedBuff[k];
          tdsMedBuff[k] = tdsMedBuff[j];
          tdsMedBuff[j] = tdsBuff;
        }
        if (tdsMeanBuff[k] < tdsMeanBuff[j])
        {
          tdsBuff = tdsMeanBuff[k];
          tdsMeanBuff[k] = tdsMeanBuff[j];
          tdsMeanBuff[j] = tdsBuff;
        }
      }
    }

    tdsMAD = ((tdsMedBuff[numVar / 2]) + (tdsMedBuff[(numVar / 2) + 1])) / 2;
    tdsMeanD = ((tdsMeanBuff[numVar / 2]) + (tdsMeanBuff[(numVar / 2) + 1])) / 2;
    for (int l = 0; l < 100; l++)
    {
      if (tdsMAD == 0)
      {
        tdsZScore[l] = (volVal[l] - tdsMedian) / (1.253314 * tdsMeanD);
      }
      else if (tdsMAD > 0)
      {
        tdsZScore[l] = (volVal[l] - tdsMedian) / (1.486 * tdsMAD);
      }
    }

    for (int f = 0; f < 100; f++)
    {
      if ((tdsZScore[f] > 1) || (tdsZScore[f] < -1))
      {
        volVal[f] = 0;
        tdsOutlier += 1;
      }
      else if ((tdsZScore[f] < 1) && (tdsZScore[f] > -1))
      {
        tdsNonOutlier += 1;
      }
    }

    for (int g = 0; g < 100; g++)
    {
      tdsvolSum += volVal[g];
    }

    tdsvolAvg = tdsvolSum / tdsNonOutlier;
    tempComp = 1 + 0.02 * (TempC - 25.);
    volComp = tdsvolAvg / tempComp;
    tdsValue = (133.42 * volComp * volComp * volComp - 255.86 * volComp * volComp + 857.39 * volComp) * 0.5;
    Serial.print("The TDS value is:");
    Serial.println(tdsValue);
    Serial.println(tdsmeansum);

    // Obtaining PH values
    analogSetPinAttenuation(PH_PIN, ADC_11db);
    phVol = analogRead(PH_PIN) * (3.3 / 4095.) * 1000.;
    float slope = (7.0 - 4.0) / ((phValue7 - 1500.0) / 3.0 - (phValue4 - 1500.0) / 3.0);
    float intercept = 7.0 - slope * (phValue7 - 1500.0) / 3.0;
    phValue = slope * (phVol - 1500.0) / 3.0 + intercept;
    Serial.print("The PH value is:");
    Serial.println(phValue);
    Serial.println(phVol);

    // Obtaining TBD Values

    for (int i = 0; i < 100; i++)
    {
      // 5V input voltage
      // tbdVol[i] = analogRead(TBD_PIN)*(4.5/4095.)*(0.733/0.719);
      tbdVol[i] = analogRead(TBD_PIN) * (3.3 / 4095.);
    }

    for (int j = 0; j < 100; j++)
    {
      tbdmeansum += tbdVol[j];
      for (int k = j + 1; k < 100; k++)
      {
        if (tbdVol[k] < tbdVol[j])
        {
          tbdBuff = tbdVol[k];
          tbdVol[k] = tbdVol[j];
          tbdVol[j] = tbdBuff;
        }
      }
    }

    tbdavg = tbdmeansum / numVar;
    medianVal = (tbdVol[numVar / 2] + tbdVol[(numVar / 2) + 1]) / 2;

    for (int l = 0; l < 100; l++)
    {
      medianBuff[l] = abs(tbdVol[l] - medianVal);
    }

    for (int j = 0; j < 100; j++)
    {
      for (int k = j + 1; k < 100; k++)
      {
        if (medianBuff[k] < medianBuff[j])
        {
          tbdBuff = medianBuff[k];
          medianBuff[k] = medianBuff[j];
          medianBuff[j] = tbdBuff;
        }
        if (meanBuff[k] < meanBuff[j])
        {
          tbdBuff = meanBuff[k];
          meanBuff[k] = meanBuff[j];
          meanBuff[j] = tbdBuff;
        }
      }
    }

    medianDeviation = (medianBuff[numVar / 2] + medianBuff[(numVar / 2) + 1]) / 2;
    tbdmeanD = (meanBuff[numVar / 2] + meanBuff[(numVar / 2) + 1]) / 2;

    for (int l = 0; l < 100; l++)
    {
      if (medianDeviation == 0)
      {
        modZscore[l] = (tbdVol[l] - medianVal) / (1.253314 * tbdmeanD);
      }
      else if (medianDeviation > 0)
      {
        modZscore[l] = (tbdVol[l] - medianVal) / (1.486 * medianDeviation);
      }
    }

    for (int z = 0; z < 100; z++)
    {
      if ((modZscore[z] > 1) || (modZscore[z] < -1))
      {
        tbdVol[z] = 0;
        outlierCount += 1;
      }
      else if ((modZscore[z] < 1) && (modZscore[z] > -1))
      {
        nonOutlierCount += 1;
      }
    }

    for (int y = 0; y < 100; y++)
    {
      tbdvolSum += tbdVol[y];
    }

    tbdvolAvg = tbdvolSum / nonOutlierCount;

    if ((sizeof(tbdVol) / sizeof(tbdVol[0]) != numVar) || (nonOutlierCount == 0) || (tbdvolAvg > tbdCalibVal) )
    {
      relativeVal = prevVal;
    }
    else
    {
      relativeVal = 100. - (tbdvolAvg / tbdCalibVal) * 100.;
    }

    prevVal = relativeVal;

    Serial.print("The Relative Turbidity is:");
    Serial.print(relativeVal);
    Serial.println(" % ");
    Serial.print("Voltage:");
    Serial.println(tbdvolAvg);
    Serial.print("Calibrated Voltage:");
    Serial.println(tbdCalibVal);
    Serial.print("Non Outlier:");
    Serial.println(nonOutlierCount);
    Serial.print("Outlier:");
    Serial.println(outlierCount);

    tbdvolSum = 0;
    tdsvolSum = 0;
    nonOutlierCount = 0;
    outlierCount = 0;
    tdsOutlier = 0;
    tdsNonOutlier = 0;
    tdsmeansum = 0;
    tbdmeansum = 0;

    ThingSpeak.setField(1, TempC);
    ThingSpeak.setField(2, tdsValue);
    ThingSpeak.setField(3, phValue);
    ThingSpeak.setField(4, relativeVal);

    int x = ThingSpeak.writeFields(channelNumber, myWriteAPIKey);

    if (x == 200)
    {
      Serial.println("Channel update successful");
    }
    else
    {
      Serial.println("Problem updating channel. HTTP error code" + String(x));
    }
    Time = millis();
  }
}