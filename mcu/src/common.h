#pragma once

/* 1. Define internal helpers */
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define GLUE_HELPER(x, y) x ## y
#define GLUE(x, y) GLUE_HELPER(x, y)

// Unique IDs
#ifndef DEVICE_NAME
#define DEVICE_NAME "GlowBug"
#endif

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define COMMAND_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

