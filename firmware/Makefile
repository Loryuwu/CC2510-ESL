CC = sdcc
CFLAGS = --model-small --opt-code-speed
LDFLAGS = --out-fmt-ihx \
  --code-loc 0x0000 --code-size 0x3FFF \
  --xram-loc 0xF000 --xram-size 0x0F00 \
  --iram-size 0x100

SRCS_EPD = src/main.c src/epd.c
SRCS_RADIO_TX = src/main_radio_test.c src/radio.c src/delay.c
SRCS_RADIO_RX = src/main_radio_rx_test.c src/radio.c src/delay.c
SRCS_RADIO_LOOP = src/main_radio_loopback.c src/radio.c src/delay.c
# epd.c already provides delay_ms(), so delay.c must NOT be linked here.
SRCS_DISPLAY = src/main_display.c src/epd.c src/radio.c src/nfc.c
SRCS_NFC_SCAN = src/main_nfc_scan.c src/radio.c src/delay.c
SRCS_NFC_UID = src/main_nfc_uid.c src/radio.c src/delay.c
SRCS_NFC_TEST = src/main_nfc_test.c src/nfc.c src/radio.c src/delay.c
SRCS_NFC_DUMP = src/main_nfc_dump.c src/nfc.c src/radio.c src/delay.c
# epd.c provides delay_ms(), so delay.c must NOT be linked here.
SRCS_ENROLL = src/main_enroll.c src/epd.c src/nfc.c src/radio.c
# Zusatzmodule aus dem urspruenglichen Fork, in die Firmware verdrahtet.
# flash: epd.c liefert delay_ms(), daher wird delay.c hier NICHT gelinkt.
SRCS_FLASH_TEST = src/main_flash_test.c src/epd.c src/flash.c
SRCS_COBS_TEST  = src/main_cobs_test.c src/uart.c src/cobs.c

RELS_EPD = $(SRCS_EPD:.c=.rel)
RELS_RADIO_TX = $(SRCS_RADIO_TX:.c=.rel)
RELS_RADIO_RX = $(SRCS_RADIO_RX:.c=.rel)
RELS_RADIO_LOOP = $(SRCS_RADIO_LOOP:.c=.rel)
RELS_DISPLAY = $(SRCS_DISPLAY:.c=.rel)
RELS_NFC_SCAN = $(SRCS_NFC_SCAN:.c=.rel)
RELS_NFC_UID = $(SRCS_NFC_UID:.c=.rel)
RELS_NFC_TEST = $(SRCS_NFC_TEST:.c=.rel)
RELS_NFC_DUMP = $(SRCS_NFC_DUMP:.c=.rel)
RELS_ENROLL = $(SRCS_ENROLL:.c=.rel)
RELS_FLASH_TEST = $(SRCS_FLASH_TEST:.c=.rel)
RELS_COBS_TEST  = $(SRCS_COBS_TEST:.c=.rel)

all: firmware.bin

radio-tx: radio_test.bin

radio-rx: radio_rx_test.bin

radio-loopback: radio_loopback.bin

display: display.bin

display42: display42.bin

nfc-scan: nfc_scan.bin

nfc-uid: nfc_uid.bin

nfc-test: nfc_test.bin

nfc-dump: nfc_dump.bin

enroll: enroll.bin

flash-test: flash_test.bin

cobs-test: cobs_test.bin

%.rel: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

firmware.ihx: $(RELS_EPD)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(RELS_EPD)

radio_test.ihx: $(RELS_RADIO_TX)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(RELS_RADIO_TX)

radio_rx_test.ihx: $(RELS_RADIO_RX)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(RELS_RADIO_RX)

radio_loopback.ihx: $(RELS_RADIO_LOOP)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(RELS_RADIO_LOOP)

display.ihx: $(RELS_DISPLAY)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(RELS_DISPLAY)

# 4.2" panel variant: epd.c and main_display.c recompiled with -DPANEL_42
src/epd_42.rel: src/epd.c
	$(CC) -c $(CFLAGS) -DPANEL_42 -o $@ $<

src/main_display_42.rel: src/main_display.c
	$(CC) -c $(CFLAGS) -DPANEL_42 -o $@ $<

RELS_DISPLAY42 = src/main_display_42.rel src/epd_42.rel src/radio.rel src/nfc.rel

display42.ihx: $(RELS_DISPLAY42)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(RELS_DISPLAY42)

nfc_scan.ihx: $(RELS_NFC_SCAN)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(RELS_NFC_SCAN)

nfc_uid.ihx: $(RELS_NFC_UID)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(RELS_NFC_UID)

nfc_test.ihx: $(RELS_NFC_TEST)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(RELS_NFC_TEST)

nfc_dump.ihx: $(RELS_NFC_DUMP)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(RELS_NFC_DUMP)

enroll.ihx: $(RELS_ENROLL)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(RELS_ENROLL)

flash_test.ihx: $(RELS_FLASH_TEST)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(RELS_FLASH_TEST)

cobs_test.ihx: $(RELS_COBS_TEST)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(RELS_COBS_TEST)

%.bin: %.ihx
	python hex2bin.py $< $@

clean:
	rm -f src/*.rel src/*.asm src/*.lst src/*.sym src/*.rst
	rm -f *.ihx *.bin *.lk *.map *.mem

.PHONY: all radio-tx radio-rx radio-loopback display display42 nfc-scan nfc-uid nfc-test nfc-dump enroll flash-test cobs-test clean
