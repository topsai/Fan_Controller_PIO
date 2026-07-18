import unittest

from hil.protocol import HilClient, HilCommandError, ProtocolError


class FakeTransport:
    def __init__(self, responses):
        self.responses = iter(responses)
        self.lines = []

    def exchange(self, line, timeout=2.0):
        self.lines.append((line, timeout))
        response = next(self.responses)
        if isinstance(response, Exception):
            raise response
        return response


class HilClientTest(unittest.TestCase):
    def test_adds_monotonic_sequence_and_parses_json(self):
        transport = FakeTransport([
            '{"type":"ack","sequence":1,"ok":true}',
            '{"type":"status","sequence":2,"ok":true,"role":"receiver"}',
        ])
        client = HilClient(transport)
        self.assertTrue(client.command("PING")["ok"])
        self.assertEqual("receiver", client.command("STATUS")["role"])
        self.assertEqual("HIL 1 PING", transport.lines[0][0])
        self.assertEqual("HIL 2 STATUS", transport.lines[1][0])

    def test_rejects_invalid_json_and_mismatched_sequence(self):
        with self.assertRaises(ProtocolError):
            HilClient(FakeTransport(["not-json"])).command("PING")
        with self.assertRaises(ProtocolError):
            HilClient(FakeTransport(['{"type":"ack","sequence":9,"ok":true}'])).command("PING")

    def test_raises_stable_command_error(self):
        client = HilClient(FakeTransport(['{"type":"ack","sequence":1,"ok":false,"error":"invalid_argument"}']))
        with self.assertRaisesRegex(HilCommandError, "invalid_argument"):
            client.command("INPUT SPEED 9")

    def test_timeout_is_not_hidden(self):
        client = HilClient(FakeTransport([TimeoutError("no response")]))
        with self.assertRaises(TimeoutError):
            client.command("PING", timeout=0.1)


if __name__ == "__main__":
    unittest.main()
