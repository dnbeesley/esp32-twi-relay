#include <SPIFFS.h>
#include "config.h"

/**
 * @brief Loads the conents of /config.json into a JSON document
 *
 * @param doc The JSON document
 */
void loadConfig(JsonDocument &doc)
{
    log_d("Beginning SPIFFS");
    if (!SPIFFS.begin(true))
    {
        log_e("An Error has occurred while mounting SPIFFS");
        return;
    }

    log_d("Loading config");

    delay(100);
    auto configFile = SPIFFS.open("/config.json", FILE_READ);

    if (!configFile)
    {
        log_e("Failed to open file for reading");
        return;
    }
    else
    {
        log_d("Opened file for reading");
    }

    auto error = deserializeJson(doc, configFile);
    configFile.close();

    if (error)
    {
        log_e("deserializeJson() failed: %s", error.c_str());
        return;
    }
}
