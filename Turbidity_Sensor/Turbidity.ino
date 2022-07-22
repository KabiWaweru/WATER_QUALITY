#include <Arduino.h>
#include <Preferences.h>

Preferences storage;

const int TBD_PIN = 35;
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

void setup()
{
    Serial.begin(115200);
    storage.begin("phValues",false);
    tbdCalibVal = storage.getFloat("TBD_Value", 0.0);
}

void loop()
{
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
      else if ((tdsZScore[z] < 1) && (tdsZScore[z] > -1))
      {
        nonOutlierCount += 1;
      }
    }

    for (int y = 0; y < 100; y++)
    {
      tbdvolSum += tbdVol[y];
    }

    tbdvolAvg = tbdvolSum / nonOutlierCount;

    if ((tbdvolAvg > tbdCalibVal) || (tbdvolAvg == tbdCalibVal))
    {
      relativeVal = 0;
    }

    if ((sizeof(tbdVol) / sizeof(tbdVol[0]) != numVar) || (nonOutlierCount == 0))
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
}