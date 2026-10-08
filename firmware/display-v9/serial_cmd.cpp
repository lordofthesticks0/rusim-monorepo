#include "serial_cmd.h"

#include <ctype.h>

#include "fake_data.h"
#include "link_modbus.h"
#include "provision_ap.h"
#include "ui_config.h"
#include "ui_shell.h"

// 96 bytes was sized for the one-argument fake-data commands. CREDS carries a
// username and a password of up to LINK_USER_MAX / LINK_PASS_MAX, so the
// buffer has to fit the command word, both fields, two spaces and the
// terminator. 200 is comfortably above that and still bounded.
static char s_line[200];
static size_t s_len = 0;

static void print_help() {
  Serial.println("CMDS: TEMP <f> | SETPOINT <f> | DURATION <1-99> | STIRRER ON|OFF |");
  Serial.println("      POST ON|OFF | RUN ON|OFF | EXP <n> | NODE <0-5> ON|OFF |");
  Serial.println("      NULL <c> <b> <s> | UNNULL <c> <b> <s> | PUSH | STATUS | LINK | HELP");
  Serial.println("      (s = 0 CO2,1 CH4,2 Press,3 pH,4 Temp)");
  Serial.println("CREDS <user> <pass>   stage portal credentials for the WT32");
  Serial.println("AP                    toggle the cloaked SoftAP credential form");
}

static void print_status() {
  Serial.printf("run=%d exp=%d dur=%dh set=%.1f stir=%d post=%d chamber=%.1f nulls=%d\n",
                g.experimentRunning, g.expNum, g.durationH, g.tempSetpointC,
                g.stirrerOn, g.postEnabled, g.chamberTempC, fake_null_count());
  for (int c = 0; c < UI_CLUSTERS; c++) {
    Serial.printf("node %d: %s\n", c, g.cluster[c].online ? "ONLINE" : "OFFLINE");
  }
}

// Credentials sourced from the phone form on /setup, and staged onto the
// Modbus link exactly as the on-screen Confirm does. Username only: the
// password would land in the terminal scrollback and in any captured log,
// which is what the non-echoing serial prompt in the WT32 ping-test exists
// to avoid. Use the SoftAP or the touchscreen for the password itself.
static void handle_creds(char *args) {
  char *user = strtok(args, " \t");
  char *pass = strtok(nullptr, " \t");
  if (user == nullptr || pass == nullptr) {
    Serial.println("usage: CREDS <user> <pass>");
    return;
  }
  int dur = g.durationH;
  if (dur < UI_DURATION_MIN_H) dur = UI_DURATION_MIN_H;
  if (dur > UI_DURATION_MAX_H) dur = UI_DURATION_MAX_H;
  g.durationH = dur;
  link_modbus_set_credentials(user, pass);
  link_modbus_set_exp_state(g.experimentRunning, g.expNum, g.durationH);
  Serial.printf("[SETUP] creds staged from serial user='%s'\n", user);
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
  // Captured before strtok starts replacing delimiters with NULs.
  const size_t lineLen = strlen(line);

  // Tokenize: CMD [args...]
  char *cmd = strtok(line, " \t");
  if (cmd == nullptr || *cmd == '\0') return;
  for (char *p = cmd; *p; p++) *p = toupper(*p);

  bool mutated = false;

  if (strcmp(cmd, "CREDS") == 0) {
    // strtok replaced the first delimiter after the command word with a NUL,
    // so the argument text starts one byte past the end of cmd. If cmd ran to
    // the end of the line there was no delimiter, and stepping over the
    // terminator would read past the line into whatever the previous, longer
    // command left in the buffer.
    size_t cmdLen = strlen(cmd);
    if (cmd + cmdLen >= line + lineLen) {
      Serial.println("usage: CREDS <user> <pass>");
      return;
    }
    handle_creds(cmd + cmdLen + 1);
    return;
  } else if (strcmp(cmd, "AP") == 0) {
    if (provision_ap_active()) {
      provision_ap_end();
      Serial.println("[AP] stopped");
    } else {
      provision_ap_begin();
      if (provision_ap_active()) {
        char payload[PROVISION_QR_MAX];
        provision_ap_wifi_qr_payload(payload, sizeof(payload));
        Serial.printf("[AP] ssid='%s' hidden=%d\n", provision_ap_ssid(),
                      provision_ap_hidden() ? 1 : 0);
        Serial.printf("[AP] step 1 QR: %s\n", payload);
        Serial.printf("[AP] step 2 QR: %s\n", provision_ap_url());
      } else {
        Serial.println("[AP] failed to start");
      }
    }
  } else if (strcmp(cmd, "HELP") == 0 || strcmp(cmd, "?") == 0) {
    print_help();
  } else if (strcmp(cmd, "STATUS") == 0) {
    print_status();
  } else if (strcmp(cmd, "LINK") == 0) {
    link_modbus_debug_print();
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
        link_modbus_set_exp_state(g.experimentRunning, g.expNum, g.durationH);
        mutated = true;
      } else {
        Serial.println("range 1-99");
      }
    } else {
      Serial.println("usage: DURATION 24");
    }
  } else if (strcmp(cmd, "EXP") == 0) {
    char *a = strtok(nullptr, " \t");
    if (a != nullptr) {
      int v = atoi(a);
      if (v >= UI_EXP_NUM_MIN && v <= UI_EXP_NUM_MAX) {
        g.expNum = v;
        link_modbus_set_exp_state(g.experimentRunning, g.expNum, g.durationH);
        mutated = true;
      } else {
        Serial.printf("range %d-%d\n", UI_EXP_NUM_MIN, UI_EXP_NUM_MAX);
      }
    } else {
      Serial.println("usage: EXP 3");
    }
  } else if (strcmp(cmd, "RUN") == 0) {
    char *a = strtok(nullptr, " \t");
    bool v = false;
    if (a != nullptr && parse_onoff(a, v)) {
      // Each transition from stopped to running is a new experiment number.
      if (v && !g.experimentRunning) g.expNum++;
      g.experimentRunning = v;
      link_modbus_set_exp_state(g.experimentRunning, g.expNum, g.durationH);
      Serial.printf("[FAKE->WT32] RUN %s exp=%d\n", v ? "ON" : "OFF", g.expNum);
      mutated = true;
    } else {
      Serial.println("usage: RUN ON|OFF");
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
