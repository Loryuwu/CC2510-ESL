/*
 * PriceTag Gateway - nRF24L01+ TX Test
 *
 * Sends test packets that the CC2510 can receive.
 * Use this to test ESP32->CC2510 direction.
 *
 * Same wiring as gateway_rx_test.
 *
 * Address: 5 bytes. The nRF24 transmits addresses REVERSED on air (MSByte
 * = array[last] first), so the array must be the reverse of the desired
 * on-air order. Desired on-air: AA AA A7 5C A7 - the AA AA act as extended
 * preamble for the CC2510 demodulator (nRF24 sends only 1 preamble byte,
 * CC2510 needs ~3-4), then sync word A7 5C, then A7 as first data byte.
 * => array = {A7, 5C, A7, AA, AA}. (Verified via loopback 2026-10-16.)
 */

#include <SPI.h>
#include <RF24.h>

#define CE_PIN   1
#define CSN_PIN  2

#define NRF_CHANNEL    76
#define NRF_DATARATE   RF24_250KBPS
#define NRF_ADDR_WIDTH 5
#define NRF_PAYLOAD_SIZE 32

/* On-air = REVERSE of array. Array {A7,5C,A7,AA,AA} -> on-air AA AA A7 5C A7:
 * AA AA = extra preamble BEFORE the CC2510 sync word (A7 5C), then A7 as
 * first data byte. */
const uint8_t txAddr[NRF_ADDR_WIDTH] = {0xA7, 0x5C, 0xA7, 0xAA, 0xAA};

RF24 radio(CE_PIN, CSN_PIN);

uint16_t seq = 0;

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);

    Serial.println();
    Serial.println("=== PriceTag Gateway TX Test (5-byte addr) ===");

    SPI.begin(3, 9, 7, CSN_PIN);  /* SCK, MISO, MOSI, SS */

    if (!radio.begin(&SPI)) {
        Serial.println("ERROR: nRF24 not found! Check wiring.");
        while (1) delay(1000);
    }

    radio.setChannel(NRF_CHANNEL);
    radio.setDataRate(NRF_DATARATE);
    radio.setPALevel(RF24_PA_MAX);
    radio.setAddressWidth(NRF_ADDR_WIDTH);
    radio.setPayloadSize(NRF_PAYLOAD_SIZE);

    radio.setAutoAck(false);
    radio.disableCRC();
    radio.setRetries(0, 0);
    radio.disableDynamicPayloads();

    radio.openWritingPipe(txAddr);
    radio.stopListening();

    Serial.printf("Channel: %d, Addr (on-air): AA:AA:A7:5C:A7, PA: MAX\n",
                  NRF_CHANNEL);
    Serial.println("Sending test packets every 2 seconds...\n");
}

void loop() {
    uint8_t payload[NRF_PAYLOAD_SIZE];
    memset(payload, 0, sizeof(payload));

    /* Build protocol packet: [ADDR][LEN][data...][XOR] */
    uint8_t tag_addr = 0x01;    /* target tag address */
    uint8_t data[] = {'H', 'E', 'L', 'L', 'O',
                      (uint8_t)(seq >> 8), (uint8_t)(seq & 0xFF)};
    uint8_t data_len = sizeof(data);

    payload[0] = tag_addr;
    payload[1] = data_len;
    memcpy(&payload[2], data, data_len);

    /* XOR checksum over [ADDR][LEN][data...] */
    uint8_t xor_val = 0;
    for (uint8_t i = 0; i < data_len + 2; i++)
        xor_val ^= payload[i];
    payload[data_len + 2] = xor_val;

    bool ok = radio.write(payload, sizeof(payload));

    Serial.printf("TX seq=%u addr=0x%02X len=%u %s\n",
                  seq, tag_addr, data_len, ok ? "OK" : "FAIL");

    seq++;
    delay(25);   /* send often so the CC2510 catches bursts quickly */
}
