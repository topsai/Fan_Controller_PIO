"""Line-oriented HIL protocol client."""

import json


class ProtocolError(RuntimeError):
    pass


class HilCommandError(ProtocolError):
    def __init__(self, error, response):
        super().__init__(error)
        self.error = error
        self.response = response


class HilClient:
    def __init__(self, transport):
        self.transport = transport
        self.sequence = 0
        self.on_exchange = None

    def command(self, command, timeout=2.0):
        self.sequence += 1
        request = f"HIL {self.sequence} {command}"
        raw = self.transport.exchange(request, timeout=timeout)
        try:
            response = json.loads(raw)
        except (TypeError, json.JSONDecodeError) as error:
            raise ProtocolError(f"invalid JSON response: {raw!r}") from error
        if not isinstance(response, dict):
            raise ProtocolError("response must be a JSON object")
        if response.get("sequence") != self.sequence:
            raise ProtocolError(
                f"sequence mismatch: expected {self.sequence}, got {response.get('sequence')}"
            )
        if response.get("type") not in {"ack", "status"}:
            raise ProtocolError(f"invalid response type: {response.get('type')!r}")
        if self.on_exchange:
            self.on_exchange(request, raw, response)
        if response.get("ok") is not True:
            raise HilCommandError(response.get("error", "unknown_error"), response)
        return response
