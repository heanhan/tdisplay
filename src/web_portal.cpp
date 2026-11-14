#include "web_portal.h"

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "esp_wifi.h"

#include "_preference.h"
#include "version.h"
#include "web_portal_assets.h"

// 页面与接口都挂在 WiFiManager 自己创建的 WebServer 上（AP、captive portal、DNS 都复用它），
// 因此只在配置门户真正开启时才有这些路由，正常休眠/运行期间不开放任何端口。
static WiFiManager* s_wm = NULL;

// 连接状态机：0 未开始 1 进行中 2 成功 3 失败
static int8_t s_conn_state = 0;
static String s_attempt = "";
static unsigned long s_conn_deadline = 0;
static const unsigned long CONNECT_TIMEOUT_MS = 25000;

static const int SCAN_MAX_NETS = 20;      // 页面最多显示的网络数
static const int SCAN_CANDIDATES = 48;    // 排序前的候选上限，须大于 SCAN_MAX_NETS

// NVS 单个字符串的上限约 4000 字节，留出余量，避免写入静默失败
static const unsigned int SCHEDULE_MAX_BYTES = 3500;

// ---------------- JSON 输出 ----------------

static String jsonEsc(const String& in) {
    String o;
    o.reserve(in.length() + 8);
    for (unsigned int i = 0; i < in.length(); i++) {
        char c = in[i];
        switch (c) {
        case '"':  o += "\\\""; break;
        case '\\': o += "\\\\"; break;
        case '\n': o += "\\n"; break;
        case '\r': o += "\\r"; break;
        case '\t': o += "\\t"; break;
        default:
            if ((unsigned char)c < 0x20 || (unsigned char)c == 0x7F) {
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                o += buf;
            } else {
                o += c; // UTF-8 原样输出即可，JSON 允许
            }
        }
    }
    return o;
}

class JsonOut {
public:
    JsonOut() { m_s.reserve(320); }
    void str(const char* k, const String& v) { sep(); m_s += '"'; m_s += k; m_s += "\":\""; m_s += jsonEsc(v); m_s += '"'; }
    void num(const char* k, long v) { sep(); m_s += '"'; m_s += k; m_s += "\":"; m_s += String(v); }
    void raw(const char* k, const String& v) { sep(); m_s += '"'; m_s += k; m_s += "\":"; m_s += v; }
    String done() { String o; o.reserve(m_s.length() + 2); o = '{'; o += m_s; o += '}'; return o; }

private:
    String m_s;
    bool m_first = true;
    void sep() { if (!m_first) m_s += ','; m_first = false; }
};

static void sendJson(const String& body) {
    s_wm->server->send(200, "application/json", body);
}

static void sendErr(const String& msg) {
    JsonOut j;
    j.num("ok", 0);
    j.str("err", msg);
    sendJson(j.done());
}

static void sendOk() {
    JsonOut j;
    j.num("ok", 1);
    sendJson(j.done());
}

// ---------------- 参数读写 ----------------

struct Params {
    String host, key, loc, type, week1st, cdd_label, cdd_date, tags, schedule;
};

static void paramsLoad(Params& p) {
    Preferences pref;
    pref.begin(PREF_NAMESPACE);
    p.host = pref.getString(PREF_QWEATHER_HOST, "");
    p.key = pref.getString(PREF_QWEATHER_KEY, "");
    p.loc = pref.getString(PREF_QWEATHER_LOC, "");
    p.type = pref.getString(PREF_QWEATHER_TYPE, "0");
    p.cdd_label = pref.getString(PREF_CD_DAY_LABLE, "");
    p.cdd_date = pref.getString(PREF_CD_DAY_DATE, "");
    p.tags = pref.getString(PREF_TAG_DAYS, "");
    p.week1st = pref.getString(PREF_SI_WEEK_1ST, "0");
    p.schedule = pref.getString(PREF_STUDY_SCHEDULE, "");
    pref.end();
}

// putString 成功时返回 strlen(value)，空串返回 0，与失败无法区分。
// 用读回比对判定，否则「清空某个字段再保存」会被误报成写入闪存失败。
static bool putParam(Preferences& pref, const char* key, const String& value) {
    pref.putString(key, value);
    return pref.getString(key, "\x01") == value;
}

