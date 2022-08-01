#include <Arduino.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <WiFi.h>
#include <ThingSpeak.h>
#include <EEPROM.h>
#include <Preferences.h>

Preferences storage;

const int BUS_PIN = D1;
const int TDS_PIN = 34;
const int PH_PIN = 39;
const int TBD_PIN = 35;
OneWire Sensor_Bus(BUS_PIN);
DallasTemperature Sensor(&Sensor_Bus);

const int numVar = 100;

// Variables for Turbidity Values
float tbdCalibVal;
float relativeVal;

// Variable for TDS Values
float tdsValue;

// Variable for Temperature Value
float TempC;

// Variables for PH Values
float phValue;
float phValue4;
float phValue7;
float phVol;

// Measurement Flags
int sensorFlag = 1;
int measurementFlag = 0;

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

  if ((millis() - Time > publishTime)||(measurementFlag == 0))
  {
    switch (sensorFlag)
    {
      case 1:
        if(getTemp(TempC))
        {
          sensorFlag = 2;
        }
        else
        {
          sensorFlag = 1;
        }
        measurementFlag = 0;
      break;
      case 2:
        if(getPH(phVol,phValue4,phValue7,phValue))
        {
          sensorFlag = 3;
        }
        else
        {
          sensorFlag = 2;
        }
        measurementFlag = 0;
      break;
      case 3:
        if(getTDS(tdsValue,TempC))
        {
          sensorFlag = 4;
        }
        else
        {
          sensorFlag = 3;
        }
      break;
      case 4:
        if(getTBD(relativeVal))
        {
          //Serial.println("Getting TBD Value");
          sensorFlag = 1;
          measurementFlag = 1;

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
        }
        else
        {
          sensorFlag = 4;
          measurementFlag = 0;
        }
      break;
    }
    Time = millis();
  }
}

bool getTemp(float &tempVal)
{
  Sensor.requestTemperatures();
  tempVal = Sensor.getTempCByIndex(0);

  if(tempVal < 0 )
  {
    Serial.print("Temperature Reading Error");
    return false;
  }
  else
  {
    Serial.print("The temperature is:");
    Serial.println(tempVal);
    return true;
  }
  
}

bool getPH(float phVol,float phValue4,float phValue7, float &phValue)
{
  analogSetPinAttenuation(PH_PIN, ADC_11db);
  phVol = analogRead(PH_PIN) * (3.3 / 4095.) * 1000.;

  if(phVol == 0)
  {
    Serial.print("PH Reading Error");
    return false;
  }
  else
  {
    float slope = (7.0 - 4.0) / ((phValue7 - 1500.0) / 3.0 - (phValue4 - 1500.0) / 3.0);
    float intercept = 7.0 - slope * (phValue7 - 1500.0) / 3.0;
    phValue = slope * (phVol - 1500.0) / 3.0 + intercept;
    Serial.print("The PH value is:");
    Serial.println(phValue);
    Serial.println(phVol);

    return true;
  }
}

bool getTDS(float &tdsValue, float tempVal)
{
  float volVal[numVar];
  for (int a = 0; a < numVar; a++)
  {
    volVal[a] = analogRead(TDS_PIN) * 3.3 / 4095.;

    if(volVal[a] == 0)
    {
      Serial.print("TDS Reading Error");
      return false;
    }
  }

  // Rearrange Array from Smallest to Largest
  arrayArrange(volVal,numVar);

  // Mean
  float tdsSum = getSum(volVal,numVar);
  float tdsMean = getMean(tdsSum,numVar);

  //Median
  float tdsMedian = getMedian(volVal, numVar);

  //Median Deviations
  // Pointer to the Array
  float *tdsMedianDeviationPointer;
  tdsMedianDeviationPointer = getMedianDeviations(volVal,tdsMedian,numVar);

  //Mean Deviations
  // Pointer to the Array
  float *tdsMeanDeviationPointer;
  tdsMeanDeviationPointer = getMeanDeviations(volVal,tdsMean,numVar); 

  //Rearrange Median Buff
  arrayArrange(tdsMedianDeviationPointer,numVar);

  //Rearrange Mean Buff
  arrayArrange(tdsMeanDeviationPointer,numVar);

  //Find tdsMAD
  float tdsMAD = getMedian(tdsMedianDeviationPointer,numVar);

  //Find tdsmeanD
  float tdsMeanD = getMedian(tdsMeanDeviationPointer,numVar);

  //Find Z Score Value
  // Pointer to Z Score Array
  float *tdsZScorePointer;
  tdsZScorePointer = zScore(volVal,tdsMAD,tdsMedian,tdsMeanD,numVar);

  //Find Outlier Count
  float tdsOutlier = 0;
  float tdsNonOutlier = 0;
  outlierCount(tdsZScorePointer,volVal,tdsOutlier,tdsNonOutlier);
  if((sizeof(volVal)/sizeof(volVal[0]) != numVar)||(tdsNonOutlier == 0))
  {
    return false;
  }
  else
  {
    float tdsvolSum = getSum(volVal,numVar);

    float tdsvolAvg = tdsvolSum / tdsNonOutlier;
    float tempComp = 1 + 0.02 * (tempVal - 25.);
    float volComp = tdsvolAvg / tempComp;
    tdsValue = (133.42 * volComp * volComp * volComp - 255.86 * volComp * volComp + 857.39 * volComp) * 0.5;
    Serial.print("The TDS value is:");
    Serial.println(tdsValue);

    return true;
  }
}

