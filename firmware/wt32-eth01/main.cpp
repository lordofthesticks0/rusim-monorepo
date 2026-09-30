#include <Arduino.h>
#include <ETH.h>
#include <ESPping.h>

// WT32-ETH01 Ethernet test:
// - Brings up the LAN8720 over RMII (variant defaults: ADDR 1, PWR 16, MDC 23, MDIO 18)
// - Prints the DHCP-assigned IP address over serial
// - Pings 1.1.1.1 and reports the result over serial

static const IPAddress kPingTarget(1, 1, 1, 1);
static const uint8_t kPingCount = 3;
static const uint32_t kPingIntervalMs = 5000;

static volatile bool eth_connected = false;

void onEthEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("[ETH] Started");
      ETH.setHostname("wt32-eth01");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("[ETH] Link up");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.println("[ETH] Got IP");
      Serial.print("[ETH] IP address: ");
      Serial.println(ETH.localIP());
      Serial.print("[ETH] Gateway: ");
      Serial.println(ETH.gatewayIP());
      Serial.print("[ETH] Netmask: ");
      Serial.println(ETH.subnetMask());
      Serial.print("[ETH] MAC: ");
      Serial.println(ETH.macAddress());
      eth_connected = true;
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("[ETH] Link down");
      eth_connected = false;
      break;
    case ARDUINO_EVENT_ETH_STOP:
      Serial.println("[ETH] Stopped");
      eth_connected = false;
      break;
    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  // Wait briefly for USB-serial monitors to attach (non-blocking for headless use)
  delay(500);

  Serial.println();
  Serial.println("WT32-ETH01 Ethernet ping test");
  Serial.println("Target: 1.1.1.1");

  WiFi.onEvent(onEthEvent);

  // Explicit WT32-ETH01 RMII pins (same as variant defaults in pins_arduino.h):
  // PHY addr 1, power pin 16, MDC 23, MDIO 18, LAN8720, 50 MHz clock in on GPIO0.
  if (!ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER, ETH_PHY_MDC, ETH_PHY_MDIO,
                 ETH_PHY_TYPE, ETH_CLK_MODE)) {
    Serial.println("[ETH] ERROR: ETH.begin() failed");
  } else {
    Serial.println("[ETH] Waiting for link + DHCP...");
  }
}

void loop() {
  if (!eth_connected) {
    Serial.println("[ETH] Waiting for IP address (DHCP)...");
    delay(kPingIntervalMs);
    return;
  }

  Serial.print("[ETH] Assigned IP: ");
  Serial.println(ETH.localIP());

  Serial.print("[PING] Pinging 1.1.1.1 ... ");
  bool ok = Ping.ping(kPingTarget, kPingCount);
  if (ok) {
    Serial.print("OK, avg time = ");
    Serial.print(Ping.averageTime());
    Serial.println(" ms");
  } else {
    Serial.println("FAIL (no reply)");
  }

  delay(kPingIntervalMs);
}
