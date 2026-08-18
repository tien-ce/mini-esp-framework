#include "core/file_system.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
bool init_file_system(bool formatonfail, const char *basepath, uint8_t maxopenfiles, const char *partitionlable)
{
  return LittleFS.begin(formatonfail, basepath, maxopenfiles, partitionlable);
}
size_t file_system_get_size()
{
  return LittleFS.totalBytes();
}
size_t file_system_get_used()
{
  return LittleFS.usedBytes();
}
/* file and directory interaction */
char* readFile(const char *path, unsigned int *bytesRead)
{
    File file = LittleFS.open(path, "r", false);
    if (!file || file.isDirectory()) {
        *bytesRead = 0;
        return NULL;
    }

    size_t size = file.size();
    #ifdef SYSTEMS_USE_PSRAM
    char *buffer = (char*) ps_malloc(size + 1);
    #else
    char *buffer = (char*)malloc(size + 1);
    #endif
    if (!buffer) {
        file.close();
        if (bytesRead) *bytesRead = 0;
        return NULL;
    }

    size_t readLen = file.readBytes(buffer, size);
    buffer[readLen] = '\0';
    file.close();

    *bytesRead = readLen;
    return buffer;
}

unsigned int writefile(const char *path, const char *data, unsigned int length)
{
    File file = LittleFS.open(path, "w", false);
    if (!file || file.isDirectory()) {
        return 0;
    }
    
    size_t bytesWrite = file.write((const uint8_t*)data, length);
    file.close();
    
    return (unsigned int)bytesWrite;
}

char *list_file(const char *dir_path) {
    File root = LittleFS.open(dir_path);
    if (!root || !root.isDirectory()) {
        return NULL;
    }
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            obj[file.name()] = file.size();
        }
        file = root.openNextFile();
    }
    root.close();
    size_t jsonLen = measureJson(doc) + 1;
    char *jsonBuffer = (char*)malloc(jsonLen);
    if (!jsonBuffer) {
        return NULL;
    }
    serializeJson(doc, jsonBuffer, jsonLen);
    return jsonBuffer;
}

bool remove_file(const char *path) {
    if (!path || !LittleFS.exists(path)) {
        return false;
    }
    return LittleFS.remove(path);
}

bool rename_file(const char *pathFrom, const char *pathTo) {
    if (!pathFrom || !pathTo || !LittleFS.exists(pathFrom)) {
        return false;
    }
    return LittleFS.rename(pathFrom, pathTo);
}

bool make_directory(const char *path) {
    if (!path || LittleFS.exists(path)) {
        return false;
    }
    return LittleFS.mkdir(path);
}

bool remove_dir(const char *path) {
    if (!path || !LittleFS.exists(path)) {
        return false;
    }
    return LittleFS.rmdir(path);
}
