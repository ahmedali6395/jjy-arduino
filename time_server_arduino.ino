#include <WiFi.h>
#include <time.h>

#include "esp_sntp.h"
#include "time-services.h"
#include "esp32-hal-cpu.h" 


// Wi-Fi configuration
const char* ssid = "Roorda";
const char* password = "comcast1";

// Time server
const char* NTP = "pool.ntp.org";

// Service to be used
const time_service service = JJY;

// Pin to be used for PWM
const int FLASH_PIN = 5;
const int SIGNAL_PIN = 18;
const int SNTP_ACQUIRED_INDICATOR_PIN = 17;
const int WIFI_ACQUIRED = 16; 

// Frequency to transmit
const int frequency = 40000;
const int resolution = 8;

void setup(void) {
    Serial.begin(115200);

    pinMode(FLASH_PIN, OUTPUT);
    pinMode(SNTP_ACQUIRED_INDICATOR_PIN, OUTPUT); 
    pinMode(WIFI_ACQUIRED, OUTPUT);

    digitalWrite(FLASH_PIN, LOW); 
    digitalWrite(SNTP_ACQUIRED_INDICATOR_PIN, LOW); 
    digitalWrite(WIFI_ACQUIRED, LOW); 


    int i = 0; 

    while(i<5){
        delay(200); 
        digitalWrite(SNTP_ACQUIRED_INDICATOR_PIN, HIGH); 
        Serial.write("initializing!\n");
        delay(200); 
        digitalWrite(SNTP_ACQUIRED_INDICATOR_PIN, LOW); 
        i++; 
    }

    if (!ledcAttach(SIGNAL_PIN, frequency, resolution)) {
        Serial.println("Attaching signal pin failed!");
    }
    Serial.println("starting WiFi"); 
    WiFi.begin(ssid, password);
    Serial.println("Wifi started successfully"); 

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print("No connection\n");
        int status = digitalRead(WIFI_ACQUIRED); 
        digitalWrite(WIFI_ACQUIRED, !status); 
    }
    digitalWrite(WIFI_ACQUIRED, HIGH); 

    Serial.println("Connected!\n");


    // Get time from NTP 
    int SNTP_sync_count = 0; 
    configTime(0, 0, NTP);

    // Wait until sync is fully complete
    while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {
        Serial.println("Waiting for SNTP time sync...");
        delay(500);
        int status = digitalRead(SNTP_ACQUIRED_INDICATOR_PIN); 
        digitalWrite(SNTP_ACQUIRED_INDICATOR_PIN, !status);
        SNTP_sync_count++; 
        if(SNTP_sync_count>=60){
            Serial.println("ESP RESTART!"); 
            ESP.restart(); 
        }
    }
    digitalWrite(SNTP_ACQUIRED_INDICATOR_PIN, HIGH); 

    struct tm timeinfo;

    getLocalTime(&timeinfo); 

    setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0/2", 1);
    tzset();

    Serial.println(&timeinfo, "Current time adjusted to time zone: %Y-%m-%d %H:%M:%S\n");

    timeval tv;
    gettimeofday(&tv, NULL);

    
    int wait_time = 60 - (tv.tv_sec%60) - 1; 

    String to_print = "Waiting "+ String(wait_time)+" seconds before the top of the minute"; 
    Serial.println(to_print); 
    delay(wait_time*1000); 

}

void loop() {
    struct tm timeinfo;
    timeval tv;
    int64_t wait_us;
    getLocalTime(&timeinfo);
    gettimeofday(&tv, NULL);


    // Get the start of the minute in UNIX time (using this for convenience since time_services.cpp uses the time_t as opposed to tv_sec)
    time_t unix_time = time(NULL);
    time_t minute_start = unix_time - unix_time % 60 + 60 ;

    // Prepare the minute in accordance to JJY bits
    uint64_t minute_bits = prepareMinute(service, minute_start);

    for (int second = 0; second < 60; second++) {
        int modulation = getModulationForSecond(service, minute_bits, second);

        // Wait till the start of the second
        gettimeofday(&tv, NULL);
        wait_us = 1000000 - tv.tv_usec % 1000000;   
        delayMicroseconds(wait_us);

        if (service == JJY) {
            // transmit the PWM at 50% duty cycle
            ledcWrite(SIGNAL_PIN, 128);

            // Turn on indicator light
            digitalWrite(FLASH_PIN, HIGH);

            // output time at beginning of transmission
            getLocalTime(&timeinfo);
            Serial.println(&timeinfo, "Current time: %Y-%m-%d %H:%M:%S\n");
        }

        // modulation is presumabley in milliseconds
        delayMicroseconds(modulation * 1000);

        digitalWrite(FLASH_PIN, LOW);
        ledcWrite(SIGNAL_PIN, 0);
    }

    minute_start+=60; 
}