static bool paramsSave(const Params& p) {
    Preferences pref;
    pref.begin(PREF_NAMESPACE);
    bool ok = true;
    ok &= putParam(pref, PREF_QWEATHER_HOST, p.host);
    ok &= putParam(pref, PREF_QWEATHER_KEY, p.key);
    ok &= putParam(pref, PREF_QWEATHER_LOC, p.loc);
    ok &= putParam(pref, PREF_QWEATHER_TYPE, p.type);
    ok &= putParam(pref, PREF_CD_DAY_LABLE, p.cdd_label);
    ok &= putParam(pref, PREF_CD_DAY_DATE, p.cdd_date);
    ok &= putParam(pref, PREF_TAG_DAYS, p.tags);
    ok &= putParam(pref, PREF_SI_WEEK_1ST, p.week1st);
    ok &= putParam(pref, PREF_STUDY_SCHEDULE, p.schedule);
    pref.end();
    return ok;
}

static String paramsJson(const Params& p) {
    JsonOut j;
    j.str("host", p.host);
    j.str("key", p.key);
    j.str("loc", p.loc);
    j.str("type", p.type);
    j.str("week1st", p.week1st);
    j.str("cdd_label", p.cdd_label);
    j.str("cdd_date", p.cdd_date);
    j.str("tags", p.tags);
    j.str("schedule", p.schedule);
    return j.done();
}

static bool onlyChars(const String& s, const char* allowed) {
    for (unsigned int i = 0; i < s.length(); i++) {
        if (s[i] == 0 || strchr(allowed, s[i]) == NULL) return false;
    }
    return true;
}

static bool onlyDigits(const String& s) {
    if (s.length() == 0) return false;
    return onlyChars(s, "0123456789");
}

// 按 UTF-8 码点计数，中文一个字算 1 个字符，和前端 maxlength 的口径一致
static int utf8Chars(const String& s) {
    int n = 0;
    for (unsigned int i = 0; i < s.length(); i++) {
        if ((s[i] & 0xC0) != 0x80) n++;
    }
    return n;
}