bool getTBD(float &relativeVal)
{
  float tbdVol[numVar];
  for (int i = 0; i < numVar; i++)
  {
    // 5V input voltage
    // tbdVol[i] = analogRead(TBD_PIN)*(4.5/4095.)*(0.733/0.719);
    tbdVol[i] = analogRead(TBD_PIN) * (3.3 / 4095.);

    if(tbdVol[i] == 0)
    {
      Serial.print("TBD Reading Error");
      return false;
    }
  }

  // Rearrange Array from Smallest to Largest
  arrayArrange(tbdVol,numVar);

  //Mean
  float tbdSum = getSum(tbdVol, numVar);
  float tbdMean = getMean(tbdSum,numVar);

  //Median
  float tbdMedian = getMedian(tbdVol,numVar);

  //Median Deviations
  //Pointer to Array
  float * tbdMedianDeviationPointer;
  tbdMedianDeviationPointer = getMedianDeviations(tbdVol,tbdMedian,numVar);

  //Mean Deviations
  //Pointer to Array
  float * tbdMeanDeviationPointer;
  tbdMeanDeviationPointer = getMeanDeviations(tbdVol,tbdMean,numVar);

  //Rearrange Median Buff
  arrayArrange(tbdMedianDeviationPointer,numVar);

  //Rearrange Mean Buff
  arrayArrange(tbdMeanDeviationPointer,numVar);

  //Find tbdMAD
  float tbdMAD = getMedian(tbdMedianDeviationPointer,numVar);

  //Find tdsmeanD
  float tbdMeanD = getMedian(tbdMeanDeviationPointer,numVar);

   //Find Z Score Value
  // Pointer to Z Score Array
  float *tbdZScorePointer;
  tbdZScorePointer = zScore(tbdVol,tbdMAD,tbdMedian,tbdMeanD,numVar);

  //Find Outlier Count
  float tbdOutlier = 0;
  float tbdNonOutlier = 0;
  outlierCount(tbdZScorePointer,tbdVol,tbdOutlier,tbdNonOutlier);

  if(((sizeof(tbdVol))/sizeof(tbdVol[0]) != numVar)||(tbdNonOutlier == 0))
  {
    Serial.println("TBD Reading Error");
    return false;
  }
  else
  {
    float tbdvolSum = getSum(tbdVol,numVar);

    float tbdvolAvg = tbdvolSum/tbdNonOutlier;
    if(tbdvolAvg > tbdCalibVal)
    {
      Serial.print("TBD Reading Error");
      return false;
    }
    else
    {
      relativeVal = 100. - (tbdvolAvg/tbdCalibVal) * 100.;
      Serial.print("The Relative Turbidity is:");
      Serial.print(relativeVal);
      Serial.println(" % ");
      //Serial.print("Voltage:");
      //Serial.println(tbdvolAvg);
      //Serial.print("Calibrated Voltage:");
      //Serial.println(tbdCalibVal);
      //Serial.print("Non Outlier:");
      //Serial.println(tbdOutlier);
      //Serial.print("Outlier:");
      //Serial.println(tbdNonOutlier);

      return true;
    }
  }
}

void arrayArrange(float array[], const int size)
{
  float arrayBuff;

  for (int f = 0; f < size; f++)
  {
    for (int d = f + 1; d < size; d++)
    {
      if (array[d] < array[f])
      {
        arrayBuff = array[d];
        array[d] = array[f];
        array[f] = arrayBuff;
      }
      }
  }
}

float getSum(float voltage[], const int size)
{
  float sum = 0;

  for(int b = 0; b < size; b++)
  {
    sum += voltage[b];
  }

  return sum;
}

float getMean(float sum, const int size)
{
  float avg = sum / size;
  return avg;
}

float getMedian(float array[], const int size)
{
  float medianVal = ((array[size / 2]) + array[(size / 2) + 1]) / 2;
  return medianVal;
}

float * getMedianDeviations(float volArray[], float medianVal, const int size)
{
  static float medBuff[numVar];
  for(int g = 0; g<size; g++)
  {
    medBuff[g] = abs(volArray[g] - medianVal);
  }
  return medBuff;
}

float * getMeanDeviations(float volArray[], float meanVal, const int size)
{
  static float meanBuff[numVar];
  for(int j = 0; j < numVar; j++)
  {
    meanBuff[j] = abs(volArray[j] - meanVal);
  }
  return meanBuff;
}

float * zScore(float volArray[],float medianDeviationVal, float medianVal,float meanDeviationVal, const int size)
{
  static float tdsZScore[numVar];
  for (int l = 0; l < size; l++)
  {
    if (medianDeviationVal == 0)
    {
      tdsZScore[l] = (volArray[l] - medianVal) / (1.253314 * meanDeviationVal);
    }
    else if (medianDeviationVal > 0)
    {
      tdsZScore[l] = (volArray[l] - medianVal) / (1.486 * medianDeviationVal);
    }
  }
  return tdsZScore;
}

void outlierCount(float zScoreArray[],float volArray[],float &outlier, float &nonOutlier)
{
  for (int l = 0; l < numVar; l++)
    {
      if ((zScoreArray[l] > 1) || (zScoreArray[l] < -1))
      {
        volArray[l] = 0;
        outlier += 1;
      }
      else if ((zScoreArray[l] < 1) && (zScoreArray[l] > -1))
      {
        nonOutlier += 1;
      }
    }
}


