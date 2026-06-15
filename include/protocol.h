#pragma once

#include <stdint.h>

static constexpr uint8_t CONTROL_PACKET_HEAD = 0xA5;
static constexpr uint8_t CONTROL_PACKET_TYPE = 0x01;
static constexpr uint8_t STATUS_PACKET_HEAD = 0x5A;
static constexpr uint8_t STATUS_PACKET_TYPE = 0x02;
static constexpr uint8_t CONTROL_PROTOCOL_VERSION = 2;
static constexpr uint8_t STATUS_PROTOCOL_VERSION = 2;
static constexpr uint8_t LEGACY_CONTROL_PACKET_SIZE = 7;
static constexpr uint8_t LEGACY_STATUS_PACKET_SIZE = 14;

static constexpr uint8_t STATUS_FLAG_FAILSAFE = 0x01;
static constexpr uint8_t STATUS_FLAG_VESC_VALID = 0x02;
static constexpr uint8_t STATUS_FLAG_PROTOCOL_FAULT = 0x04;
static constexpr uint8_t STATUS_FLAG_OUTPUT_LOCKED = 0x08;

inline uint8_t protocolCrc8(const uint8_t *data, uint8_t len) {
  uint8_t crc = 0x00;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
    }
  }
  return crc;
}

inline uint8_t protocolLegacyChecksum(const uint8_t *data, uint8_t len) {
  uint8_t sum = 0;
  if (len == 0) {
    return sum;
  }
  for (uint8_t i = 0; i < len - 1; i++) {
    sum += data[i];
  }
  return sum;
}

inline bool protocolLegacyChecksumIsValid(const uint8_t *data, uint8_t len) {
  if (data == nullptr || len == 0) {
    return false;
  }
  return protocolLegacyChecksum(data, len) == data[len - 1];
}

inline bool protocolSequenceIsFresh(uint16_t sequence, uint16_t lastSequence, bool hasLastSequence) {
  if (!hasLastSequence) {
    return true;
  }
  return (int16_t)(sequence - lastSequence) > 0;
}

inline void protocolRememberSequence(uint16_t sequence, uint16_t &lastSequence, bool &hasLastSequence) {
  lastSequence = sequence;
  hasLastSequence = true;
}
