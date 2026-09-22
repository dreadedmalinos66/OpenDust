#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include <Wire.h>
#include <WiFi.h>
#include <ESP32Ping.h>
#include <WebServer.h>
#include "esp_system.h"
#include "esp_random.h"

#define SD_CS_PIN 5
WebServer server(80);

String inputBuffer = "";

TFT_eSPI tft = TFT_eSPI();

static void backlightOn() {
    #ifdef TFT_BL
    pinMode(TFT_BL, OUTPUT);
    #ifdef TFT_BACKLIGHT_ON
    digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
    #else
    digitalWrite(TFT_BL, HIGH);
    #endif
    #endif
}

void tft_init(){
    backlightOn();
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
}

void entropy(){
    uint32_t entropy = esp_random();
    Serial.printf("\nentropy: %u\n", entropy);
    //cryptographic keys
    uint8_t entropy_buffer[16];
    esp_fill_random(entropy_buffer, sizeof(entropy_buffer));
}

void sd_init(){
    if (!SD.begin(SD_CS_PIN)){
        Serial.println("sd mount failed");
        return;
    }
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE){
        Serial.println("no sd found");
        return;
    }
    Serial.println("sd mount succeed");
}

void i2c_init(){
    Serial.println("starting i2c");
    Wire.begin();
}

void disable_uart(){
    Serial.println("uart disabled");
    for (;;){}
}

void ping(const char* target){
    if (WiFi.status() != WL_CONNECTED){
        Serial.println("internet is not connected");
        return;
    }

    Serial.printf("pinging %s\n", target);

    int succeessfulPings = 0;
    for (int i = 0; i < 10; i++) {
        bool success = Ping.ping(target, 1);
        if (success){
            float avgTime = Ping.averageTime();
            Serial.printf("reply from %s: time=%.1fms\n", target, avgTime);
            succeessfulPings++;
        } else {
            Serial.println("request timed out");
        }
        delay(250);
    }
    Serial.printf("ping complete: %d/10 received\n", succeessfulPings);
}

bool readCredentials(fs::FS &fs, const char *path) {
    if (!fs.exists(path)) {
        Serial.println("wifi config file not found, read the manual");
        return false;
    }
    File file = fs.open(path, FILE_READ);
    if (!file) {
        Serial.println("cant open wifi config");
        return false;
    }

    String ssid = file.readStringUntil('\n');
    String pass = file.readStringUntil('\n');
    file.close();

    ssid.trim();
    pass.trim();

    if (ssid.length() == 0){
        Serial.println("ssid in wifi config is empty");
        return false;
    }

    Serial.printf("connecting to wifi %s\n", ssid.c_str());

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(120);
        Serial.print(".");
        attempts++;
    }
    if (WiFi.status() == WL_CONNECTED){
        Serial.println("\nconnected to wifi");
        Serial.print("ip is: ");
        Serial.println(WiFi.localIP());
        return true;
    } else {
        Serial.println("\nfailed to connect to wifi");
        return false;
    }
}

void net_init() {
    readCredentials(SD, "/private/boot/nethost");
}

//html local web site
void handleFileRead() {
    String path = server.uri();

    if (path.endsWith("/")) {
        path += "index.html";
    }

    if (SD.exists(path)) {
        File file = SD.open(path, FILE_READ);

        String contentType = "text/plain";
        if (path.endsWith(".html") || path.endsWith(".htm")) {
            contentType = "text/html";
        } else if (path.endsWith(".css")) {
            contentType = "text/css";
        } else if (path.endsWith(".js")) {
            contentType = "application/javascript";
        } else if (path.endsWith(".png")) {
            contentType = "image/png";
        } else if (path.endsWith(".jpg")) {
            contentType = "image/jpeg";
        }

        server.streamFile(file, contentType);
        file.close();
    } else {
        server.send(404, "text/plain", "404: file Not Found");
    }
}

void web_server_init() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("no network connected");
        return;
    }

    server.onNotFound(handleFileRead);
    server.begin();

    Serial.print("web server running at http://");
    Serial.println(WiFi.localIP());
}

void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("\n%s\n", dirname);
    File root = fs.open(dirname);
    if (!root){
        Serial.println("no directory");
        return;
    }
    if (!root.isDirectory()){
        Serial.println("not a directory");
        return;
    }
    File file = root.openNextFile();
    while (file) {
        Serial.print(file.name());
        Serial.print("   ");
        Serial.println(file.size());
        file = root.openNextFile();  }
}

void readFile(fs::FS &fs, const char * path) {
    if (!fs.exists(path)) {
        Serial.printf("file '%s' does not exist.\n", path);
        return;
    }

    File file = fs.open(path, FILE_READ);
    if (!file) {
        Serial.println("failed to open file.");
        return;
    }

    while (file.available()) {
        Serial.write(file.read());
    }
    Serial.println();
    file.close();
}




