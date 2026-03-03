#pragma once

#include <Arduino.h>

// ─── HTTP content types matching LEXI-R10 AT manual ───
enum HttpContentType {
    CONTENT_URL_ENCODED = 0,  // application/x-www-form-urlencoded
    CONTENT_TEXT_PLAIN  = 1,  // text/plain
    CONTENT_OCTET      = 2,  // application/octet-stream
    CONTENT_MULTIPART  = 3,  // multipart/form-data
    CONTENT_JSON       = 4,  // application/json
    CONTENT_XML        = 5,  // application/xml
    CONTENT_USER_DEF   = 6   // user defined
};

// ─── HTTP result from +UUHTTPCR URC ───
struct HttpResult {
    bool    success;       // true if http_result == 1
    int     httpCommand;   // which command triggered this (1=GET, 5=POST data, etc.)
    String  body;          // response body read from file system
};

class LTEModem {
public:
    /// @param serial  Reference to the HardwareSerial already configured
    ///                e.g. Serial1.begin(115200, SERIAL_8N1, LTERX, LTETX);
    /// @param debugSerial  Optional debug output (pass Serial for USB monitor)
    LTEModem(HardwareSerial &serial, Stream *debugSerial = nullptr);

    // ──────────── Initialisation & Registration ────────────

    /// Basic AT handshake – returns true when modem responds "OK"
    bool begin(unsigned long timeoutMs = 10000);

    /// Wait for SIM to be ready (CPIN: READY)
    bool waitForSIM(unsigned long timeoutMs = 15000);

    /// Set MNO profile (operator config). Reboots modem if changed.
    /// Common values: 0=SW default, 1=SIM ICCID select, 100=Standard Europe
    bool setMNOProfile(int profile);

    /// Set the APN for the given CID (default cid=1)
    bool setAPN(const char *apn, int cid = 1);

    /// Attach to packet-switched network
    bool attachNetwork(unsigned long timeoutMs = 60000);

    /// Activate PDP context
    bool activatePDP(int cid = 1, unsigned long timeoutMs = 30000);

    /// Wait until registered on network (CEREG/CREG)
    bool waitForRegistration(unsigned long timeoutMs = 120000);

    /// Full convenience init: begin → SIM → APN → register → attach → PDP
    bool init(const char *apn, int mnoProfile = 0, unsigned long timeoutMs = 120000);

    /// Get signal quality (RSSI). Returns raw CSQ value (0-31, 99=unknown)
    int getSignalQuality();

    // ──────────── HTTP Methods ────────────

    /// HTTP GET  – returns response body in result.body
    HttpResult httpGet(const char *server, const char *path, uint16_t port = 80,
                       bool secure = false, unsigned long timeoutMs = 30000);

    /// HTTP POST data – posts the supplied data string
    HttpResult httpPost(const char *server, const char *path,
                        const char *data, HttpContentType contentType = CONTENT_JSON,
                        uint16_t port = 80, bool secure = false,
                        unsigned long timeoutMs = 30000);

    // ──────────── Low-level helpers (public for advanced use) ────────────

    /// Send an AT command and wait for expected response (default "OK")
    bool sendAT(const char *cmd, const char *expected = "OK",
                unsigned long timeoutMs = 5000);

    /// Send AT command, capture full response into `response`
    bool sendATResponse(const char *cmd, String &response,
                        unsigned long timeoutMs = 5000);

    /// Read a file from the modem file system (FOPEN/FREAD/FCLOSE)
    String readModemFile(const char *filename, unsigned long timeoutMs = 5000);

private:
    HardwareSerial &_serial;
    Stream         *_debug;

    static constexpr int HTTP_PROFILE = 0;  // LEXI-R10 only supports profile 0
    static constexpr const char *HTTP_RESP_FILE = "http_resp";

    void   _dbg(const char *msg);
    void   _dbg(const String &msg);
    String _readLine(unsigned long timeoutMs = 2000);
    bool   _waitForURC(const char *prefix, String &out, unsigned long timeoutMs);
    void   _flushInput();

    // Configure HTTP profile (server, port, secure)
    bool _httpSetup(const char *server, uint16_t port, bool secure);

    // Wait for +UUHTTPCR URC and read the response file
    HttpResult _httpWaitResult(int expectedCmd, unsigned long timeoutMs);
};