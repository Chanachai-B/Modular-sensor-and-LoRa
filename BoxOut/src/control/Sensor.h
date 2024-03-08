#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_ADS1X15.h>
#include <DHT.h>
#include "INA219.h"
#include <DFRobot_ESP_EC.h>
#include <DFRobot_ESP_PH_WITH_ADC.h>

#define luxSensor 0x23
#define DHTPIN 15
#define DHTTYPE DHT22
int btnState = 0;
float t_in_b, h_in_b, batt, ecVoltage, ecValue, phVoltage, phValue, temperature = 25, pumpVoltage;

// int cycleRead;
bool statusPump;
DFRobot_ESP_EC EC;
DFRobot_ESP_PH_WITH_ADC PH;
Adafruit_AHTX0 aht;
sensors_event_t humidity, temp;
Adafruit_ADS1115 ecSensor;
Adafruit_ADS1115 pHSensor;
Adafruit_ADS1115 nonContactWater;

uint8_t buf[4] = {0};
DHT dht(DHTPIN, DHTTYPE);
INA219 sensorBatt(0x40);

// void initSensorBatt()
// {
//     if (!sensorBatt.begin())
//     {
//         // Serial.println("could not connect INA219. Fix and Reboot");
//         batt = 0;
//     }
// }

// void initSensorTempInAir()
// {
//     if (!aht.begin())
//     {
//         // Serial.println("Could not find AHT? Check wiring");
//         t_in_a = 0;
//         h_in_a = 0;
//     }
// }

// void initSensorEC()
// {
//     if (!ecSensor.begin())
//     {
//         // Serial.println("Failed to initialize ADS.");
//         h_in_s = 0;
//     }
// }

// void initSensorPH()
// {
//     if (!pHSensor.begin(0x49))
//     {
//         // Serial.println("Failed to initialize ADS.");
//         h_in_s = 0;
//     }
// }

// void initSensorNonContactWater()
// {
//     if (!nonContactWater.begin(0x4A))
//     {
//         // Serial.println("Failed to initialize ADS.");
//         h_in_s = 0;
//     }
// }

// void initSensorTempInBox()
// {
//     dht.begin();
// }

// void initSensor()
// {
//     initSensorEC();
//     initSensorPH();
//     initSensorNonContactWater();
//     initSensorBatt();
//     initSensorTempInAir();
//     initSensorHumiInSoil();
//     initSensorTempInBox();
// }

void setupWaterSensor()
{
    Serial.begin(115200);
    EEPROM.begin(64);
    ecSensor.setGain(GAIN_ONE);
    pHSensor.setGain(GAIN_ONE);
    nonContactWater.setGain(GAIN_ONE);
    // ecSensor.begin(0x48);
    // pHSensor.begin(0x49);
    EC.begin();
    PH.begin();
}

void readBatt()
{
    if (!sensorBatt.begin())
    {
        Serial.println("could not connect INA219. Fix and Reboot");
        batt = 0;
    }
    else
    {
        sensorBatt.setMaxCurrentShunt(5, 0.002);
        batt = sensorBatt.getBusVoltage();
    }
}

