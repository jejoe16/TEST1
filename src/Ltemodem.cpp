#include "LTEModem.h"

// ═══════════════════════════════════════════════════════════
//  Constructor
// ═══════════════════════════════════════════════════════════

LTEModem::LTEModem(HardwareSerial &serial, Stream *debugSerial)
    : _serial(serial), _debug(debugSerial) {}

// ═══════════════════════════════════════════════════════════
//  Debug helpers
// ═══════════════════════════════════════════════════════════

void LTEModem::_dbg(const char *msg) {
    if (_debug) { _debug->println(msg); }
}

void LTEModem::_dbg(const String &msg) {
    if (_debug) { _debug->println(msg); }
}

// ═══════════════════════════════════════════════════════════
//  Serial helpers
// ═══════════════════════════════════════════════════════════

void LTEModem::_flushInput() {
    while (_serial.available()) { _serial.read(); }
}

String LTEModem::_readLine(unsigned long timeoutMs) {
    String line;
    unsigned long start = millis();
    while (millis() - start < timeoutMs) {
        if (_serial.available()) {
            char c = _serial.read();
            if (c == '\n') {
                line.trim();
                if (line.length() > 0) return line;
            } else {
                line += c;
            }
        }
    }
    line.trim();
    return line;
}

bool LTEModem::sendAT(const char *cmd, const char *expected, unsigned long timeoutMs) {
    _flushInput();
    _serial.print(cmd);
    _serial.print("\r\n");
    _dbg(String("> ") + cmd);

    unsigned long start = millis();
    while (millis() - start < timeoutMs) {
        String line = _readLine(timeoutMs - (millis() - start));
        if (line.length() == 0) continue;
        _dbg(String("< ") + line);
        if (line.indexOf(expected) >= 0) return true;
        if (line.indexOf("ERROR") >= 0) return false;
    }
    return false;
}

bool LTEModem::sendATResponse(const char *cmd, String &response, unsigned long timeoutMs) {
    _flushInput();
    response = "";
    _serial.print(cmd);
    _serial.print("\r\n");
    _dbg(String("> ") + cmd);

    unsigned long start = millis();
    while (millis() - start < timeoutMs) {
        String line = _readLine(timeoutMs - (millis() - start));
        if (line.length() == 0) continue;
        _dbg(String("< ") + line);
        if (line.indexOf("OK") >= 0) return true;
        if (line.indexOf("ERROR") >= 0) return false;
        response += line + "\n";
    }
    return false;
}

bool LTEModem::_waitForURC(const char *prefix, String &out, unsigned long timeoutMs) {
    unsigned long start = millis();
    while (millis() - start < timeoutMs) {
        String line = _readLine(timeoutMs - (millis() - start));
        if (line.startsWith(prefix)) {
            out = line;
            return true;
        }
    }
    return false;
}

// ═══════════════════════════════════════════════════════════
//  Initialisation & Registration
// ═══════════════════════════════════════════════════════════

bool LTEModem::begin(unsigned long timeoutMs) {
    unsigned long start = millis();

    // Disable echo first try
    sendAT("ATE0", "OK", 1000);

    // Keep trying basic AT until modem responds
    while (millis() - start < timeoutMs) {
        if (sendAT("AT", "OK", 1000)) {
            _dbg("Modem responsive");
            // Enable verbose error codes
            sendAT("AT+CMEE=2", "OK", 1000);
            return true;
        }
        delay(500);
    }
    _dbg("Modem not responding");
    return false;
}

bool LTEModem::waitForSIM(unsigned long timeoutMs) {
    unsigned long start = millis();
    while (millis() - start < timeoutMs) {
        String resp;
        if (sendATResponse("AT+CPIN?", resp, 2000)) {
            if (resp.indexOf("READY") >= 0) {
                _dbg("SIM ready");
                return true;
            }
        }
        delay(1000);
    }
    _dbg("SIM not ready");
    return false;
}

bool LTEModem::setMNOProfile(int profile) {
    // Read current profile
    String resp;
    if (sendATResponse("AT+UMNOPROF?", resp, 2000)) {
        // Check if already set
        String check = String("+UMNOPROF: ") + String(profile);
        if (resp.indexOf(check) >= 0) {
            _dbg("MNO profile already set");
            return true;
        }
    }

    // Must be in airplane mode to change MNO profile
    sendAT("AT+CFUN=0", "OK", 5000);
    delay(1000);

    String cmd = String("AT+UMNOPROF=") + String(profile);
    if (!sendAT(cmd.c_str(), "OK", 5000)) {
        _dbg("Failed to set MNO profile");
        return false;
    }

    // Reboot modem to apply
    sendAT("AT+CFUN=15", "OK", 5000);
    delay(5000);

    // Wait for modem to come back
    return begin(15000);
}

bool LTEModem::setAPN(const char *apn, int cid) {
    String cmd = String("AT+CGDCONT=") + String(cid) + ",\"IP\",\"" + apn + "\"";
    return sendAT(cmd.c_str(), "OK", 5000);
}

