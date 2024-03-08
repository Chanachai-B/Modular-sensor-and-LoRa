#include <Arduino.h>
#include <EEPROM.h>
#include "control/LoRa.h"
#include "control/Sensor.h"
#include "control/Display.h"

#define BTN_ACTIVE_LCD 25
#define BTN_CHANGE_MODE 32
#define BTN_ACTIVE_AC 33
#define RELAY 2
#define GPIO_LCD GPIO_NUM_25
#define MODE_GPIO 0x302000000
#define SW_DEBOUNCE_TIME 400
#define PRESSED_RESET_TIME 5000
#define LCD_WORKING_TIME 30000
#define TIME_WAIT_INPUT_CMD 5000
#define PERIOD_INTERVAL 1000
#define ADDR_MODE 0x70

RTC_DATA_ATTR boolean debugMode = false;
hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

int state = 1;

int periodSentData = 0;
int statusSentData;
boolean statusLCD = false;
boolean lcdFlag = false;
boolean statusAC = false;
boolean calibrationIsRunning = false;
char cmd[10];
String sensorData = "";
unsigned long btnLcdPressed = 0;
unsigned long btnModePressed = 0;
unsigned long btnAcPressed = 0;
unsigned long previousMillisSw = 0;
unsigned long timeToPressedBTN = 0;
unsigned long timeToActiveLCD = 0;
unsigned long timeToPumpActive = 0;
unsigned long timeReadSensor = 0;
unsigned long lastTimeInterval = 0;
unsigned long stateFive = 0;
unsigned long checkPressedOneSec = 0;
unsigned long last[] = {0, 0, 0, 0, 0, 0, 0, 0};

int i = 0;
bool readSerial(char result[])
{
    while (Serial.available() > 0)
    {
        char inChar = Serial.read();
        if (inChar == '\n')
        {
            result[i] = '\0';
            Serial.flush();
            i = 0;
            return true;
        }
        if (inChar != '\r')
        {
            result[i] = inChar;
            i++;
        }
        delay(1);
    }
    return false;
}

void updateConfigMode(boolean mode)
{
    EEPROM.put(ADDR_MODE, mode);
    EEPROM.commit();
    EEPROM.get(ADDR_MODE, debugMode);
}

void getConfigMode()
{
    EEPROM.get(ADDR_MODE, debugMode);
}

void setMode(boolean mode)
{
    if (!mode)
    {
        esp_sleep_enable_timer_wakeup(10 * 60 * 1000 * 1000);
    }
    else
    {
        esp_sleep_enable_timer_wakeup(10 * 1000 * 1000);
    }
}

void changeMode()
{
    getConfigMode();
    statusLCD = true;
    Serial.println("Change mode");
    debugMode = !debugMode;
    setMode(debugMode);
    Serial.println(debugMode);
    timeToActiveLCD = millis();
    showMode(debugMode);
    updateConfigMode(debugMode);
}

void changeDisplay()
{
    portENTER_CRITICAL_ISR(&timerMux);
    lcdFlag = true;
    portEXIT_CRITICAL_ISR(&timerMux);
}

void enableTimeInterrupt()
{
    timer = timerBegin(0, 80, true);                   // Timer 0, divider 80
    timerAttachInterrupt(timer, &changeDisplay, true); // Attach callback function
    timerAlarmWrite(timer, 5000000, true);             // Set period to 5 second (5,000,000 microseconds)
    timerAlarmEnable(timer);                           // Enable the timer
}

void showDisplay()
{
    sensorData = readSensorAll();
    if (lcdState == 0 || lcdState == 1)
    {
        timeToActiveLCD = millis();
        lcdState = 2;
        lcdFirstPage(batt, t_in_b, h_in_b);
    }
    else if (lcdState == 2)
    {
        timeToActiveLCD = millis();
        lcdState = 1;
        lcdSecondPage(ecValue, phValue);
    }
}

void setPumpWorking(boolean status)
{
    statusAC = status;
    Serial.print("setPumpWorking");
    Serial.println(status);
    digitalWrite(RELAY, status);
    // Serial.println(status);
}

