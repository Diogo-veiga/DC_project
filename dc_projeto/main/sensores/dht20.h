#pragma once
#include "driver/i2c_master.h"
#include "esp_err.h"

typedef struct{
    float humidade;
    float temperatura;
}sensor;