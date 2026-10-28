#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>

// SMART EVACUATION MANAGER - ARDUINO UNO
// Demo mode works without an ADS1115 module.

// ---------------- PIN CONFIGURATION ----------------

const int PIN_T[3] = {A0, A1, A2};
const int PIN_G[3] = {A3, -1, -1};

const int LED_R[4] = {2, 4, 6, 8};
const int LED_G[4] = {3, 5, 7, 9};

const int PIN_BUZZ = 10;
const int PIN_BTN_START = 11;
const int PIN_BTN_BUG = 12;

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int NN = 5;
const int NROOM = 3;

const char* NAME[NN] = {
  "Room A", "Room B", "Room C", "EXIT 1", "EXIT 2"
};

const char* CODE[NN] = {"A", "B", "C", "E1", "E2"};

struct Edge {
  uint8_t a, b;
  float d;
};

const Edge EDGES[] = {
  {0, 1, 10}, {1, 2, 8}, {0, 2, 15},
  {1, 3, 16}, {2, 4, 12}, {1, 4, 24}, {0, 3, 40}
};

const int NE = sizeof(EDGES) / sizeof(EDGES[0]);

const float ALERT_HAZARD = 40;
const float FAULT_HAZARD = 75;
const float RISK_GAIN = 9;
const float NTC_BETA = 3950;

float temp[NROOM], aqi[NROOM], hz[NN], W[NN][NN];
bool sensFault[NROOM];

bool bugMode = false, quarantine = false, failsafe = false;
int startRoom = 0, consecutiveBugs = 0, healCount = 0;

unsigned long fixUntil = 0, lastCycle = 0, lastPrint = 0;
unsigned long lastBtn = 0, pressStart = 0, scenStart = 0;

uint32_t blockedMask = 0, prevBlockedMask = 0xFFFFFFFFUL;

int routeLedMode = 0;
int alarmMode = 0;
float routeRisk = 0;

bool prevS = HIGH, prevB = HIGH, forceCycle = true;
bool demoMode = true;
int scen = -1;

const bool BUZZER_ENABLED = true;

float baseG[NROOM] = {2000, 2000, 2000};
int rawT[NROOM], rawG[NROOM];

struct Scenario {
  const char* title;
  float t[3];
  float a[3];
  bool bug;
};

const Scenario SC[] = {
  {"All clear",                {26,26,26}, {40,40,40}, false},
  {"Room B heating up",        {26,52,26}, {40,40,40}, false},
  {"Room C smoky",             {26,26,26}, {40,40,250}, false},
  {"Room B on FIRE (INF)+BUG", {26,65,26}, {40,40,40}, true},
  {"B and C blocked + BUG",    {26,65,62}, {40,40,40}, true},
  {"Room A trapped, no exit",  {70,26,26}, {40,40,40}, false},
  {"Fire out, recovering",     {30,38,30}, {40,90,40}, false}
};

const int NSC = sizeof(SC) / sizeof(SC[0]);
const unsigned long SCEN_MS = 6000;

struct Route {
  int path[NN];
  int len;
  float cost;
  bool ok;
};

Route cur;

// ---------------- SENSOR READING ----------------

int readAvg(int pin) {
  long sum = 0;

  for (int i = 0; i < 8; i++) {
    sum += analogRead(pin);
  }

  return sum / 8;
}

// No ADS1115 is installed.
// Only Gas A is connected to an available analog pin.
// Demo readings for all rooms come from SC[].
int readGas(int room) {
  if (room == 0) {
    return readAvg(A3);
  }

  return 0;
}

float score(float v, float lo, float hi) {
  return constrain(
    (v - lo) / (hi - lo), 0.0f, 1.0f
  ) * 100.0f;
}

float aqiFromRaw(int raw, float base) {
  if (base < 1) base = 1;

  if (raw <= base) {
    return 40.0f * raw / base;
  }

  return 40.0f +
         (raw - base) * 460.0f / (1023.0f - base);
}

void readSensors() {
  for (int r = 0; r < NROOM; r++) {
    rawT[r] = readAvg(PIN_T[r]);
    rawG[r] = readGas(r);

    if (demoMode) {
      temp[r] = SC[scen].t[r];
      aqi[r] = SC[scen].a[r];
      sensFault[r] = false;
    } else {
      sensFault[r] = (rawT[r] <= 2 || rawT[r] >= 1021);

      float a = constrain(rawT[r], 1, 1022);

      temp[r] = 1.0f /
        (log(1023.0f / a - 1.0f) / NTC_BETA
         + 1.0f / 298.15f) - 273.15f;

      aqi[r] = aqiFromRaw(rawG[r], baseG[r]);
    }

    float h = max(
      score(temp[r], 35, 60),
      score(aqi[r], 100, 300)
    );

    if (sensFault[r]) {
      h = max(h, FAULT_HAZARD);
    }

    hz[r] = h;
  }

  hz[3] = hz[4] = 0;
}

