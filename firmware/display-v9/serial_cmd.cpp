#include "serial_cmd.h"

#include "fake_data.h"
#include "ui_config.h"
#include "ui_shell.h"

static char s_line[96];
static size_t s_len = 0;

static void print_help() {
  Serial.println("CMDS: TEMP <f> | SETPOINT <f> | DURATION <1-99> | STIRRER ON|OFF |");
  Serial.println("      POST ON|OFF | NODE <0-5> ON|OFF | NULL <c> <b> <s> |");
  Serial.println("      UNNULL <c> <b> <s> | PUSH | STATUS | HELP");
  Serial.println("      (s = 0 CO2,1 CH4,2 Press,3 pH,4 Temp)");
}

static void print_status() {
  Serial.printf("run=%d dur=%dh set=%.1f stir=%d post=%d chamber=%.1f nulls=%d\n",
                g.experimentRunning, g.durationH, g.tempSetpointC, g.stirrerOn,
                g.postEnabled, g.chamberTempC, fake_null_count());
  for (int c = 0; c < UI_CLUSTERS; c++) {
    Serial.printf("node %d: %s\n", c, g.cluster[c].online ? "ONLINE" : "OFFLINE");
  }
}

static bool parse_onoff(const char *t, bool &out) {
  if (strcasecmp(t, "ON") == 0 || strcasecmp(t, "1") == 0) {
    out = true;
    return true;
  }
  if (strcasecmp(t, "OFF") == 0 || strcasecmp(t, "0") == 0) {
    out = false;
    return true;
  }
  return false;
}

static void handle_line(char *line) {
  // Tokenize: CMD [args...]
  char *cmd = strtok(line, " \t");
  if (cmd == nullptr || *cmd == '\0') return;
  for (char *p = cmd; *p; p++) *p = toupper(*p);

  bool mutated = false;

  if (strcmp(cmd, "HELP") == 0 || strcmp(cmd, "?") == 0) {
    print_help();
  } else if (strcmp(cmd, "STATUS") == 0) {
    print_status();
  } else if (strcmp(cmd, "PUSH") == 0) {
    fake_wt32_push();
    mutated = true;
  } else if (strcmp(cmd, "TEMP") == 0) {
    char *a = strtok(nullptr, " \t");
    if (a != nullptr) {
      g.chamberTempC = atof(a);
      Serial.printf("chamber=%.1f\n", g.chamberTempC);
      mutated = true;
    } else {
      Serial.println("usage: TEMP 38.3");
    }
  } else if (strcmp(cmd, "SETPOINT") == 0) {
    char *a = strtok(nullptr, " \t");
    if (a != nullptr) {
      float v = atof(a);
      if (v >= UI_TEMP_MIN_C && v <= UI_TEMP_MAX_C) {
        g.tempSetpointC = v;
        mutated = true;
      } else {
        Serial.println("range 35.0-40.0");
      }
    } else {
      Serial.println("usage: SETPOINT 37.5");
    }
  } else if (strcmp(cmd, "DURATION") == 0) {
    char *a = strtok(nullptr, " \t");
    if (a != nullptr) {
      int v = atoi(a);
      if (v >= UI_DURATION_MIN_H && v <= UI_DURATION_MAX_H) {
        g.durationH = v;
        mutated = true;
      } else {
        Serial.println("range 1-99");
      }
    } else {
      Serial.println("usage: DURATION 24");
    }
  } else if (strcmp(cmd, "STIRRER") == 0) {
    char *a = strtok(nullptr, " \t");
    bool v = false;
    if (a != nullptr && parse_onoff(a, v)) {
      g.stirrerOn = v;
      Serial.printf("[FAKE->WT32] STIRRER %s\n", v ? "ON" : "OFF");
      mutated = true;
    } else {
      Serial.println("usage: STIRRER ON|OFF");
    }
  } else if (strcmp(cmd, "POST") == 0) {
    char *a = strtok(nullptr, " \t");
    bool v = false;
    if (a != nullptr && parse_onoff(a, v)) {
      g.postEnabled = v;
      mutated = true;
    } else {
      Serial.println("usage: POST ON|OFF");
    }
  } else if (strcmp(cmd, "NODE") == 0) {
    char *a = strtok(nullptr, " \t");
    char *b = strtok(nullptr, " \t");
    bool v = false;
    int c = a != nullptr ? atoi(a) : -1;
    if (c >= 0 && c < UI_CLUSTERS && b != nullptr && parse_onoff(b, v)) {
      g.cluster[c].online = v;
      if (!v) {
        for (int bi = 0; bi < UI_BOTTLES_PER_CLUSTER; bi++)
          for (int s = 0; s < S_COUNT; s++) g.cluster[c].bottle[bi].sensorNull[s] = true;
      } else {
        for (int bi = 0; bi < UI_BOTTLES_PER_CLUSTER; bi++)
          for (int s = 0; s < S_COUNT; s++)
            g.cluster[c].bottle[bi].sensorNull[s] = false;
      }
      Serial.printf("node %d %s\n", c, v ? "ONLINE" : "OFFLINE");
      mutated = true;
    } else {
      Serial.println("usage: NODE <0-5> ON|OFF");
    }
  } else if (strcmp(cmd, "NULL") == 0 || strcmp(cmd, "UNNULL") == 0) {
    bool setNull = (strcmp(cmd, "NULL") == 0);
    char *a = strtok(nullptr, " \t");
    char *b = strtok(nullptr, " \t");
    char *s = strtok(nullptr, " \t");
    int c = a != nullptr ? atoi(a) : -1;
    int bi = b != nullptr ? atoi(b) : -1;
    int si = s != nullptr ? atoi(s) : -1;
    if (c >= 0 && c < UI_CLUSTERS && bi >= 0 && bi < UI_BOTTLES_PER_CLUSTER && si >= 0 &&
        si < S_COUNT) {
      g.cluster[c].bottle[bi].sensorNull[si] = setNull;
      Serial.printf("C%d/B%d/%s %s\n", c, bi, SENSOR_NAMES[si],
                    setNull ? "NULL" : "valid");
      mutated = true;
    } else {
      Serial.println("usage: NULL <c> <b> <s>  (s=0..4)");
    }
  } else {
    Serial.println("unknown cmd; send HELP");
  }

  if (mutated) {
    ui_shell_on_data();
    ui_eval_overheat();
  }
}

void serial_cmd_poll() {
  while (Serial.available() > 0) {
    char ch = (char)Serial.read();
    if (ch == '\r') continue;
    if (ch == '\n') {
      s_line[s_len] = '\0';
      s_len = 0;
      handle_line(s_line);
    } else if (s_len + 1 < sizeof(s_line)) {
      s_line[s_len++] = ch;
    }
    // Overlong lines: keep consuming until newline, drop the excess.
  }
}
