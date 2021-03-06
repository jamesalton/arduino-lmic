/*******************************************************************************
 * Copyright (c) 2015 Thomas Telkamp and Matthijs Kooijman
 *
 * Permission is hereby granted, free of charge, to anyone
 * obtaining a copy of this document and accompanying files,
 * to do whatever they want with them without any restriction,
 * including, but not limited to, copying, modification and redistribution.
 * NO WARRANTY OF ANY KIND IS PROVIDED.
 *
 * This example sends a valid LoRaWAN packet with payload "Hello,
 * world!", using frequency and encryption settings matching those of
 * the The Things Network.
 *
 * This uses ABP (Activation-by-personalisation), where a DevAddr and
 * Session keys are preconfigured (unlike OTAA, where a DevEUI and
 * application key is configured, while the DevAddr and session keys are
 * assigned/generated in the over-the-air-activation procedure).
 *
 * Note: LoRaWAN per sub-band duty-cycle limitation is enforced (1% in
 * g1, 0.1% in g2), but not the TTN fair usage policy (which is probably
 * violated by this sketch when left running for longer)!
 *
 * To use this sketch, first register your application and device with
 * the things network, to set or generate a DevAddr, NwkSKey and
 * AppSKey. Each device should have their own unique values for these
 * fields.
 *
 * Do not forget to define the radio type correctly in config.h.
 *
 *******************************************************************************/

#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>

#define CFG_us915

// LoRaWAN NwkSKey, network session key
static const PROGMEM u1_t NWKSKEY[16] = { 0xD3, 0xBD, 0x66, 0x39, 0x1C, 0xBE, 0x00, 0xE0, 0xBF, 0xE9, 0xB4, 0x7E, 0x26, 0x84, 0x1E, 0xBD };

// LoRaWAN AppSKey, application session key
static const u1_t PROGMEM APPSKEY[16] = { 0x84, 0xE5, 0xCE, 0x42, 0xC0, 0xD7, 0x5B, 0x28, 0x2B, 0x34, 0xC6, 0xD7, 0xFD, 0xBA, 0x97, 0xAF };

// LoRaWAN end-device address (DevAddr)
static const u4_t DEVADDR = 0x26021CEC  ; // <-- Change this address for every node!

// These callbacks are only used in over-the-air activation, so they are
// left empty here (we cannot leave them out completely unless
// DISABLE_JOIN is set in config.h, otherwise the linker will complain).
void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }

static uint8_t mydata[] = "Hello, world!";
static osjob_t sendjob;

// Schedule TX every this many seconds (might become longer due to duty
// cycle limitations).
const unsigned TX_INTERVAL = 60;

// Pin mapping for LinkSprite
const lmic_pinmap lmic_pins = {
    .nss = 10, // slave select (SPI)
    .rxtx = LMIC_UNUSED_PIN, // antenna switch, typically unused
    .rst = 5, // reset
    .dio = {3, 2, 4}, // pins for DIO0 / DIO1 / DIO2
};
// Also needed pins: MOSI to MOSI (11), MISO to MISO (12), SCK to SCK (13) (standard Arduino SPI pins)

void onEvent (ev_t ev) {
    SerialUSB.print(os_getTime());
    SerialUSB.print(": ");
    switch(ev) {
        case EV_SCAN_TIMEOUT:
            SerialUSB.println(F("EV_SCAN_TIMEOUT"));
            break;
        case EV_BEACON_FOUND:
            SerialUSB.println(F("EV_BEACON_FOUND"));
            break;
        case EV_BEACON_MISSED:
            SerialUSB.println(F("EV_BEACON_MISSED"));
            break;
        case EV_BEACON_TRACKED:
            SerialUSB.println(F("EV_BEACON_TRACKED"));
            break;
        case EV_JOINING:
            SerialUSB.println(F("EV_JOINING"));
            break;
        case EV_JOINED:
            SerialUSB.println(F("EV_JOINED"));
            break;
        case EV_RFU1:
            SerialUSB.println(F("EV_RFU1"));
            break;
        case EV_JOIN_FAILED:
            SerialUSB.println(F("EV_JOIN_FAILED"));
            break;
        case EV_REJOIN_FAILED:
            SerialUSB.println(F("EV_REJOIN_FAILED"));
            break;
        case EV_TXCOMPLETE:
            SerialUSB.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
            if (LMIC.txrxFlags & TXRX_ACK)
              SerialUSB.println(F("Received ack"));
            if (LMIC.dataLen) {
              SerialUSB.println(F("Received "));
              SerialUSB.println(LMIC.dataLen);
              SerialUSB.println(F(" bytes of payload"));
            }
            // Schedule next transmission
            os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL), do_send);
            break;
        case EV_LOST_TSYNC:
            SerialUSB.println(F("EV_LOST_TSYNC"));
            break;
        case EV_RESET:
            SerialUSB.println(F("EV_RESET"));
            break;
        case EV_RXCOMPLETE:
            // data received in ping slot
            SerialUSB.println(F("EV_RXCOMPLETE"));
            break;
        case EV_LINK_DEAD:
            SerialUSB.println(F("EV_LINK_DEAD"));
            break;
        case EV_LINK_ALIVE:
            SerialUSB.println(F("EV_LINK_ALIVE"));
            break;
         default:
            SerialUSB.println(F("Unknown event"));
            break;
    }
}