void demoTick() {
  if (!demoMode) return;

  if (scen < 0 || millis() - scenStart >= SCEN_MS) {
    scen = (scen + 1) % NSC;
    scenStart = millis();

    bugMode = SC[scen].bug;

    if (!bugMode) {
      quarantine = false;
      consecutiveBugs = 0;
    }

    prevBlockedMask = 0xFFFFFFFFUL;

    Serial.print(F("\n=== DEMO SCENARIO "));
    Serial.print(scen + 1);
    Serial.print('/');
    Serial.print(NSC);
    Serial.print(F(": "));
    Serial.print(SC[scen].title);
    Serial.println(F(" ==="));
  }
}

// ---------------- GRAPH ----------------

bool edgeExists(int a, int b) {
  for (int i = 0; i < NE; i++) {
    if ((EDGES[i].a == a && EDGES[i].b == b) ||
        (EDGES[i].a == b && EDGES[i].b == a)) {
      return true;
    }
  }

  return false;
}

void buildGraph() {
  for (int i = 0; i < NN; i++) {
    for (int j = 0; j < NN; j++) {
      W[i][j] = INFINITY;
    }
  }

  blockedMask = 0;

  for (int i = 0; i < NE; i++) {
    float risk = max(hz[EDGES[i].a], hz[EDGES[i].b]);

    float w = risk >= 100
      ? INFINITY
      : EDGES[i].d * (1.0f + RISK_GAIN * risk / 100.0f);

    W[EDGES[i].a][EDGES[i].b] = w;
    W[EDGES[i].b][EDGES[i].a] = w;

    if (isinf(w)) {
      blockedMask |= (1UL << i);
    }
  }
}

// ---------------- ROUTING ----------------

Route makeRoute(int src, int* prev, float* dist) {
  Route r;
  r.ok = false;
  r.len = 0;
  r.cost = INFINITY;

  int best = -1;

  for (int e = NROOM; e < NN; e++) {
    if (!isinf(dist[e]) &&
        (best == -1 || dist[e] < dist[best])) {
      best = e;
    }
  }

  if (best == -1) return r;

  int tmp[NN], n = 0;

  for (int v = best; v != -1 && n < NN; v = prev[v]) {
    tmp[n++] = v;
  }

  if (n == 0 || tmp[n - 1] != src) return r;

  for (int i = 0; i < n; i++) {
    r.path[i] = tmp[n - 1 - i];
  }

  r.len = n;
  r.cost = dist[best];
  r.ok = true;

  return r;
}

Route dijkstraRoute(int src) {
  float dist[NN];
  int prev[NN];
  bool done[NN];

  for (int i = 0; i < NN; i++) {
    dist[i] = INFINITY;
    prev[i] = -1;
    done[i] = false;
  }

  dist[src] = 0;

  for (int it = 0; it < NN; it++) {
    int u = -1;

    for (int i = 0; i < NN; i++) {
      if (!done[i] &&
          (u == -1 || dist[i] < dist[u])) {
        u = i;
      }
    }

    if (u == -1 || isinf(dist[u])) break;

    done[u] = true;

    for (int v = 0; v < NN; v++) {
      if (!edgeExists(u, v)) continue;

      float w = W[u][v];

      if (bugMode && isinf(w)) {
        w = 5.0f;
      }

      if (isinf(w)) continue;

      if (dist[u] + w < dist[v]) {
        dist[v] = dist[u] + w;
        prev[v] = u;
      }
    }
  }

  return makeRoute(src, prev, dist);
}

Route bellmanFord(int src) {
  float dist[NN];
  int prev[NN];

  for (int i = 0; i < NN; i++) {
    dist[i] = INFINITY;
    prev[i] = -1;
  }

  dist[src] = 0;

  for (int k = 0; k < NN - 1; k++) {
    for (int i = 0; i < NE; i++) {
      int a = EDGES[i].a;
      int b = EDGES[i].b;
      float w = W[a][b];

      if (isinf(w)) continue;

      if (!isinf(dist[a]) && dist[a] + w < dist[b]) {
        dist[b] = dist[a] + w;
        prev[b] = a;
      }

      if (!isinf(dist[b]) && dist[b] + w < dist[a]) {
        dist[a] = dist[b] + w;
        prev[a] = b;
      }
    }
  }

  return makeRoute(src, prev, dist);
}

// ---------------- GUARDIAN AGENT ----------------

