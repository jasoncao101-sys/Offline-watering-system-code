// ====================================================================
//   GREENHAVEN SMART IRRIGATION SYSTEM - FULLY OFFLINE SAFE VERSION
// ====================================================================

// --- HARDWARE PINS ---
const int SOIL_SENSOR_PIN = 32;
const int RELAY_ENABLE = 23;    

// --- SYSTEM STATE VARIABLES ---
bool valveIsCurrentlyOpen = false;
int currentMoisture = 2700; 

// --- SAFETY TIMERS & LOCAL TRACKING ---
unsigned long valveOpenStartTime = 0; 
const unsigned long MAX_WATERING_DURATION = 300000; // 5 minutes max runtime (in ms)

unsigned long valveCloseTimer = 0;      // Tracks the 1-minute wet timer
bool startingWetConfirmation = false;   // Flags if we are actively counting down

unsigned long valveCloseTimestamp = 0;  // Stores millis() when valve was last closed
bool hasWateredOnce = false;            // Tracks if a cooldown is active
const unsigned long COOLDOWN_PERIOD_MS = 14400000; // 4 hours cooldown in ms (4 * 60 * 60 * 1000)

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_ENABLE, OUTPUT);
 
  digitalWrite(RELAY_ENABLE, HIGH); // Valve firmly CLOSED at boot
  Serial.println("--- System Online: Standalone / Offline Irrigation Mode ---");
}

void loop() {
  currentMoisture = analogRead(SOIL_SENSOR_PIN); 
  unsigned long currentMillis = millis();
 
  Serial.print("Moisture Level: ");
  Serial.println(currentMoisture);

  // --- SAFETY CHECK: 5-MINUTE HARD TIMEOUT ---
  if (valveIsCurrentlyOpen && (currentMillis - valveOpenStartTime >= MAX_WATERING_DURATION)) {
    Serial.println("\n!!! EMERGENCY SHUTOFF: 5-Minute Maximum Watering Limit Reached !!!");
    digitalWrite(RELAY_ENABLE, HIGH); 
    valveIsCurrentlyOpen = false;
    startingWetConfirmation = false; 
    valveCloseTimestamp = millis();
    hasWateredOnce = true;
  }

  // --- SOIL IS DRY INTERACTION LOGIC (>= 1700) ---
  if (currentMoisture >= 1700) { 
    if (startingWetConfirmation) {
      Serial.println("--> Moisture fluctuated back up. Resetting 1-minute window.");
      startingWetConfirmation = false;
    }

    if (!valveIsCurrentlyOpen) {
      // 4-Hour Cooldown Guard
      if (hasWateredOnce && (currentMillis - valveCloseTimestamp < COOLDOWN_PERIOD_MS)) {
        unsigned long remainingSec = (COOLDOWN_PERIOD_MS - (currentMillis - valveCloseTimestamp)) / 1000;
        Serial.print("--> Locked in COOLDOWN. Remaining: ");
        Serial.print(remainingSec / 60);
        Serial.println(" minutes.");
      } 
      else {
        delay(50); // Anti-static check
        if (analogRead(SOIL_SENSOR_PIN) >= 1700) { 
          Serial.println("=======> [SOIL DRY] Valve OPEN <=======");
          digitalWrite(RELAY_ENABLE, LOW); 
          valveIsCurrentlyOpen = true;
          valveOpenStartTime = millis(); 
          delay(500); 
        }
      }
    }
  }
  // --- SOIL IS WET RECOVERY LOGIC (< 900) ---
  else if (currentMoisture < 900) {
    if (valveIsCurrentlyOpen) {
      if (!startingWetConfirmation) {
        valveCloseTimer = millis();
        startingWetConfirmation = true;
        Serial.println("--> Moisture under 900. Starting 1-minute verification...");
      }
      
      // 1 Full Minute Under 900 Verified
      if (currentMillis - valveCloseTimer >= 60000) {
        Serial.println("=======> [SOIL VERIFIED WET] Valve CLOSED <=======");
        digitalWrite(RELAY_ENABLE, HIGH); 
        valveIsCurrentlyOpen = false;
        startingWetConfirmation = false; 
        valveCloseTimestamp = millis(); 
        hasWateredOnce = true;
        delay(500); 
      } else {
        unsigned long secondsElapsed = (currentMillis - valveCloseTimer) / 1000;
        Serial.print("--> Confirming wet state: ");
        Serial.print(secondsElapsed);
        Serial.println(" / 60 seconds.");
      }
    }
  }
  // --- GREY ZONE (900 to 1700) ---
  else {
    if (startingWetConfirmation) {
      Serial.println("--> Moisture drifted above 900. Pausing verification window.");
      startingWetConfirmation = false;
    }
  }
 
  delay(100);
}