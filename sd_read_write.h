#ifndef __SD_READ_WRITE_H
#define __SD_READ_WRITE_H

#include "FS.h"
#include "SD.h"
#include "SPI.h"

// Pinos do módulo SD (sem conflito com o HX711)
#define SD_SCK   40
#define SD_MISO  39
#define SD_MOSI  41
#define SD_CS    42

void listDir(fs::FS &fs, const char * dirname, uint8_t levels);
void createDir(fs::FS &fs, const char * path);
void removeDir(fs::FS &fs, const char * path);
void readFile(fs::FS &fs, const char * path);
void writeFile(fs::FS &fs, const char * path, const char * message);
void appendFile(fs::FS &fs, const char * path, const char * message);
void renameFile(fs::FS &fs, const char * path1, const char * path2);
void deleteFile(fs::FS &fs, const char * path);
void testFileIO(fs::FS &fs, const char * path);

#endif