const char* verifyRoute(const Route& r, int src, const Route& ref) {
  if (!r.ok) {
    return ref.ok ? "safe path exists but router failed" : nullptr;
  }

  if (r.len < 2 || r.path[0] != src) {
    return "route does not start at user";
  }

  if (r.path[r.len - 1] < NROOM) {
    return "route does not end at an exit";
  }

  float sum = 0;

  for (int i = 0; i < r.len - 1; i++) {
    int a = r.path[i];
    int b = r.path[i + 1];

    if (!edgeExists(a, b)) return "non-existent corridor";
    if (isinf(W[a][b])) return "route uses blocked corridor";

    sum += W[a][b];
  }

  if (fabs(sum - r.cost) > 0.01f) return "route cost mismatch";
  if (ref.ok && sum > ref.cost + 0.01f) return "route is not optimal";

  return nullptr;
}

String pathStr(const Route& r) {
  String s;

  for (int i = 0; i < r.len; i++) {
    if (i) s += '>';
    s += CODE[r.path[i]];
  }

  return s;
}

// ---------------- OUTPUTS ----------------

void buzz(int frequency) {
  if (!BUZZER_ENABLED || frequency <= 0) {
    noTone(PIN_BUZZ);
  } else {
    tone(PIN_BUZZ, frequency);
  }
}

void updateOutputs() {
  unsigned long t = millis();

  for (int r = 0; r < NROOM; r++) {
    bool red = hz[r] >= ALERT_HAZARD;

    digitalWrite(LED_R[r], red ? HIGH : LOW);
    digitalWrite(LED_G[r], red ? LOW : HIGH);
  }

  bool blinkOn = (t / 300) % 2;
  bool rr = routeLedMode == 1 ||
            (routeLedMode == 2 && blinkOn);

  digitalWrite(LED_R[3], rr ? HIGH : LOW);
  digitalWrite(LED_G[3], routeLedMode == 0 ? HIGH : LOW);

  if (alarmMode == 2) {
    buzz(blinkOn ? 2500 : 0);
  } else if (alarmMode == 1) {
    buzz((t % 2000) < 120 ? 1500 : 0);
  } else {
    buzz(0);
  }
}

void showLcd() {
  char l1[17], l2[17];

  if (cur.ok) {
    snprintf(l1, sizeof(l1), "%-10s%5.0f",
             pathStr(cur).c_str(), cur.cost);
  } else {
    snprintf(l1, sizeof(l1), "%-16s", "NO ROUTE");
  }

  if (failsafe) {
    snprintf(l2, sizeof(l2), "%-16s", "FAILSAFE-EVAC");
  } else if (!cur.ok) {
    snprintf(l2, sizeof(l2), "%-16s", "NO SAFE PATH!!");
  } else if (millis() < fixUntil) {
    snprintf(l2, sizeof(l2), "%-16s", "AGENT FIXED BUG");
  } else if (blockedMask) {
    String b = "NO:";

    for (int i = 0; i < NE; i++) {
      if (blockedMask & (1UL << i)) {
        b += CODE[EDGES[i].a];
        b += '-';
        b += CODE[EDGES[i].b];
        b += ' ';
      }
    }

    snprintf(l2, sizeof(l2), "%-16s", b.c_str());
  } else if (routeRisk >= ALERT_HAZARD) {
    snprintf(l2, sizeof(l2), "%-16s", "CAUTION: HAZARD");
  } else {
    snprintf(l2, sizeof(l2), "%-16s", "ROUTE SAFE");
  }

  lcd.setCursor(0, 0);
  lcd.print(l1);

  lcd.setCursor(0, 1);
  lcd.print(l2);
}

// ---------------- MAIN CYCLE ----------------

