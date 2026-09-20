/*
 * Neapolis 2026 - Ponte ESP8266 Wi-Fi/TCP per ChibiOS
 *
 * Questo sketch implementa il piccolo sottoinsieme di comandi ESP-AT usato
 * dal firmware RT-STM32G474RE-SOS-SERVER. La Nucleo costruisce i pacchetti
 * MQTT; l'ESP8266 offre soltanto il collegamento Wi-Fi e un socket TCP
 * trasparente verso Mosquitto.
 *
 * UART ESP8266 <-> Nucleo: 38400 baud, 8-N-1.
 * Non vengono memorizzate credenziali nello sketch: SSID, password e indirizzo
 * del broker arrivano dalla Nucleo attraverso i comandi AT.
 */

#include <ESP8266WiFi.h>

namespace {

/* Deve corrispondere ad APP_UART_BAUD nella configurazione condivisa. */
constexpr uint32_t UART_BAUD = 38400U;
constexpr uint32_t WIFI_TIMEOUT_MS = 30000U;
constexpr uint32_t TCP_TIMEOUT_MS = 10000U;
constexpr uint32_t ESCAPE_GUARD_MS = 1000U;
constexpr size_t COMMAND_MAX = 256U;

WiFiClient tcpClient;

String commandLine;
bool commandEcho = true;
bool transparentModeRequested = false;
bool transparentMode = false;

uint32_t lastSerialDataMs = 0U;
uint32_t escapeLastPlusMs = 0U;
uint8_t pendingPlusCount = 0U;

void replyOk() {
  Serial.print(F("\r\nOK\r\n"));
}

void replyError() {
  Serial.print(F("\r\nERROR\r\n"));
}

void replyFail() {
  Serial.print(F("\r\nFAIL\r\n"));
}

/* Estrae una stringa racchiusa tra virgolette e aggiorna la posizione. */
bool readQuoted(const String &source, int &position, String &value) {
  while ((position < static_cast<int>(source.length())) &&
         ((source[position] == ' ') || (source[position] == ','))) {
    ++position;
  }

  if ((position >= static_cast<int>(source.length())) ||
      (source[position] != '\"')) {
    return false;
  }

  const int start = ++position;
  const int end = source.indexOf('\"', start);
  if (end < 0) {
    return false;
  }

  value = source.substring(start, end);
  position = end + 1;
  return true;
}

bool waitForWiFi() {
  const uint32_t start = millis();
  while ((WiFi.status() != WL_CONNECTED) &&
         ((millis() - start) < WIFI_TIMEOUT_MS)) {
    delay(50);
    yield();
  }
  return WiFi.status() == WL_CONNECTED;
}

bool openTcpConnection(const String &host, uint16_t port) {
  tcpClient.stop();
  tcpClient.setNoDelay(true);

  const uint32_t start = millis();
  while ((millis() - start) < TCP_TIMEOUT_MS) {
    if (tcpClient.connect(host.c_str(), port)) {
      return true;
    }
    delay(200);
    yield();
  }
  return false;
}

void closeTcpConnection() {
  transparentMode = false;
  transparentModeRequested = false;
  pendingPlusCount = 0U;
  tcpClient.stop();
}

void executeCommand(String command) {
  command.trim();

  if (commandEcho) {
    Serial.print(command);
    Serial.print(F("\r\n"));
  }

  if (command == F("AT")) {
    replyOk();
    return;
  }

  if (command == F("ATE0")) {
    commandEcho = false;
    replyOk();
    return;
  }

  if ((command == F("AT+CWMODE=1")) ||
      (command == F("AT+CWMODE_CUR=1"))) {
    WiFi.mode(WIFI_STA);
    replyOk();
    return;
  }

  if (command.startsWith(F("AT+CWJAP="))) {
    int position = command.indexOf('=') + 1;
    String ssid;
    String password;
    if (!readQuoted(command, position, ssid) ||
        !readQuoted(command, position, password)) {
      replyError();
      return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    WiFi.begin(ssid.c_str(), password.c_str());
    if (!waitForWiFi()) {
      replyFail();
      return;
    }

    Serial.print(F("\r\nWIFI CONNECTED\r\nWIFI GOT IP\r\n"));
    replyOk();
    return;
  }

  if (command == F("AT+CIPMUX=0")) {
    replyOk();
    return;
  }

  if (command == F("AT+CIPMODE=1")) {
    transparentModeRequested = true;
    replyOk();
    return;
  }

  if (command.startsWith(F("AT+CIPSTART="))) {
    int position = command.indexOf('=') + 1;
    String protocol;
    String host;
    if (!readQuoted(command, position, protocol) ||
        !readQuoted(command, position, host)) {
      replyError();
      return;
    }

    while ((position < static_cast<int>(command.length())) &&
           ((command[position] == ' ') || (command[position] == ','))) {
      ++position;
    }
    const long parsedPort = command.substring(position).toInt();
    if (!protocol.equalsIgnoreCase("TCP") || host.length() == 0U ||
        (parsedPort <= 0L) || (parsedPort > 65535L) ||
        (WiFi.status() != WL_CONNECTED)) {
      replyError();
      return;
    }

    if (!openTcpConnection(host, static_cast<uint16_t>(parsedPort))) {
      replyError();
      return;
    }

    Serial.print(F("\r\nCONNECT\r\n"));
    replyOk();
    return;
  }

  if (command == F("AT+CIPSEND")) {
    if (!transparentModeRequested || !tcpClient.connected()) {
      replyError();
      return;
    }

    /* Il carattere '>' e l'ultima risposta testuale prima del flusso MQTT. */
    Serial.print(F("\r\n>"));
    Serial.flush();
    transparentMode = true;
    pendingPlusCount = 0U;
    lastSerialDataMs = millis();
    return;
  }

  if (command == F("AT+CIPCLOSE")) {
    closeTcpConnection();
    Serial.print(F("\r\nCLOSED\r\n"));
    replyOk();
    return;
  }

  replyError();
}

void processCommandMode() {
  while (Serial.available() > 0) {
    const char byteRead = static_cast<char>(Serial.read());

    if (byteRead == '\n') {
      if (commandLine.length() > 0U) {
        executeCommand(commandLine);
        commandLine = "";
      }
      continue;
    }
    if (byteRead == '\r') {
      continue;
    }

    if (commandLine.length() < COMMAND_MAX) {
      commandLine += byteRead;

      /*
       * La Nucleo invia sempre "+++" prima del primo AT per recuperare un ESP
       * eventualmente rimasto in modalita trasparente dopo il proprio reset.
       * Se l'ESP e gia in modalita comandi, i tre caratteri non devono essere
       * concatenati al comando successivo formando "+++AT".
       */
      if (commandLine == F("+++")) {
        commandLine = "";
        Serial.print(F("\r\nOK\r\n"));
      }
    } else {
      commandLine = "";
      replyError();
    }
  }
}

void flushPendingPlusesToTcp() {
  while ((pendingPlusCount > 0U) && tcpClient.connected()) {
    tcpClient.write(static_cast<uint8_t>('+'));
    --pendingPlusCount;
  }
  pendingPlusCount = 0U;
}

void leaveTransparentMode() {
  transparentMode = false;
  transparentModeRequested = false;
  pendingPlusCount = 0U;
  commandLine = "";
  Serial.print(F("\r\nOK\r\n"));
}

void processTransparentSerial() {
  const uint32_t now = millis();

  while (Serial.available() > 0) {
    const uint8_t byteRead = static_cast<uint8_t>(Serial.read());
    const uint32_t receivedAt = millis();

    if (pendingPlusCount == 0U) {
      const bool guardBefore = (receivedAt - lastSerialDataMs) >= ESCAPE_GUARD_MS;
      if ((byteRead == static_cast<uint8_t>('+')) && guardBefore) {
        pendingPlusCount = 1U;
        escapeLastPlusMs = receivedAt;
        continue;
      }
    } else if ((byteRead == static_cast<uint8_t>('+')) &&
               (pendingPlusCount < 3U)) {
      ++pendingPlusCount;
      escapeLastPlusMs = receivedAt;
      continue;
    } else {
      flushPendingPlusesToTcp();
    }

    if (tcpClient.connected()) {
      tcpClient.write(byteRead);
    }
    lastSerialDataMs = receivedAt;
  }

  if ((pendingPlusCount == 3U) &&
      ((now - escapeLastPlusMs) >= ESCAPE_GUARD_MS)) {
    leaveTransparentMode();
  } else if ((pendingPlusCount > 0U) && (pendingPlusCount < 3U) &&
             ((now - escapeLastPlusMs) >= ESCAPE_GUARD_MS)) {
    /* Un '+' isolato appartiene al pacchetto MQTT e non e un escape. */
    flushPendingPlusesToTcp();
    lastSerialDataMs = now;
  }
}

void forwardTcpToSerial() {
  if (!transparentMode) {
    return;
  }

  while (tcpClient.available() > 0) {
    uint8_t buffer[128];
    const int availableBytes = tcpClient.available();
    const size_t wanted = static_cast<size_t>(availableBytes) < sizeof(buffer)
                              ? static_cast<size_t>(availableBytes)
                              : sizeof(buffer);
    const int received = tcpClient.read(buffer, wanted);
    if (received > 0) {
      Serial.write(buffer, static_cast<size_t>(received));
    }
  }
}

}  // namespace

void setup() {
  Serial.begin(UART_BAUD, SERIAL_8N1);
  Serial.setTimeout(50U);
  commandLine.reserve(COMMAND_MAX);

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  lastSerialDataMs = millis();
}

void loop() {
  if (transparentMode) {
    processTransparentSerial();
    forwardTcpToSerial();

    if (!tcpClient.connected() && (pendingPlusCount == 0U)) {
      transparentMode = false;
      transparentModeRequested = false;
      Serial.print(F("\r\nCLOSED\r\n"));
    }
  } else {
    processCommandMode();
  }

  yield();
}