// 服务端独立校验：前端的校验只是体验，这里才是不写坏 NVS、不触发刷屏除零的底线
static bool paramsValidate(Params& p, String& err) {
    p.host.trim();
    p.key.trim();
    p.loc.trim();
    p.cdd_label.trim();
    p.cdd_date.trim();
    p.tags.trim();
    p.schedule.trim();
    p.type = (p.type == "1") ? "1" : "0";
    p.week1st = (p.week1st == "1") ? "1" : "0";

    const char* HOST_OK = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-";
    const char* KEY_OK = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";

    if (p.host.length() > 0) {
        if (p.host.length() > 64 || !onlyChars(p.host, HOST_OK) || p.host.indexOf('.') < 0) {
            err = "API Host 不是合法域名，例：xxxxxx.re.qweatherapi.com";
            return false;
        }
    }
    if (p.key.length() > 0) {
        if (p.key.length() > 32 || !onlyChars(p.key, KEY_OK)) {
            err = "API Key 只能包含字母、数字、下划线和连字符，最长 32 位";
            return false;
        }
    }
    if (p.key.length() > 0 && p.host.length() == 0) {
        err = "填了 API Key 就必须填 Host";
        return false;
    }
    if (p.loc.length() > 0) {
        if (p.loc.length() > 64 || !onlyChars(p.loc, "0123456789.,-")) {
            err = "位置 ID 只能是城市 ID（纯数字）或「经度,纬度」";
            return false;
        }
        if (p.loc.indexOf(',') >= 0) {
            int a = p.loc.indexOf(',');
            if (a < 1 || a != p.loc.lastIndexOf(',') || a == (int)p.loc.length() - 1) {
                err = "经纬度格式应为「116.41,39.92」";
                return false;
            }
        }
    }
    if (p.cdd_label.length() > 0 && utf8Chars(p.cdd_label) > 10) {
        err = "倒数日名称最多 10 个字符";
        return false;
    }
    if (p.cdd_date.length() > 0) {
        if (p.cdd_date.length() != 8 || !onlyDigits(p.cdd_date)) {
            err = "倒数日日期格式为 yyyyMMdd，例：20261231";
            return false;
        }
        int m = p.cdd_date.substring(4, 6).toInt();
        int d = p.cdd_date.substring(6, 8).toInt();
        if (m < 1 || m > 12 || d < 1 || d > 31) {
            err = "倒数日日期的月份或日期超出范围";
            return false;
        }
        if (p.cdd_label.length() == 0) {
            err = "倒数日需要同时填写名称和日期，只有日期不会显示";
            return false;
        }
    }

    if (p.tags.length() > 0) {
        if (p.tags.length() > 30) {
            err = "日期 Tag 总长不能超过 30 个字符";
            return false;
        }
        int count = 0;
        unsigned int from = 0;
        while (from <= p.tags.length()) {
            int semi = p.tags.indexOf(';', from);
            String item = (semi < 0) ? p.tags.substring(from) : p.tags.substring(from, semi);
            item.trim();
            // 允许尾随分号（用户常这么粘），但不能一项都没有
            if (item.length() > 0) {
                if (item.length() != 9 || !onlyDigits(item.substring(0, 8)) || item[8] < 'a' || item[8] > 'd') {
                    err = "日期 Tag「" + item + "」应为 8 位日期加图标字母 a-d";
                    return false;
                }
                if (++count > 3) {
                    err = "日期 Tag 最多 3 个";
                    return false;
                }
            }
            if (semi < 0) break;
            from = semi + 1;
        }
        if (count == 0) {
            err = "日期 Tag 至少要有 1 项，或者留空";
            return false;
        }
    }

    if (p.schedule.length() > 0) {
        if (p.schedule.length() > SCHEDULE_MAX_BYTES) {
            err = "课程表超过了 " + String(SCHEDULE_MAX_BYTES) + " 字节（中文一个字占 3 字节）";
            return false;
        }
        if (p.schedule.length() < 4 || !onlyDigits(p.schedule.substring(0, 3)) || p.schedule[3] != ';') {
            err = "课程表必须以三位课数加分号开头，例：430;";
            return false;
        }
        int counts = (p.schedule[0] - '0') + (p.schedule[1] - '0') + (p.schedule[2] - '0');
        // 三项全 0 时 screen_ink.cpp 的 drawStudySchedule 会除零并跳过渲染，直接拦住
        if (counts == 0) {
            err = "上午 / 下午 / 晚上的课数不能全为 0";
            return false;
        }
        int segs = 0;
        unsigned int from = 4;
        while (from < p.schedule.length()) {
            int semi = p.schedule.indexOf(';', from);
            String item = (semi < 0) ? p.schedule.substring(from) : p.schedule.substring(from, semi);
            item.trim();
            if (item.length() > 0) {
                if (item.indexOf(',') < 0) {
                    err = "第 " + String(segs + 1) + " 段只有星期、没有课程";
                    return false;
                }
                if (++segs > 7) {
                    err = "每日课程最多 7 段（一周七天）";
                    return false;
                }
            }
            if (semi < 0) break;
            from = semi + 1;
        }
        if (segs == 0) {
            err = "课程表至少要写一天的课程";
            return false;
        }
    }

    return true;
}

// ---------------- 状态查询 ----------------

// NVS 里存过的 STA 账号。WiFi.SSID() 在未连接时不一定可靠，直接读 esp_wifi 配置。
static String savedSsid() {
    wifi_config_t conf = {};
    if (esp_wifi_get_config(WIFI_IF_STA, &conf) != ESP_OK) return "";
    String ssid = String((char*)conf.sta.ssid);
    ssid.trim();
    return ssid;
}

static void hIndex() {
    s_wm->server->sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    s_wm->server->send_P(200, "text/html; charset=utf-8", PORTAL_PAGE, sizeof(PORTAL_PAGE) - 1);
}