void cycle() {
  demoTick();
  readSensors();
  buildGraph();

  Route ref = bellmanFord(startRoom);
  Route r = quarantine ? ref : dijkstraRoute(startRoom);

  const char* why =
    quarantine ? nullptr : verifyRoute(r, startRoom, ref);

  if (why) {
    healCount++;
    consecutiveBugs++;

    Serial.print(F("GUARDIAN: "));
    Serial.println(why);
    Serial.println(F("Replacing route with verified route"));

    r = ref;
    fixUntil = millis() + 4000;

    if (consecutiveBugs >= 3 && !quarantine) {
      quarantine = true;
      Serial.println(F("Primary router quarantined"));
    }
  } else if (!quarantine) {
    consecutiveBugs = 0;
  }

  failsafe = false;

  const char* why2 = verifyRoute(r, startRoom, ref);

  if (why2) {
    r.ok = false;
    failsafe = true;

    Serial.print(F("FAILSAFE: "));
    Serial.println(why2);
  }

  cur = r;
  routeRisk = 0;

  if (cur.ok) {
    for (int i = 0; i < cur.len - 1; i++) {
      routeRisk = max(routeRisk,
        max(hz[cur.path[i]], hz[cur.path[i + 1]]));
    }
  }

  if (!cur.ok) {
    routeLedMode = 2;
    alarmMode = 2;
  } else if (routeRisk >= ALERT_HAZARD) {
    routeLedMode = 1;
    alarmMode = 1;
  } else {
    routeLedMode = 0;
    alarmMode = blockedMask ? 1 : 0;
  }

  if (blockedMask != prevBlockedMask) {
    for (int i = 0; i < NE; i++) {
      if (blockedMask & (1UL << i)) {
        Serial.print(F("BLOCKED: "));
        Serial.print(NAME[EDGES[i].a]);
        Serial.print(F(" <-> "));
        Serial.println(NAME[EDGES[i].b]);
      }
    }

    if (!blockedMask) {
      Serial.println(F("All paths passable again"));
    }

    prevBlockedMask = blockedMask;
  }

  showLcd();

  if (millis() - lastPrint >= 1000) {
    lastPrint = millis();

    Serial.println(F("-------------------------------"));

    for (int r2 = 0; r2 < NROOM; r2++) {
           Serial.print(temp[r2], 1);
      Serial.print(F(" C AQI "));
      Serial.print(aqi[r2], 0);
      Serial.print(F(" hazard "));
      Serial.print(hz[r2], 0);
      Serial.println(hz[r2] >= ALERT_HAZARD ? " RED" : " GREEN");
    }

    Serial.print(F("MODE "));
    Serial.print(demoMode ? F("DEMO") : F("LIVE"));
    Serial.print(F(" | Room "));
    Serial.print(NAME[startRoom]);
    Serial.print(F(" | bug="));
    Serial.print(bugMode);
    Serial.print(F(" quarantine="));
    Serial.print(quarantine);
    Serial.print(F(" repairs="));
    Serial.println(healCount);

    if (cur.ok) {
      Serial.print(F("ROUTE: "));
      Serial.print(pathStr(cur));
      Serial.print(F(" | cost "));
      Serial.print(cur.cost, 1);
      Serial.print(F(" | risk "));
      Serial.println(routeRisk, 0);
    } else {
      Serial.println(F("NO SAFE ROUTE TO AN EXIT"));
    }
  }
}

// ---------------- BUTTONS ----------------

void handleButtons() {
  bool s = digitalRead(PIN_BTN_START);
  bool b = digitalRead(PIN_BTN_BUG);
  unsigned long now = millis();

  if (prevS == HIGH && s == LOW) {
    pressStart = now;
  }

  if (prevS == LOW && s == HIGH && now - lastBtn > 50) {
    lastBtn = now;
    forceCycle = true;

    if (now - pressStart >= 1000) {
      demoMode = !demoMode;
      scen = -1;

      if (!demoMode) {
        bugMode = false;
        quarantine = false;
        consecutiveBugs = 0;
      }

      Serial.println(demoMode ? F("DEMO MODE ON") : F("LIVE MODE"));
    } else {
      startRoom = (startRoom + 1) % NROOM;
      Serial.print(F("Current room: "));
      Serial.println(NAME[startRoom]);
    }
  }

  if (prevB == HIGH && b == LOW && now - lastBtn > 200) {
    lastBtn = now;
    forceCycle = true;
    bugMode = !bugMode;

    if (!bugMode) {
      quarantine = false;
      consecutiveBugs = 0;
    }

    Serial.println(bugMode ? F("BUG INJECTED") : F("ROUTER PATCHED"));
  }

  prevS = s;
  prevB = b;
}

// ---------------- SETUP / LOOP ----------------

void setup() {
  Serial.begin(115200);

  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print(F("Evac Manager"));
  lcd.setCursor(0, 1);
  lcd.print(F("Booting..."));
  delay(1500);

  for (int i = 0; i < 4; i++) {
    pinMode(LED_R[i], OUTPUT);
    pinMode(LED_G[i], OUTPUT);
  }

  pinMode(PIN_BUZZ, OUTPUT);
  pinMode(PIN_BTN_START, INPUT_PULLUP);
  pinMode(PIN_BTN_BUG, INPUT_PULLUP);

  lcd.clear();
  lcd.print(F("Evac Manager"));
  lcd.setCursor(0, 1);
  lcd.print(F("System Ready"));
  delay(1000);

  for (int r = 0; r < NROOM; r++) {
    baseG[r] = constrain(readGas(r), 10, 1000);
  }

  Serial.println(F("Smart Evacuation Manager ready"));
  Serial.println(F("Blue: tap=change room, hold=demo/live"));
  Serial.println(F("Red: inject/clear router bug"));
  Serial.println(F("Demo mode runs without ADS1115"));

  lcd.clear();
}

void loop() {
  handleButtons();

  if (forceCycle || millis() - lastCycle >= 500) {
    forceCycle = false;
    lastCycle = millis();
    cycle();
  }

  updateOutputs();
}
