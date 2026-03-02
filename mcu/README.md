

Convert PNG to raw RGB565

```bash
# 1. Convert PNG to raw 16-bit RGB
magick input.png -resize 240x135! -depth 8 bgr:interim.raw

# 2. Use ffmpeg to force the exact RGB565BE (Big Endian) format
ffmpeg -v quiet -f rawvideo -pixel_format bgr24 -video_size 240x135 \
       -i interim.raw -f rawvideo -pix_fmt rgb565be data/colour/image.bin

# 3. Clean up
rm interim.raw
```

Convert XBM header file to raw bin
```bash
grep -o '0x[0-9a-fA-F]\{2\}' file_name.h | sed 's/0x//' | tr -d '\n' | xxd -r -p > data/monochrome/file_name.bin 
```