static void hState() {
    bool connected = (WiFi.status() == WL_CONNECTED);
    if (s_conn_state == 1) {
        if (connected) {
            s_conn_state = 2;
        } else if ((long)(millis() - s_conn_deadline) >= 0) {
            s_conn_state = 3;
        } else if (WiFi.status() == WL_CONNECT_FAILED) {
            s_conn_state = 3;
        }
    }

    long left = 0;
    if (s_conn_state == 1) {
        left = (long)((s_conn_deadline - millis()) / 1000);
        if (left < 0) left = 0;
    }

    JsonOut j;
    j.str("ver", J_VERSION);
    j.str("ap", WiFi.softAPIP().toString());
    j.num("connected", connected ? 1 : 0);
    j.str("sta", connected ? WiFi.SSID() : "");
    j.str("ip", connected ? WiFi.localIP().toString() : "");
    j.num("rssi", connected ? WiFi.RSSI() : 0);
    j.str("saved", savedSsid());
    j.str("attempt", s_attempt);
    j.num("connecting", s_conn_state);
    j.num("timeout_left", left);
    j.num("heap", (long)ESP.getFreeHeap());
    sendJson(j.done());
}

static void hScanStart() {
    // 异步扫描：扫描期间 wmm.process() 照常跑，页面靠 /api/scan 轮询取结果
    WiFi.scanNetworks(true, false, false, 250, 0);
    sendOk();
}

static void hScanResult() {
    int16_t n = WiFi.scanComplete();
    JsonOut j;
    if (n == -1) { // 仍在扫描
        j.raw("scanning", "true");
        sendJson(j.done());
        return;
    }

    // 先收满候选再排序，最后才截断到 SCAN_MAX_NETS。
    // 若先截断再排序，信号强但排在扫描结果后面的网络会被弱信号挤掉。
    String names[SCAN_CANDIDATES];
    int32_t best[SCAN_CANDIDATES];
    bool opennet[SCAN_CANDIDATES];
    int cnt = 0;

    for (int i = 0; i < n; i++) {
        String ssid = WiFi.SSID(i);
        ssid.trim();
        if (ssid.length() == 0) continue; // 隐藏网络
        int32_t rssi = WiFi.RSSI(i);
        bool isOpen = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
        int at = -1;
        for (int k = 0; k < cnt; k++) {
            if (names[k] == ssid) { at = k; break; }
        }
        if (at < 0) {
            if (cnt >= SCAN_CANDIDATES) continue;
            at = cnt++;
            names[at] = ssid;
            best[at] = rssi;
            opennet[at] = isOpen;
        } else if (rssi > best[at]) {
            best[at] = rssi;
            if (!isOpen) opennet[at] = false; // 同名有多个热点时，按需要密码的那个报
        }
    }
    WiFi.scanDelete();

    for (int a = 0; a < cnt; a++) {
        for (int b = a + 1; b < cnt; b++) {
            if (best[b] <= best[a]) continue;
            int32_t tr = best[a]; best[a] = best[b]; best[b] = tr;
            String tn = names[a]; names[a] = names[b]; names[b] = tn;
            bool to = opennet[a]; opennet[a] = opennet[b]; opennet[b] = to;
        }
    }

    int show = (cnt < SCAN_MAX_NETS) ? cnt : SCAN_MAX_NETS;
    String arr = "[";
    for (int a = 0; a < show; a++) {
        if (a) arr += ',';
        arr += "{\"s\":\"";
        arr += jsonEsc(names[a]);
        arr += "\",\"r\":";
        arr += String(best[a]);
        arr += ",\"o\":";
        arr += String(opennet[a] ? 1 : 0);
        arr += '}';
    }
    arr += ']';

    j.raw("scanning", "false");
    j.num("count", show);
    j.raw("nets", arr);
    sendJson(j.done());
}

static void hConnect() {
    String body = s_wm->server->arg("plain");
    if (body.length() == 0) {
        sendErr("请求体为空");
        return;
    }
    JsonDocument doc;
    DeserializationError e = deserializeJson(doc, body);
    if (e) {
        sendErr("请求格式不是合法 JSON");
        return;
    }
    String ssid = doc["ssid"].is<const char*>() ? String(doc["ssid"].as<const char*>()) : String("");
    String pass = doc["pass"].is<const char*>() ? String(doc["pass"].as<const char*>()) : String("");
    ssid.trim();
    if (ssid.length() == 0) {
        sendErr("请填写网络名称");
        return;
    }
    if (ssid.length() > 32) {
        sendErr("网络名称最长 32 字节");
        return;
    }
    if (pass.length() > 64) {
        sendErr("密码最长 64 字节");
        return;
    }

    Serial.printf("[portal] connect to [%s]\r\n", ssid.c_str());
    s_attempt = ssid;
    s_conn_state = 1;
    s_conn_deadline = millis() + CONNECT_TIMEOUT_MS;
    WiFi.persistent(true); // 连上后写 NVS，下次开机由 wm.autoConnect() 自动重连
    WiFi.begin(ssid.c_str(), pass.length() > 0 ? pass.c_str() : NULL);
    sendOk();
}