void processCommand(String command){
command.trim();

if (command.equalsIgnoreCase("list_commands")){
    Serial.println("\nlist_commands\ndisable_uart\necho text\nreset\nregen_entropy\nread_file /file/path\nls\nremount\nping web.site\nshow_ip\nmanual\nstart_web_server\nsysinfo\nrestart_network\nrestart_i2c\nrestart_tft\ndraw_tft\nclear_tft");
}

else if (command.equalsIgnoreCase("disable_uart")){
    disable_uart();
}

else if (command.startsWith("echo ")){
    String argument = command.substring(5);
    Serial.printf("\n%s\n", argument.c_str());
}

else if (command.equalsIgnoreCase("reset")){
    ESP.restart();
}

else if (command.equalsIgnoreCase("regen_entropy")){
    entropy();
}

else if (command.startsWith("read_file ")) {
    String filePath = command.substring(10);
    filePath.trim();

    if (!filePath.startsWith("/")) {
        filePath = "/" + filePath;
    }

    if (filePath.length() > 1) {
        readFile(SD, filePath.c_str());
    }
}

else if (command.startsWith("ls")) {
    String path = command.substring(3);
    path.trim();

    if (path.length() == 0) {
        path = "/";
    }

    listDir(SD, path.c_str(), 0);
}

else if (command.equalsIgnoreCase("remount")){
    sd_init();
}

else if (command.startsWith("ping ")){
    String target = command.substring(5);
    target.trim();
    if (target.length() > 0){
        ping(target.c_str());
    } else {
        Serial.println("read manual for documentation");
    }
}

else if (command.equalsIgnoreCase("show_ip")){
    Serial.println(WiFi.localIP());
}

else if (command.equalsIgnoreCase("manual")){
    Serial.println("sorry, not implemented yet\nyou can download manual / documentation in releases\n");
}

else if (command.equalsIgnoreCase("start_web_server")){
    web_server_init();
}

else if (command.equalsIgnoreCase("sysinfo")){
    Serial.printf("total heap: %u\n", ESP.getHeapSize());
    Serial.printf("free heap: %u\n", ESP.getFreeHeap());
    Serial.printf("min free: %u\n", ESP.getMinFreeHeap());
    Serial.printf("max alloc: %u\n", ESP.getMaxAllocHeap());
    // if (psramFound()) Serial.printf("free psram: %u\n", ESP.getFreePsram());
    // ^ in next update :P
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    Serial.print("chip model: ");
    Serial.println(chip_info.model);
    Serial.print("number of cores: ");
    Serial.println(chip_info.cores);
    Serial.print("total sd size: ");
    uint64_t totalSize = SD.cardSize();
    Serial.print(totalSize);
    Serial.print("\n");
}

else if (command.equalsIgnoreCase("restart_network")){
    net_init();
}

else if (command.equalsIgnoreCase("restart_i2c")){
    i2c_init();
}
// EXPERIMENTAL
else if (command.equalsIgnoreCase("restart_tft")){
    Serial.println("remember that for now only ILI9341 compatible displays are supported!");
    tft_init();
}

else if (command.equalsIgnoreCase("draw_tft")){
    Serial.println("expect manual cordinates input in later updates, for now they are hard coded");
    tft.fillRect(6 + 2, 24 + 2, 12, 5, TFT_GREEN);
    tft.fillRect(29, 120, 99, 5, TFT_GREEN);
}

else if (command.equalsIgnoreCase("clear_tft")){
    tft.fillScreen(TFT_BLACK);
}
// EXPERIMENTAL


else if (command.length() > 0) {
    Serial.println("unknown command.");
}

}

void setup() {
    Serial.begin(115200);
    Serial.println("\nstarting boot\n\nhttps://github.com/dreadedmalinos66/OpenDust");

    entropy();
    sd_init();
    i2c_init();
    net_init();
    // add here what function to launch automatically on boot
}

void loop() {
    while (Serial.available() > 0) {
        char c = Serial.read();

        if (c == '\r') {
            continue;
        }

        if (c == '\b' || c == 127) {
            if (inputBuffer.length() > 0) {
                inputBuffer.remove(inputBuffer.length() - 1);
                Serial.print("\b \b");
            }
        }
        else if (c == '\n') {
            Serial.println();
            if (inputBuffer.length() > 0) {
                processCommand(inputBuffer);
                inputBuffer = "";
            }
            Serial.print("# ");
        }
        else if (c >= 32 && c <= 126) {
            inputBuffer += c;
            Serial.print(c);
        }
    }
    server.handleClient();
}
