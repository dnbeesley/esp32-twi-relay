#include <ArduinoJson.h>

/**
 * @brief Defines a i2c device to read from
 *
 */
typedef struct ReadDevice
{
    unsigned char address;
    size_t length;
};

/**
 * @brief Loads the conents of /config.json into a JSON document
 *
 * @param doc The JSON document
 */
void loadConfig(JsonDocument &doc);
