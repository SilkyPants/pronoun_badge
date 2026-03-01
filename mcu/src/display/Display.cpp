
#include "Display.h"

#include <LittleFS.h>

#include "common.h"

#ifdef USE_TFT_ESPI
#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();
#endif

#ifdef USE_SSD1315
#include <U8g2lib.h>

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(
    U8G2_R2,
    /*reset =*/U8X8_PIN_NONE,
    /*clock =*/OLED_SCL,
    /*data  =*/OLED_SDA
  );
#endif

#ifdef BOOT_IMAGE
/* This all assumes that:
- Include file lives under boot_images
- is named {BOOT_IMAGE}.h
- has the data defined as {BOOT_IMAGE}_bits
*/

/* Include the dynamic BOOT_IMAGE header file */
/* We build the path as a single token sequence, then stringize it.
   Note: No quotes inside the 'PATH' macro. */
#define BOOT_IMAGES_PATH(img) boot_images/img.h
#define BOOT_INCLUDE_HEADER STR(BOOT_IMAGES_PATH(BOOT_IMAGE))

/* Include the resulting header */
#include BOOT_INCLUDE_HEADER
/* This results in: #define BOOT_IMAGE_BITS {BOOT_IMAGE}_bits */
#define BOOT_IMAGE_BITS GLUE(BOOT_IMAGE, _bits)

#endif

void initDisplay()
{
#ifdef USE_SSD1315
    // Create power for the OLED
    pinMode(OLED_GND, OUTPUT);
    digitalWrite(OLED_GND, LOW); // GND
    pinMode(OLED_VCC, OUTPUT);
    digitalWrite(OLED_VCC, HIGH); // VCC

    delay(100); // Wait for OLED to stabilize

    u8g2.begin();
#endif

#ifdef USE_TFT_ESPI
    // Initialize the display
    tft.init();
    tft.setRotation(1); // Landscape orientation
#endif

#ifdef BOOT_IMAGE
    drawBootImage();
#endif
}

void drawImage(const char *filename, uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
    File file = LittleFS.open(filename, "r");
    if (!file)
        return;

#ifdef USE_SSD1315

    size_t size = file.size();
    uint8_t *buffer = (uint8_t *)malloc(size);

    if (buffer)
    {
        file.read(buffer, size);
        u8g2.clearBuffer();
        // U8g2's drawXBM is designed for this specific byte format
        u8g2.drawXBM(x, y, w, h, buffer);
        u8g2.sendBuffer();
        free(buffer);
    }

#endif

#ifdef USE_TFT_ESPI
    tft.startWrite();
    tft.setAddrWindow(x, y, w, h);

    uint16_t lineBuffer[w];
    for (int row = 0; row < h; row++)
    {
        file.read((uint8_t *)lineBuffer, w * 2); // 2 bytes per pixel in RGB565
        tft.pushImage(x, y + row, w, 1, lineBuffer);
    }

    tft.endWrite();
#endif

    file.close();
}

void invertDisplay()
{
#ifdef USE_SSD1315
    // Draw a solid box over the SAME area as the image using XOR mode
    // Because it's XOR:
    // First time this runs: Image is inverted.
    // Second time this runs: Image is restored to original!
    u8g2.setDrawColor(2);
    u8g2.drawBox(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    u8g2.sendBuffer();
#endif

#ifdef USE_TFT_ESPI
    tft.invertDisplay(invert);
#endif
}

void drawBootImage()
{
#ifdef USE_TFT_ESPI
    tft.pushImage(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, BOOT_IMAGE_BITS);
#else
    u8g2.clearBuffer();
    u8g2.drawXBMP(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, BOOT_IMAGE_BITS);
    u8g2.sendBuffer();
#endif
}