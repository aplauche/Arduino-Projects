#include <Arduino.h>
#include "sensirion_common.h"
#include "sgp30.h"

void setup() {
    Serial.begin(115200);
    Serial.println("Starting SGP30 test...");

    // Initialize sensor
    while (sgp_probe() != STATUS_OK) {
        Serial.println("SGP30 not found. Check wiring!");
        delay(500);
    }

    // Initialize air quality algorithm
    if (sgp_iaq_init() == STATUS_OK) {
        Serial.println("SGP30 initialized!");
    } else {
        Serial.println("SGP30 init failed!");
    }
}

void loop() {
    s16 err;
    u16 tvoc_ppb, co2_eq_ppm;

    err = sgp_measure_iaq_blocking_read(&tvoc_ppb, &co2_eq_ppm);
    if (err == STATUS_OK) {
        Serial.print("tVOC: ");
        Serial.print(tvoc_ppb);
        Serial.print(" ppb | CO2eq: ");
        Serial.print(co2_eq_ppm);
        Serial.println(" ppm");
    } else {
        Serial.println("Error reading IAQ values");
    }

    delay(1000);
}
