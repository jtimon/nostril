"""Independent loopback WebSocket/TLS fixture (Python websockets >= 10)."""
import asyncio
import json
import pathlib
import signal
import ssl
import sys

import websockets
from websockets.legacy.server import serve


async def main():
    port_file, cert, key, event_file, unfollowed_file = sys.argv[1:]
    early_event = pathlib.Path(event_file).read_text()
    unfollowed_event = pathlib.Path(unfollowed_file).read_text()
    tls = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    tls.load_cert_chain(cert, key)
    received = {}
    counts = {}
    stopped = asyncio.Event()
    asyncio.get_running_loop().add_signal_handler(signal.SIGTERM, stopped.set)

    async def handler(socket, path):
        try:
            assert path in ("/nostril?probe=1", "/nostril?probe=early"), path
            assert socket.subprotocol is None
            async for raw in socket:
                assert isinstance(raw, str) and "\0" not in raw
                command = json.loads(raw)
                if command[0] == "REQ":
                    assert len(command) == 3 and command[:2] == ["REQ", "local"]
                    assert command[2]["kinds"] == [1] and command[2]["limit"] == 0
                    assert set(command[2]) <= {"kinds", "limit", "authors"}
                    if "authors" in command[2]:
                        # Deliberately violate the filter: the client must apply
                        # author selection locally even for valid signed notes.
                        await socket.send(unfollowed_event)
                        await socket.send(early_event)
                    if path.endswith("=early"):
                        await socket.send(early_event)
                    await socket.send(['["EOSE",', '"local"]'])
                    await socket.send('["NOTICE","fixture ready"]')
                    await socket.send("not json")
                    await socket.send('["EVENT","local",{}]')
                elif command[0] == "CLOSE":
                    assert command == ["CLOSE", "local"]
                elif command[0] == "EVENT":
                    assert len(command) == 2
                    event = command[1]
                    event_id = event["id"]
                    content = event["content"]
                    counts[event_id] = counts.get(event_id, 0) + 1
                    if content in ("retry", "lost-ack"):
                        assert received.setdefault(event_id, raw) == raw, "retry resigned or changed the event"
                    if content == "reject":
                        await socket.send(json.dumps(["OK", event_id, False, "blocked: fixture policy"]))
                        continue
                    if content == "oversize":
                        await socket.send("x" * 65537)
                        continue
                    if content == "binary":
                        await socket.send(b"[]")
                        continue
                    if content == "invalid-utf8":
                        await socket.write_frame(True, 1, b"\xff")
                        continue
                    if content == "closed":
                        await socket.send('["CLOSED","local","blocked: fixture subscription"]')
                        await socket.send('["EOSE","local"]')
                        await socket.send(early_event)
                        continue
                    message = json.dumps(["EVENT", "local", event], ensure_ascii=False)
                    await socket.send([message[:17], message[17:]])
                    pong = await socket.ping(b"nostril-ping")
                    await asyncio.wait_for(pong, timeout=5)
                    if content == "retry" and counts[event_id] == 1:
                        await socket.close()
                        continue
                    if content == "lost-ack" and counts[event_id] == 1:
                        continue
                    await socket.send(json.dumps(["OK", event_id, True, ""]))
        except websockets.exceptions.ConnectionClosed:
            pass

    async with serve(handler, "127.0.0.1", 0) as plain, \
            serve(handler, "127.0.0.1", 0, ssl=tls) as secure, \
            serve(handler, "127.0.0.2", 0, ssl=tls) as wrong_host:
        ports = [server.sockets[0].getsockname()[1] for server in (plain, secure, wrong_host)]
        pathlib.Path(port_file).write_text("\n".join(map(str, ports)))
        await stopped.wait()


asyncio.run(main())
