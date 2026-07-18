#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "protocol.h"

#pragma pack(push, 1)
struct ReceiverControlFrameV2 {
  uint8_t head;
  uint8_t type;
  uint8_t version;
  uint16_t sequence;
  int16_t throttle;
  uint8_t speedLevel;
  uint8_t buttons;
  uint8_t flags;
  uint8_t crc;
};

struct ReceiverControlFrameV1 {
  uint8_t head;
  uint8_t type;
  int16_t throttle;
  uint8_t speedLevel;
  uint8_t buttons;
  uint8_t checksum;
};
#pragma pack(pop)

enum ReceiverFrameResult : uint8_t {
  RECEIVER_FRAME_OK,
  RECEIVER_FRAME_BAD_LENGTH,
  RECEIVER_FRAME_BAD_HEADER,
  RECEIVER_FRAME_BAD_VERSION,
  RECEIVER_FRAME_BAD_CRC,
};

struct ReceiverDecodedControl {
  int16_t throttle;
  uint8_t speedLevel;
  uint8_t buttons;
  uint8_t flags;
  bool legacy;
  bool hasSequence;
  uint16_t sequence;
};

inline ReceiverFrameResult receiverDecodeControlFrame(const uint8_t *data,
                                                      size_t length,
                                                      ReceiverDecodedControl &decoded) {
  memset(&decoded, 0, sizeof(decoded));
  decoded.speedLevel = 1;
  if (data == nullptr) return RECEIVER_FRAME_BAD_LENGTH;

  if (length == sizeof(ReceiverControlFrameV2)) {
    ReceiverControlFrameV2 frame = {};
    memcpy(&frame, data, sizeof(frame));
    if (frame.head != CONTROL_PACKET_HEAD || frame.type != CONTROL_PACKET_TYPE) return RECEIVER_FRAME_BAD_HEADER;
    if (frame.version != CONTROL_PROTOCOL_VERSION) return RECEIVER_FRAME_BAD_VERSION;
    if (protocolCrc8(data, sizeof(frame) - 1) != frame.crc) return RECEIVER_FRAME_BAD_CRC;
    decoded.throttle = frame.throttle;
    decoded.speedLevel = frame.speedLevel;
    decoded.buttons = frame.buttons;
    decoded.flags = frame.flags;
    decoded.hasSequence = true;
    decoded.sequence = frame.sequence;
    return RECEIVER_FRAME_OK;
  }

  if (length == sizeof(ReceiverControlFrameV1)) {
    ReceiverControlFrameV1 frame = {};
    memcpy(&frame, data, sizeof(frame));
    if (frame.head != CONTROL_PACKET_HEAD || frame.type != CONTROL_PACKET_TYPE) return RECEIVER_FRAME_BAD_HEADER;
    if (!protocolLegacyChecksumIsValid(data, sizeof(frame))) return RECEIVER_FRAME_BAD_CRC;
    decoded.throttle = frame.throttle;
    decoded.speedLevel = frame.speedLevel;
    decoded.buttons = frame.buttons;
    decoded.legacy = true;
    return RECEIVER_FRAME_OK;
  }

  return RECEIVER_FRAME_BAD_LENGTH;
}
