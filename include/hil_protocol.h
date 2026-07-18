#pragma once

#ifdef FAN_CONTROLLER_HIL

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static constexpr size_t HIL_MAX_LINE_LENGTH = 192;
static constexpr size_t HIL_MAX_TEXT_LENGTH = 64;
static constexpr uint8_t HIL_PROTOCOL_VERSION = 1;

enum HilCommandType : uint8_t {
  HIL_COMMAND_INVALID,
  HIL_COMMAND_PING,
  HIL_COMMAND_STATUS,
  HIL_COMMAND_OUTPUTS_LOCK,
  HIL_COMMAND_OUTPUTS_UNLOCK,
  HIL_COMMAND_RESET,
  HIL_COMMAND_REBOOT,
  HIL_COMMAND_REMOTE_FRAME,
  HIL_COMMAND_REMOTE_CONTROL,
  HIL_COMMAND_REMOTE_REPEAT,
  HIL_COMMAND_REMOTE_INVALID,
  HIL_COMMAND_VESC_PHYSICAL,
  HIL_COMMAND_VESC_VALUE,
  HIL_COMMAND_VESC_FAULT,
  HIL_COMMAND_INPUT_JOYSTICK,
  HIL_COMMAND_INPUT_SPEED,
  HIL_COMMAND_BUTTON_CLICK,
  HIL_COMMAND_BUTTON_HOLD,
  HIL_COMMAND_STATUS_FRAME,
  HIL_COMMAND_SENSOR_PHYSICAL,
  HIL_COMMAND_SENSOR_VALUE,
  HIL_COMMAND_SENSOR_FAULT,
  HIL_COMMAND_NVS_CLEAR,
};

enum HilParseResult : uint8_t {
  HIL_PARSE_OK,
  HIL_PARSE_EMPTY,
  HIL_PARSE_LINE_TOO_LONG,
  HIL_PARSE_INVALID_PREFIX,
  HIL_PARSE_INVALID_SEQUENCE,
  HIL_PARSE_UNKNOWN_COMMAND,
  HIL_PARSE_INVALID_ARGUMENT,
};

struct HilCommand {
  uint32_t sequence;
  HilCommandType type;
  char text[HIL_MAX_TEXT_LENGTH];
  char data[HIL_MAX_TEXT_LENGTH];
  int32_t values[6];
  float realValues[4];
};

inline bool hilParseUint32(const char *text, uint32_t &value) {
  if (text == nullptr || *text == '\0' || *text == '-') return false;
  errno = 0;
  char *end = nullptr;
  const unsigned long parsed = strtoul(text, &end, 10);
  if (errno == ERANGE || end == text || *end != '\0' || parsed > UINT32_MAX) return false;
  value = static_cast<uint32_t>(parsed);
  return true;
}

inline bool hilParseInt32(const char *text, int32_t &value) {
  if (text == nullptr || *text == '\0') return false;
  errno = 0;
  char *end = nullptr;
  const long parsed = strtol(text, &end, 10);
  if (errno == ERANGE || end == text || *end != '\0' || parsed < INT32_MIN || parsed > INT32_MAX) return false;
  value = static_cast<int32_t>(parsed);
  return true;
}

inline bool hilParseFloat(const char *text, float &value) {
  if (text == nullptr || *text == '\0') return false;
  errno = 0;
  char *end = nullptr;
  const float parsed = strtof(text, &end);
  if (errno == ERANGE || end == text || *end != '\0' || !isfinite(parsed)) return false;
  value = parsed;
  return true;
}

inline void hilCopyText(char *destination, const char *source) {
  strncpy(destination, source, HIL_MAX_TEXT_LENGTH - 1);
  destination[HIL_MAX_TEXT_LENGTH - 1] = '\0';
}

