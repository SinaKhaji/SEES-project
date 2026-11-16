#include <Adafruit_CircuitPlayground.h>
#include <bluefruit.h>
#include <time.h>

#define BUFFER_SIZE 10000

BLEUart bleuart; 
BLEBas  blebas;

String name;

struct Sample { uint32_t time_ms; float x, y, z; };

static Sample buffer[BUFFER_SIZE];
static int    count    = 0;
static int    overflow = 0;

void setup() {
  name = String(getMcuUniqueID())+"_ACC";

  CircuitPlayground.begin();
  Serial.begin(115200);
  delay(5000);

  Bluefruit.autoConnLed(true);
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  Bluefruit.begin();
  Bluefruit.setTxPower(4);   
  Bluefruit.setName(name.c_str());
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  bleuart.begin();
  blebas.begin();

  startAdv();
}

void startAdv(void)
{
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();

  // Include bleuart 128-bit uuid
  Bluefruit.Advertising.addService(bleuart);

  // Secondary Scan Response packet (optional)
  // Since there is no room for 'Name' in Advertising packet
  Bluefruit.ScanResponse.addName();
  
  /* Start Advertising
   * - Enable auto advertising if disconnected
   * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
   * - Timeout for fast mode is 30 seconds
   * - Start(timeout) with timeout = 0 will advertise forever (until connected)
   * 
   * For recommended advertising interval
   * https://developer.apple.com/library/content/qa/qa1931/_index.html   
   */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds  

  Serial.print("Service UUID: ");
  printInHex(BLEUART_UUID_SERVICE);

  Serial.print("Characteristic TX UUID: ");
  printInHex(BLEUART_UUID_CHR_TXD);

  Serial.print("Characteristic RX UUID: ");
  printInHex(BLEUART_UUID_CHR_RXD);

  Serial.printf("Device name: %s \n", name.c_str());
}

void printInHex(const uint8_t* uuid) {
  char str[37];
  sprintf(str,
    "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
      uuid[15], uuid[14], uuid[13], uuid[12],
      uuid[11], uuid[10],
      uuid[9],  uuid[8],
      uuid[7],  uuid[6],
      uuid[5],  uuid[4], uuid[3], uuid[2], uuid[1], uuid[0]
  );
  Serial.println(str);
}

void buffer_push(float x, float y, float z) {
    if (count >= BUFFER_SIZE) {
        overflow = 1;
        Serial.print("Overflow!!!!!\n");
        return;
    }
    buffer[count++] = (Sample){ millis(), x, y, z };
}

void flush_and_send(void) {
    if (count == 0) return;
  
    // Send all at once (up to MTU limit ~512 bytes with negotiated MTU)
    bleuart.write((uint8_t*)buffer, count * sizeof(Sample));
  
    Serial.printf("%s's buffer sent! (%d samples)\n", name.c_str(), count);
    count    = 0;
    overflow = 0;
}

void loop() {
  float x = CircuitPlayground.motionX();
  float y = CircuitPlayground.motionY();
  float z = CircuitPlayground.motionZ();

  buffer_push(x, y, z);

  if (Bluefruit.connected()) {
    flush_and_send();
  }

  delay(200);
}

// callback invoked when central connects
void connect_callback(uint16_t conn_handle)
{
  // Get the reference to current connection
  BLEConnection* connection = Bluefruit.Connection(conn_handle);

  char central_name[32] = { 0 };
  connection->getPeerName(central_name, sizeof(central_name));

  Serial.print("Connected to ");
  Serial.println(central_name);
}

/**
 * Callback invoked when a connection is dropped
 * @param conn_handle connection where this event happens
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;

  Serial.println();
  Serial.print("Disconnected, reason = 0x"); Serial.println(reason, HEX);
}