void toggleAC()
{
    statusAC = !statusAC;
    setPumpWorking(statusAC);
}

void checkWakeupReason()
{
    uint64_t GPIO_reason = esp_sleep_get_ext1_wakeup_status();
    uint64_t pin = (log(GPIO_reason)) / log(2);
    switch (pin)
    {
    case 25:
        Serial.println("Wakeup by GPIO25");
        Serial.print("Show LCD");
        timeToActiveLCD = millis();
        statusLCD = true;
        enableTimeInterrupt();
        showDisplay();
        break;
    case 32:
        Serial.println("Wakeup by GPIO32");
        Serial.print("Change mode");
        changeMode();
        break;
    case 33:
        Serial.println("Wakeup by GPIO33");
        Serial.print("Active AC");
        btnState = 13;
        // toggleAC();
        break;
    default:
        Serial.print("ESP32 wake up in another cause!!!!");
    }
}

void setTimerPump()
{
    timeWorkingPump = timeWorkingPump * 60 * 1000;
}

IRAM_ATTR void btnActiveLcdIsPressed()
{
    if (millis() - btnLcdPressed >= SW_DEBOUNCE_TIME)
    {
        // statusSentData = false;
        btnState = 11;
        timeToPressedBTN = millis();
        // Serial.println("btn lcd is pressed");
    }
    btnLcdPressed = millis();
}

IRAM_ATTR void btnChangeModeIsPressed()
{
    if (millis() - btnModePressed >= SW_DEBOUNCE_TIME)
    {
        // statusSentData = false;
        btnState = 12;
        // Serial.println("btn mode is pressed");
    }
    btnModePressed = millis();
}

IRAM_ATTR void btnActiveAcIsPressed()
{
    if (millis() - btnAcPressed >= SW_DEBOUNCE_TIME)
    {
        btnState = 13;
        // Serial.println("btn AC is pressed");
    }
    btnAcPressed = millis();
}

void setup()
{
    EEPROM.begin(64);
    Serial.begin(115200);
    //  EEPROM.put(ADDR_MODE, debugMode);
    //  EEPROM.commit();
    initLoRa();
    Serial.println("LoRa init");
    lcd.init();
    Serial.println("lcd init");
    dht.begin();
    Serial.println("dht init");
    pinMode(BTN_ACTIVE_LCD, INPUT);
    pinMode(BTN_CHANGE_MODE, INPUT);
    pinMode(BTN_ACTIVE_AC, INPUT);
    pinMode(RELAY, OUTPUT);

    attachInterrupt(digitalPinToInterrupt(BTN_ACTIVE_LCD), btnActiveLcdIsPressed, RISING);
    attachInterrupt(digitalPinToInterrupt(BTN_CHANGE_MODE), btnChangeModeIsPressed, RISING);
    attachInterrupt(digitalPinToInterrupt(BTN_ACTIVE_AC), btnActiveAcIsPressed, RISING);
    esp_sleep_enable_ext1_wakeup(MODE_GPIO, ESP_EXT1_WAKEUP_ANY_HIGH);
    checkWakeupReason();

    Serial.println("Curren mode : ");
    getConfigMode();
    Serial.println(debugMode);
    setMode(debugMode);
    Serial.println("ESP Active");
}