void readWaterSensor()
{
    Serial.println("Waiting to read ec and ph.");
    while (true)
    {
        if (!ecSensor.begin(0x48))
        {
            Serial.println("EC Sensor not connect check wiring!!!");
            ecValue = 0;
        }
        else
        {
            Serial.print("temperature:");
            Serial.print(temperature, 1);
            Serial.println("^C");

            ecVoltage = ecSensor.readADC_SingleEnded(0) / 10;
            Serial.print("ecVoltage:");
            Serial.println(ecVoltage, 4);

            ecValue = EC.readEC(ecVoltage, temperature); // convert voltage to EC with temperature compensation
            Serial.print("EC:");
            Serial.print(ecValue, 4);
            Serial.println("ms/cm");
            break;
        }
        // if (calibrationIsRunning)
        // {
        //     Serial.println(F("[main]...>>>>>> calibration is running, to exit send exitph or exitec through serial <<<<<<"));
        //     // EC
        //     ecVoltage = ecSensor.readADC_SingleEnded(0) / 10;
        //     Serial.print(F("[EC Voltage]... ecVoltage: "));
        //     Serial.println(ecVoltage);
        //     ecValue = EC.readEC(ecVoltage, temperature); // convert voltage to EC with temperature compensation
        //     Serial.print(F("[EC Read]... EC: "));
        //     Serial.print(ecValue);
        //     Serial.println(F("ms/cm"));
        //     // pH
        //     phVoltage = pHSensor.readADC_SingleEnded(1) / 10;
        //     Serial.print(F("[pH Voltage]... phVoltage: "));
        //     Serial.println(phVoltage);
        //     phValue = PH.readPH(phVoltage, temperature);
        //     Serial.print(F("[pH Read]... pH: "));
        //     Serial.println(phValue);
        // }

        // if (readSerial(cmd))
        // {
        //     strupr(cmd);
        //     if (calibrationIsRunning || strstr(cmd, "PH") || strstr(cmd, "EC"))
        //     {
        //         calibrationIsRunning = true;
        //         Serial.println(F("[]... >>>>>calibration is now running PH and EC are both reading, if you want to stop this process enter EXITPH or EXITEC in Serial Monitor<<<<<"));
        //         if (strstr(cmd, "PH"))
        //         {
        //             PH.calibration(phVoltage, temperature, cmd); // PH calibration process by Serail CMD
        //         }
        //         if (strstr(cmd, "EC"))
        //         {
        //             EC.calibration(ecVoltage, temperature, cmd); // EC calibration process by Serail CMD
        //         }
        //     }
        //     if (strstr(cmd, "EXITPH") || strstr(cmd, "EXITEC"))
        //     {
        //         calibrationIsRunning = false;
        //     }
        // }

        // if (!calibrationIsRunning)
        // {

        //     phVoltage = pHSensor.readADC_SingleEnded(1) / 10; // read the voltage
        //     Serial.print("phVoltage:");
        //     Serial.println(phVoltage, 4);
        //     phValue = PH.readPH(phVoltage, temperature); // convert voltage to pH with temperature compensation
        //     Serial.print("pH:");
        //     Serial.println(phValue, 4);
        //     break;
        // }

        // if (btnState == 11 || btnState == 12 || btnState == 13)
        // {
        //     break;
        // }
    }
}

void readEcSensor()
{
    if (!ecSensor.begin(0x48))
    {
        Serial.println("Failed to initialize ecSensor.");
        ecValue = 0;
    }
    else
    {
        ecVoltage = ecSensor.readADC_SingleEnded(0) / 5;
        ecValue = EC.readEC(ecVoltage, temperature);
        if(ecValue < 0){
            ecValue = 0;
        }
    }
}

void readPhSensor()
{
    if (!pHSensor.begin(0x49))
    {
        Serial.println("Failed to initialize pHSensor.");
        phValue = 0;
    }
    else
    {
        phVoltage = pHSensor.readADC_SingleEnded(0) / 10;
        phValue = PH.readPH(phVoltage, temperature);
    }
}

void readStatusPump()
{
    if (!nonContactWater.begin(0x4A))
    {
        Serial.println("Failed to initialize nonContactWater.");
        statusPump = 0;
    }
    else
    {
        pumpVoltage = nonContactWater.readADC_SingleEnded(0);
        if (pumpVoltage >= 100)
        {
            statusPump = 1;
        }
        else
        {
            statusPump = 0;
        }
    }
}

void readTempInBox()
{
    h_in_b = dht.readHumidity();
    t_in_b = dht.readTemperature();

    if (isnan(h_in_b) || isnan(t_in_b))
    {
        Serial.println(F("Failed to read from DHT sensor!"));
        h_in_b = 0;
        t_in_b = 0;
        return;
    }
}

String readSensorAll()
{
    readBatt();
    // Serial.println("readBatt success");
    readTempInBox();
    // Serial.println("readTempInBox success");
    setupWaterSensor();
    // Serial.println("setupWaterSensor success");
    // readWaterSensor();
    // Serial.println("readWaterSensor success");
    readEcSensor();
    readPhSensor();
    readStatusPump();
    Serial.print("pumpVoltage");
    Serial.println(pumpVoltage);
    Serial.print("statusPump");
    Serial.println(statusPump);
    return String(t_in_b) + "," + String(h_in_b) + "," + String(ecValue) + "," + String(phValue) + "," + String(statusPump) + "," + String(batt);
    // return String(t_in_b) + "," + String(h_in_b) + "," + String(ecValue) + "," + String(phValue) + ",1," + String(batt);
}