bool LTEModem::waitForRegistration(unsigned long timeoutMs) {
    // Enable registration URC: +CEREG: <stat>
    sendAT("AT+CEREG=1", "OK", 1000);
    sendAT("AT+CREG=1", "OK", 1000);

    unsigned long start = millis();
    while (millis() - start < timeoutMs) {
        String resp;
        // Check EPS registration (LTE)
        if (sendATResponse("AT+CEREG?", resp, 2000)) {
            // stat=1 (home) or stat=5 (roaming)
            if (resp.indexOf(",1") >= 0 || resp.indexOf(",5") >= 0) {
                _dbg("Registered on LTE network");
                return true;
            }
        }
        // Fallback: check CS registration
        if (sendATResponse("AT+CREG?", resp, 2000)) {
            if (resp.indexOf(",1") >= 0 || resp.indexOf(",5") >= 0) {
                _dbg("Registered on network (CS)");
                return true;
            }
        }
        delay(2000);
    }
    _dbg("Registration timeout");
    return false;
}

bool LTEModem::attachNetwork(unsigned long timeoutMs) {
    // Check if already attached
    String resp;
    if (sendATResponse("AT+CGATT?", resp, 5000)) {
        if (resp.indexOf("+CGATT: 1") >= 0) {
            _dbg("Already attached");
            return true;
        }
    }

    if (!sendAT("AT+CGATT=1", "OK", timeoutMs)) {
        _dbg("PS attach failed");
        return false;
    }
    _dbg("PS attached");
    return true;
}

bool LTEModem::activatePDP(int cid, unsigned long timeoutMs) {
    // Check if already active
    String resp;
    if (sendATResponse("AT+CGACT?", resp, 5000)) {
        String check = String("+CGACT: ") + String(cid) + ",1";
        if (resp.indexOf(check) >= 0) {
            _dbg("PDP context already active");
            return true;
        }
    }

    String cmd = String("AT+CGACT=1,") + String(cid);
    if (!sendAT(cmd.c_str(), "OK", timeoutMs)) {
        _dbg("PDP activation failed");
        return false;
    }
    _dbg("PDP context activated");
    return true;
}

int LTEModem::getSignalQuality() {
    String resp;
    if (sendATResponse("AT+CSQ", resp, 2000)) {
        int idx = resp.indexOf("+CSQ: ");
        if (idx >= 0) {
            int comma = resp.indexOf(',', idx);
            if (comma > idx) {
                return resp.substring(idx + 6, comma).toInt();
            }
        }
    }
    return -1;
}

bool LTEModem::init(const char *apn, int mnoProfile, unsigned long timeoutMs) {
    _dbg("=== LTE Modem Init ===");

    if (!begin(10000)) return false;


    if (!waitForSIM(15000)) return false;

    if (mnoProfile >= 0) {
        if (!setMNOProfile(mnoProfile)) return false;
    }

    // Ensure full functionality
    sendAT("AT+CFUN=1", "OK", 5000);
    delay(2000);

    if (!setAPN(apn)) return false;
    if (!waitForRegistration(timeoutMs)) return false;
    if (!attachNetwork(30000)) return false;
    if (!activatePDP(1, 30000)) return false;

    int rssi = getSignalQuality();
    _dbg(String("Signal quality (CSQ): ") + String(rssi));
    _dbg("=== Init Complete ===");
    return true;
}

// ═══════════════════════════════════════════════════════════
//  File system helpers (FOPEN / FREAD / FCLOSE)
// ═══════════════════════════════════════════════════════════

String LTEModem::readModemFile(const char *filename, unsigned long timeoutMs) {
    String result;

    // Get file size first
    String sizeCmd = String("AT+ULSTFILE=2,\"") + filename + "\"";
    String sizeResp;
    if (!sendATResponse(sizeCmd.c_str(), sizeResp, 3000)) {
        _dbg("Failed to get file size");
        return result;
    }
    int sizeIdx = sizeResp.indexOf("+ULSTFILE: ");
    if (sizeIdx < 0) return result;
    int fileSize = sizeResp.substring(sizeIdx + 11).toInt();
    if (fileSize == 0) return result;

    // Open file
    String openCmd = String("AT+FOPEN=\"") + filename + "\",0";
    String openResp;
    if (!sendATResponse(openCmd.c_str(), openResp, 3000)) {
        _dbg("Failed to open file");
        return result;
    }
    int handleIdx = openResp.indexOf("+FOPEN: ");
    if (handleIdx < 0) return result;
    int fileHandle = openResp.substring(handleIdx + 8).toInt();

    // Read file – FREAD enters direct link mode with CONNECT prefix
    String readCmd = String("AT+FREAD=") + String(fileHandle) + "," + String(fileSize);
    _flushInput();
    _serial.print(readCmd);
    _serial.print("\r\n");
    _dbg(String("> ") + readCmd);

    // Wait for CONNECT line
    unsigned long start = millis();
    bool connected = false;
    while (millis() - start < timeoutMs) {
        String line = _readLine(2000);
        if (line.startsWith("CONNECT")) {
            connected = true;
            break;
        }
        if (line.indexOf("ERROR") >= 0) break;
    }

    if (connected) {
        // Read raw bytes
        start = millis();
        int bytesRead = 0;
        while (bytesRead < fileSize && (millis() - start < timeoutMs)) {
            if (_serial.available()) {
                char c = _serial.read();
                result += c;
                bytesRead++;
            }
        }
        // Wait for OK after data
        _readLine(2000);
    }

    // Close file
    String closeCmd = String("AT+FCLOSE=") + String(fileHandle);
    sendAT(closeCmd.c_str(), "OK", 2000);

    // Delete the response file to free space
    String delCmd = String("AT+UDELFILE=\"") + filename + "\"";
    sendAT(delCmd.c_str(), "OK", 2000);

    return result;
}