inline HilParseResult hilParseCommand(const char *line, HilCommand &command) {
  memset(&command, 0, sizeof(command));
  command.type = HIL_COMMAND_INVALID;
  if (line == nullptr || *line == '\0') return HIL_PARSE_EMPTY;
  const size_t length = strlen(line);
  if (length >= HIL_MAX_LINE_LENGTH) return HIL_PARSE_LINE_TOO_LONG;

  char buffer[HIL_MAX_LINE_LENGTH] = {};
  memcpy(buffer, line, length + 1);
  char *tokens[12] = {};
  size_t count = 0;
  char *save = nullptr;
  for (char *token = strtok_r(buffer, " \t\r\n", &save); token != nullptr;
       token = strtok_r(nullptr, " \t\r\n", &save)) {
    if (count >= 12) return HIL_PARSE_INVALID_ARGUMENT;
    tokens[count++] = token;
  }
  if (count == 0) return HIL_PARSE_EMPTY;
  if (strcmp(tokens[0], "HIL") != 0) return HIL_PARSE_INVALID_PREFIX;
  if (count < 3 || !hilParseUint32(tokens[1], command.sequence)) return HIL_PARSE_INVALID_SEQUENCE;

  const char *verb = tokens[2];
  if (strcmp(verb, "PING") == 0) {
    if (count != 3) return HIL_PARSE_INVALID_ARGUMENT;
    command.type = HIL_COMMAND_PING;
  } else if (strcmp(verb, "STATUS") == 0) {
    if (count == 3) command.type = HIL_COMMAND_STATUS;
    else if (count == 5 && strcmp(tokens[3], "FRAME") == 0) {
      command.type = HIL_COMMAND_STATUS_FRAME;
      hilCopyText(command.data, tokens[4]);
    } else return HIL_PARSE_INVALID_ARGUMENT;
  } else if (strcmp(verb, "RESET") == 0 || strcmp(verb, "REBOOT") == 0) {
    if (count != 3) return HIL_PARSE_INVALID_ARGUMENT;
    command.type = strcmp(verb, "RESET") == 0 ? HIL_COMMAND_RESET : HIL_COMMAND_REBOOT;
  } else if (strcmp(verb, "OUTPUTS") == 0) {
    if (count != 4) return HIL_PARSE_INVALID_ARGUMENT;
    if (strcmp(tokens[3], "LOCK") == 0) command.type = HIL_COMMAND_OUTPUTS_LOCK;
    else if (strcmp(tokens[3], "UNLOCK") == 0) command.type = HIL_COMMAND_OUTPUTS_UNLOCK;
    else return HIL_PARSE_INVALID_ARGUMENT;
  } else if (strcmp(verb, "REMOTE") == 0) {
    if (count >= 4 && strcmp(tokens[3], "FRAME") == 0) {
      if (count != 6) return HIL_PARSE_INVALID_ARGUMENT;
      command.type = HIL_COMMAND_REMOTE_FRAME;
      hilCopyText(command.text, tokens[4]);
      hilCopyText(command.data, tokens[5]);
    } else if (count >= 4 && strcmp(tokens[3], "CONTROL") == 0) {
      if (count != 9) return HIL_PARSE_INVALID_ARGUMENT;
      command.type = HIL_COMMAND_REMOTE_CONTROL;
      hilCopyText(command.text, tokens[4]);
      for (size_t i = 0; i < 4; i++) {
        if (!hilParseInt32(tokens[5 + i], command.values[i])) return HIL_PARSE_INVALID_ARGUMENT;
      }
    } else if (count >= 4 && strcmp(tokens[3], "REPEAT") == 0) {
      if (count != 5 || !hilParseInt32(tokens[4], command.values[0])) return HIL_PARSE_INVALID_ARGUMENT;
      command.type = HIL_COMMAND_REMOTE_REPEAT;
    } else if (count >= 4 && strcmp(tokens[3], "INVALID") == 0) {
      if (count != 6) return HIL_PARSE_INVALID_ARGUMENT;
      command.type = HIL_COMMAND_REMOTE_INVALID;
      hilCopyText(command.text, tokens[4]);
      hilCopyText(command.data, tokens[5]);
    } else {
      return HIL_PARSE_INVALID_ARGUMENT;
    }
  } else if (strcmp(verb, "VESC") == 0) {
    if (count == 4 && strcmp(tokens[3], "PHYSICAL") == 0) command.type = HIL_COMMAND_VESC_PHYSICAL;
    else if (count == 4 && strcmp(tokens[3], "FAULT") == 0) command.type = HIL_COMMAND_VESC_FAULT;
    else if (count == 6 && strcmp(tokens[3], "VALUE") == 0 &&
             hilParseInt32(tokens[4], command.values[0]) && hilParseInt32(tokens[5], command.values[1])) {
      command.type = HIL_COMMAND_VESC_VALUE;
    } else return HIL_PARSE_INVALID_ARGUMENT;
  } else if (strcmp(verb, "INPUT") == 0) {
    if (count != 5) return HIL_PARSE_INVALID_ARGUMENT;
    if (strcmp(tokens[3], "JOYSTICK") == 0) command.type = HIL_COMMAND_INPUT_JOYSTICK;
    else if (strcmp(tokens[3], "SPEED") == 0) command.type = HIL_COMMAND_INPUT_SPEED;
    else return HIL_PARSE_INVALID_ARGUMENT;
    if (strcmp(tokens[4], "PHYSICAL") != 0 && !hilParseInt32(tokens[4], command.values[0])) return HIL_PARSE_INVALID_ARGUMENT;
    hilCopyText(command.text, tokens[4]);
  } else if (strcmp(verb, "BUTTON") == 0) {
    if (count == 5 && strcmp(tokens[4], "CLICK") == 0) command.type = HIL_COMMAND_BUTTON_CLICK;
    else if (count == 6 && strcmp(tokens[4], "HOLD") == 0 && hilParseInt32(tokens[5], command.values[0])) command.type = HIL_COMMAND_BUTTON_HOLD;
    else return HIL_PARSE_INVALID_ARGUMENT;
    hilCopyText(command.text, tokens[3]);
  } else if (strcmp(verb, "SENSOR") == 0) {
    if (count == 5 && strcmp(tokens[4], "PHYSICAL") == 0) command.type = HIL_COMMAND_SENSOR_PHYSICAL;
    else if (count == 5 && strcmp(tokens[4], "FAULT") == 0) command.type = HIL_COMMAND_SENSOR_FAULT;
    else if (count == 6 && strcmp(tokens[4], "VALUE") == 0 && hilParseFloat(tokens[5], command.realValues[0])) {
      command.type = HIL_COMMAND_SENSOR_VALUE;
    } else return HIL_PARSE_INVALID_ARGUMENT;
    hilCopyText(command.text, tokens[3]);
  } else if (strcmp(verb, "NVS") == 0) {
    if (count != 4 || strcmp(tokens[3], "CLEAR") != 0) return HIL_PARSE_INVALID_ARGUMENT;
    command.type = HIL_COMMAND_NVS_CLEAR;
  } else {
    return HIL_PARSE_UNKNOWN_COMMAND;
  }
  return HIL_PARSE_OK;
}

inline const char *hilParseError(HilParseResult result) {
  switch (result) {
    case HIL_PARSE_OK: return "ok";
    case HIL_PARSE_EMPTY: return "empty";
    case HIL_PARSE_LINE_TOO_LONG: return "line_too_long";
    case HIL_PARSE_INVALID_PREFIX: return "invalid_prefix";
    case HIL_PARSE_INVALID_SEQUENCE: return "invalid_sequence";
    case HIL_PARSE_UNKNOWN_COMMAND: return "unknown_command";
    case HIL_PARSE_INVALID_ARGUMENT: return "invalid_argument";
  }
  return "unknown_command";
}

#endif