void loop()
{
    if (lcdFlag) // Change the screen every time. When function changeDisplay works.
    {
        sensorData = readSensorAll();
        if (lcdState == 1)
        {
            lcdState = 2;
            lcdFlag = false;
            lcdFirstPage(batt, t_in_b, t_in_b);
        }
        else
        {
            lcdState = 1;
            lcdFlag = false;
            lcdSecondPage(ecValue, phValue);
        }
    }
    if (millis() - timeToActiveLCD >= LCD_WORKING_TIME && statusLCD) // Control screen to turn off.
    {
        Serial.println("lcd off");
        statusLCD = false;
        lcdShutdown();
        // timerAlarmDisable(timer);
    }
    if (btnState == 11) // Pressed button GPIO25
    {
        checkPressedOneSec = millis();
        statusLCD = true;
        int count = 0;
        while (digitalRead(BTN_ACTIVE_LCD) != 0)
        {
            Serial.println("Pressed");
            if (millis() - timeToPressedBTN >= PRESSED_RESET_TIME)
            {
                Serial.println("Reset ESP");
                esp_restart();
            }
            if (millis() - checkPressedOneSec >= PERIOD_INTERVAL)
            {
                checkPressedOneSec = millis();
                count++;
                lcdPressedBTN(count);
            }
            // Serial.println(digitalRead(BTN_ACTIVE_LCD));
        }
        sensorData = readSensorAll();
        Serial.println(sensorData);
        Serial.println(lcdState);
        if (lcdState == 0)
        {
            timeToActiveLCD = millis();
            // lcdActive();
            lcdState = 1;
            showDisplay();
            enableTimeInterrupt();
        }
        else
        {
            timeToActiveLCD = millis();
            showDisplay();
        }
        
        btnState = 0;
    }
    if (btnState == 12) // Pressed button GPIO35
    {
        changeMode();
        btnState = 0;
    }

    if (btnState == 13)
    {
        Serial.println("btn AC is pressed");
        toggleAC();
        state = 1;
        // sensorData = readSensorAll();
        // sentLoRa(sensorData);
        btnState = 0;
    }

    if (state == 1) // Read sensor all
    {
        sensorData = readSensorAll();
        Serial.print("state : ");
        Serial.println(state);
        state++;
    }
    if (state == 2) // Sent data to geteway
    {
        Serial.print("state : ");
        Serial.println(state);
        sentLoRa(sensorData + "," + statusAC);
        if (!statusLCD)
        {
            sentDataPage(periodSentData);
        }
        state++;
    }
    if (state == 3) // Receive data from gateway
    {
        if (statusAC)
        {
            pumpActive();
        }
        int packetSize = LoRa.parsePacket();
        Serial.print("state : ");
        Serial.println(state);
        loraTime = millis();
        while (true)
        {
            // waitDataInLoop();
            packetSize = LoRa.parsePacket();
            if (millis() - timeToPumpActive >= timeWorkingPump && timeWorkingPump != 0)
            {
                Serial.println(timeWorkingPump);
                setPumpWorking(LOW);
                lcdShutdown();
                timeWorkingPump = 0;
                state = 4;
                timeReadSensor = millis();
                break;
            }
            else if (packetSize) // ESP32 have receives data
            {
                Serial.println("packetSize");
                statusSentData = checkDataLoRa();
                Serial.print("statusSentData : ");
                Serial.println(statusSentData);
                if (statusSentData == 0)
                {
                    // receivePage(true);
                    Serial.println("LoRa send success");
                    state = 4;
                    timeReadSensor = millis();
                    break;
                }
                else if (statusSentData == 1)
                {
                    Serial.println("Set timer pump");
                    setTimerPump();
                    setPumpWorking(HIGH);
                    Serial.print("pump timer : ");
                    Serial.println(timeWorkingPump / 1000);
                    timeToPumpActive = millis();
                    break;
                }
                else if (statusSentData == 2)
                {
                    Serial.println("Pump start working. Control by humidity");
                    setPumpWorking(HIGH);
                    break;
                }
                else if (statusSentData == 3)
                {
                    Serial.println("Pump start working. Control by blynk");
                    setPumpWorking(HIGH);
                    break;
                }
                else if (statusSentData == 4)
                {
                    Serial.println("Pump stop working. Control by blynk");
                    lcdShutdown();
                    setPumpWorking(LOW);
                    state = 1;
                    // sensorData = readSensorAll();
                    delay(1000);
                    // sentLoRa(sensorData+","+statusAC);
                    break;
                }
                else if (statusSentData == 10)
                {
                    Serial.println("ESP32 restart from web");
                    esp_restart();
                }
                else if (statusSentData == 11 && periodSentData < 5 && millis() - loraTime >= 5000)
                {
                    // receivePage(false);
                    periodSentData++;
                    Serial.print("LoRa sent fail resent : ");
                    Serial.println(periodSentData);
                    state = 2;
                    break;
                }
                // else
                // {
                //     state = 3;
                //     break;
                // }
            }
            // else if (statusAC == 1){
            //     state = 3;
            //     break;
            // }
            else if (millis() - loraTime >= 5000 && statusSentData == 11) // Data not received within 5 seconds. Send again.
            {
                periodSentData++;
                Serial.print("LoRa sent fail resent : ");
                Serial.println(periodSentData);
                state = 2;
                break;
            }
            else if (periodSentData >= 5) // Data not received within 25 seconds. Send fail.
            {
                periodSentData = 0;
                Serial.println("LoRa sent fail");
                state = 4;
                timeReadSensor = millis();
                break;
            }
            else if (btnState == 11 || btnState == 12 || btnState == 13) // Button is pressed. Exit loop.
            {
                break;
            }
        }
    }

    if (state == 4)
    {
        if (statusAC == 1)
        {
            state = 3;
            periodSentData = 0;
            return;
        }
        // Serial.println(millis() - timeReadSensor);
        if (millis() - timeReadSensor <= TIME_WAIT_INPUT_CMD || calibrationIsRunning)
        {
            if (millis() - lastTimeInterval >= PERIOD_INTERVAL)
            {
                Serial.println("Calibrate ec & pH is not running");
                lastTimeInterval = millis();
                if (calibrationIsRunning)
                {
                    Serial.println(F("[main]...>>>>>> calibration is running, to exit send exitph or exitec through serial <<<<<<"));
                    // EC
                    ecVoltage = ecSensor.readADC_SingleEnded(0) / 10;
                    Serial.print(F("[EC Voltage]... ecVoltage: "));
                    Serial.println(ecVoltage);
                    ecValue = EC.readEC(ecVoltage, temperature); // convert voltage to EC with temperature compensation
                    Serial.print(F("[EC Read]... EC: "));
                    Serial.print(ecValue);
                    Serial.println(F("ms/cm"));
                    // pH
                    phVoltage = pHSensor.readADC_SingleEnded(0) / 10;
                    Serial.print(F("[pH Voltage]... phVoltage: "));
                    Serial.println(phVoltage);
                    phValue = PH.readPH(phVoltage, temperature);
                    Serial.print(F("[pH Read]... pH: "));
                    Serial.println(phValue);
                }
                if (readSerial(cmd))
                {
                    strupr(cmd);

                    if (calibrationIsRunning || strstr(cmd, "PH") || strstr(cmd, "EC"))
                    {
                        calibrationIsRunning = true;
                        Serial.println(F("[]... >>>>>calibration is now running PH and EC are both reading, if you want to stop this process enter EXITPH or EXITEC in Serial Monitor<<<<<"));
                        if (strstr(cmd, "PH"))
                        {
                            PH.calibration(phVoltage, temperature, cmd); // PH calibration process by Serail CMD
                        }
                        if (strstr(cmd, "EC"))
                        {
                            EC.calibration(ecVoltage, temperature, cmd); // EC calibration process by Serail CMD
                        }
                    }
                    if (strstr(cmd, "EXITPH") || strstr(cmd, "EXITEC"))
                    {
                        calibrationIsRunning = false;
                        timeReadSensor = millis();
                    }
                }
            }
        }
        else
        {
            state = 5;
            return;
        }
    }

    if (state == 5 && !statusLCD) // ESP32 sleep
    {
        stateFive = millis();
        bool statusData;
        int count = 5;
        if (statusSentData == 0)
        {
            statusData = true;
            receivePage(true);
        }
        else if (periodSentData >= 5)
        {
            statusData = false;
            receivePage(false);
        }
        while (true)
        {
            if (btnState == 11 || btnState == 12 || btnState == 13) // Button is pressed. Exit loop.
            {
                break;
            }
            if (millis() - stateFive >= PERIOD_INTERVAL)
            {
                stateFive = millis();
                sleepPage(count, statusData);
                count--;
                Serial.println(count);
            }
            if (count < 0)
            {
                lcdShutdown();
                Serial.print("state : ");
                Serial.println(state);
                Serial.println("ESP Sleep");
                updateConfigMode(debugMode);
                Serial.println(debugMode);
                esp_deep_sleep_start();
            }
        }
    }
}