static void hParamsGet() {
    Params p;
    paramsLoad(p);
    sendJson(paramsJson(p));
}

static void hParamsPost() {
    String body = s_wm->server->arg("plain");
    if (body.length() == 0) {
        sendErr("请求体为空");
        return;
    }
    JsonDocument doc;
    DeserializationError e = deserializeJson(doc, body);
    if (e) {
        sendErr("请求格式不是合法 JSON");
        return;
    }

    Params p;
    p.host = doc["host"].is<const char*>() ? String(doc["host"].as<const char*>()) : "";
    p.key = doc["key"].is<const char*>() ? String(doc["key"].as<const char*>()) : "";
    p.loc = doc["loc"].is<const char*>() ? String(doc["loc"].as<const char*>()) : "";
    p.type = doc["type"].is<const char*>() ? String(doc["type"].as<const char*>()) : "";
    p.week1st = doc["week1st"].is<const char*>() ? String(doc["week1st"].as<const char*>()) : "";
    p.cdd_label = doc["cdd_label"].is<const char*>() ? String(doc["cdd_label"].as<const char*>()) : "";
    p.cdd_date = doc["cdd_date"].is<const char*>() ? String(doc["cdd_date"].as<const char*>()) : "";
    p.tags = doc["tags"].is<const char*>() ? String(doc["tags"].as<const char*>()) : "";
    p.schedule = doc["schedule"].is<const char*>() ? String(doc["schedule"].as<const char*>()) : "";

    String err;
    if (!paramsValidate(p, err)) {
        sendErr(err);
        return;
    }
    if (!paramsSave(p)) {
        sendErr("写入闪存失败，请缩短课程表后重试");
        return;
    }
    Serial.println("[portal] params saved");

    JsonOut j;
    j.num("ok", 1);
    j.raw("values", paramsJson(p));
    sendJson(j.done());
}

static void hClearWifi() {
    // 只擦 STA 凭据，AP 必须留着，否则这个页面立刻断线
    WiFi.persistent(true);
    WiFi.disconnect(false, true);
    s_conn_state = 0;
    s_attempt = "";
    Serial.println("[portal] saved wifi erased");
    sendOk();
}

static void hReboot() {
    sendOk();
    Serial.println("[portal] reboot requested");
    delay(600); // 等响应发出去
    ESP.restart();
}

// ---------------- 注册 ----------------

static void registerRoutes() {
    if (s_wm == NULL || !s_wm->server) {
        Serial.println("[portal] no webserver, routes skipped");
        return;
    }
    WebServer& sv = *s_wm->server;
    // 比 WiFiManager 自己的 handleRoot 先注册，按先注册先匹配的规则，/ 由本模块接管
    sv.on("/", HTTP_GET, hIndex);
    sv.on("/api/state", HTTP_GET, hState);
    sv.on("/api/scan", HTTP_POST, hScanStart);
    sv.on("/api/scan", HTTP_GET, hScanResult);
    sv.on("/api/connect", HTTP_POST, hConnect);
    sv.on("/api/params", HTTP_GET, hParamsGet);
    sv.on("/api/params", HTTP_POST, hParamsPost);
    sv.on("/api/clear", HTTP_POST, hClearWifi);
    sv.on("/api/reboot", HTTP_POST, hReboot);
    Serial.println("[portal] routes registered");
}

void web_portal_install(WiFiManager* wm) {
    s_wm = wm;
    // 回调在 WebServer 创建之后、WiFiManager 注册内置路由之前触发
    wm->setWebServerCallback(registerRoutes);
    // 没被本模块接管的页面（/update、/info、/param）套用同一套配色
    wm->setCustomHeadElement(PORTAL_NATIVE_CSS);
}