// ═══════════════════════════════════════════════════════════
//  HTTP
// ═══════════════════════════════════════════════════════════

bool LTEModem::_httpSetup(const char *server, uint16_t port, bool secure) {
    // Reset HTTP profile 0
    String resetCmd = String("AT+UHTTP=") + String(HTTP_PROFILE);
    sendAT(resetCmd.c_str(), "OK", 2000);

    // Set server name  (op_code 1)
    String serverCmd = String("AT+UHTTP=") + String(HTTP_PROFILE) + ",1,\"" + server + "\"";
    if (!sendAT(serverCmd.c_str(), "OK", 2000)) {
        _dbg("Failed to set HTTP server");
        return false;
    }

    // Set port  (op_code 5)
    String portCmd = String("AT+UHTTP=") + String(HTTP_PROFILE) + ",5," + String(port);
    if (!sendAT(portCmd.c_str(), "OK", 2000)) {
        _dbg("Failed to set HTTP port");
        return false;
    }

    // Set secure mode  (op_code 6)
    int secVal = secure ? 1 : 0;
    String secCmd = String("AT+UHTTP=") + String(HTTP_PROFILE) + ",6," + String(secVal);
    if (!sendAT(secCmd.c_str(), "OK", 2000)) {
        _dbg("Failed to set HTTP secure");
        return false;
    }

    return true;
}

HttpResult LTEModem::_httpWaitResult(int expectedCmd, unsigned long timeoutMs) {
    HttpResult result;
    result.success = false;
    result.httpCommand = expectedCmd;

    // Wait for +UUHTTPCR: <profile_id>,<http_command>,<http_result>
    String urc;
    if (!_waitForURC("+UUHTTPCR:", urc, timeoutMs)) {
        _dbg("HTTP URC timeout");
        return result;
    }
    _dbg(String("HTTP URC: ") + urc);

    // Parse: +UUHTTPCR: 0,<cmd>,<result>
    int lastComma = urc.lastIndexOf(',');
    if (lastComma < 0) return result;
    int httpResult = urc.substring(lastComma + 1).toInt();

    if (httpResult != 1) {
        _dbg("HTTP command failed");
        // Query error details
        String errResp;
        sendATResponse("AT+UHTTPER=0", errResp, 2000);
        _dbg(String("HTTP error: ") + errResp);
        return result;
    }

    result.success = true;

    // Read the response file from modem FS
    result.body = readModemFile(HTTP_RESP_FILE, 10000);

    return result;
}

HttpResult LTEModem::httpGet(const char *server, const char *path,
                              uint16_t port, bool secure, unsigned long timeoutMs) {
    HttpResult fail;
    fail.success = false;
    fail.httpCommand = 1;

    if (!_httpSetup(server, port, secure)) return fail;

    // AT+UHTTPC=0,1,"<path>","<response_file>"
    // http_command 1 = GET
    String cmd = String("AT+UHTTPC=") + String(HTTP_PROFILE) +
                 ",1,\"" + path + "\",\"" + HTTP_RESP_FILE + "\"";

    if (!sendAT(cmd.c_str(), "OK", 5000)) {
        _dbg("HTTP GET command rejected");
        return fail;
    }

    return _httpWaitResult(1, timeoutMs);
}

HttpResult LTEModem::httpPost(const char *server, const char *path,
                               const char *data, HttpContentType contentType,
                               uint16_t port, bool secure, unsigned long timeoutMs) {
    HttpResult fail;
    fail.success = false;
    fail.httpCommand = 5;

    if (!_httpSetup(server, port, secure)) return fail;

    // AT+UHTTPC=0,5,"<path>","<response_file>","<data>",<content_type>
    // http_command 5 = POST data
    String cmd = String("AT+UHTTPC=") + String(HTTP_PROFILE) +
                 ",5,\"" + path + "\",\"" + HTTP_RESP_FILE +
                 "\",\"" + data + "\"," + String((int)contentType);

    if (!sendAT(cmd.c_str(), "OK", 5000)) {
        _dbg("HTTP POST command rejected");
        return fail;
    }

    return _httpWaitResult(5, timeoutMs);
}