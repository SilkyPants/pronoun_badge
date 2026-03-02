#pragma once

#include <Arduino.h>

void initDisplay();
void drawImage(const char *filename, uint8_t x, uint8_t y, uint8_t w, uint8_t h);
void invertDisplay(bool invert);
void drawBootImage();