void do_send(osjob_t* j){
    // Check if there is not a current TX/RX job running
    if (LMIC.opmode & OP_TXRXPEND) {
        SerialUSB.println(F("OP_TXRXPEND, not sending"));
    } else {
        // Prepare upstream data transmission at the next possible time.
        LMIC_setTxData2(1, mydata, sizeof(mydata)-1, 0);
        SerialUSB.println(F("Packet queued"));
    }
    // Next TX is scheduled after TX_COMPLETE event.
}

void setup() {
    SerialUSB.begin(115200);
    while(!SerialUSB);
    SerialUSB.println(F("Starting"));
    
    // LMIC init
    os_init();
    // Reset the MAC state. Session and pending data transfers will be discarded.
    LMIC_reset();

    // Set static session parameters. Instead of dynamically establishing a session
    // by joining the network, precomputed session parameters are be provided.
    #ifdef PROGMEM
    // On AVR, these values are stored in flash and only copied to RAM
    // once. Copy them to a temporary buffer here, LMIC_setSession will
    // copy them into a buffer of its own again.
    uint8_t appskey[sizeof(APPSKEY)];
    uint8_t nwkskey[sizeof(NWKSKEY)];
    memcpy_P(appskey, APPSKEY, sizeof(APPSKEY));
    memcpy_P(nwkskey, NWKSKEY, sizeof(NWKSKEY));
    LMIC_setSession (0x1, DEVADDR, nwkskey, appskey);
    #else
    // If not running an AVR with PROGMEM, just use the arrays directly
    LMIC_setSession (0x1, DEVADDR, NWKSKEY, APPSKEY);
    #endif

//    LMIC_setupChannel(0, 903900000, DR_RANGE_MAP(DR_SF10, DR_SF7),  BAND_CENTI);      // g-band
//    LMIC_setupChannel(1, 904100000, DR_RANGE_MAP(DR_SF10, DR_SF7),  BAND_CENTI);      // g-band
//    LMIC_setupChannel(2, 904300000, DR_RANGE_MAP(DR_SF10, DR_SF7),  BAND_CENTI);      // g-band
//    LMIC_setupChannel(3, 904500000, DR_RANGE_MAP(DR_SF10, DR_SF7),  BAND_CENTI);      // g-band
//    LMIC_setupChannel(4, 904700000, DR_RANGE_MAP(DR_SF10, DR_SF7),  BAND_CENTI);      // g-band
//    LMIC_setupChannel(5, 904900000, DR_RANGE_MAP(DR_SF10, DR_SF7),  BAND_CENTI);      // g-band
//    LMIC_setupChannel(6, 905100000, DR_RANGE_MAP(DR_SF10, DR_SF7),  BAND_CENTI);      // g-band
//    LMIC_setupChannel(7, 905300000, DR_RANGE_MAP(DR_SF10, DR_SF7),  BAND_CENTI);      // g-band
//    //LMIC_setupChannel(8, 904600000, DR_RANGE_MAP(DR_SF8,  DR_SF8),  ???);      // g-band

//    for (int channel=8; channel<72; channel++) {
//       LMIC_disableChannel(channel);
//    }    
    
    LMIC_selectSubBand(1);
    
    // Enable/disable link check validation.
    // LMIC sets the ADRACKREQ bit in UP frames if there were no DN frames
    // for a while. It expects the network to provide a DN message to prove
    // connectivity with a span of UP frames. If this no such prove is coming
    // then the datarate is lowered and a LINK_DEAD event is generated.
    // This mode can be disabled and no connectivity prove (ADRACKREQ) is requested
    // nor is the datarate changed.
    // This must be called only if a session is established (e.g. after EV_JOINED)
    
    // Disable link check validation
    LMIC_setLinkCheckMode(0);
    
    // set ADR mode (if mobile turn off) - Adaptive Data Rate
    LMIC_setAdrMode(0);

    // Set data rate and transmit power for uplink (note: txpow seems to be ignored by the library)
    LMIC_setDrTxpow(DR_SF10,14);

    // Start job
    do_send(&sendjob);
}

void loop() {
    os_runloop_once();
}
