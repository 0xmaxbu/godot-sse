#!/usr/bin/env python3
"""
Mock SSE Server for testing Godot SSEClient GDExtension

Endpoints:
  GET  /events            - Send 3 standard SSE events
  POST /chat              - Send 5 chunk events with different types
  GET  /rapid             - Send 100 events rapidly
  GET  /error-404         - Return 404 Not Found
  GET  /wrong-type        - Return application/json instead of text/event-stream
  GET  /with-id           - Send events with id field
  GET  /multiline         - Send events with multi-line data
  GET  /retry             - Send retry field to override reconnect time
  GET  /disconnect        - Send 2 events then close connection
  GET  /reconnect-test    - Send 1 event then close (for auto-reconnect testing)
  GET  /retry-override    - Send retry:500 + data then close
  GET  /events-with-id    - Smart ID-based resumption (checks Last-Event-ID header)
"""

import http.server
import socketserver
import json
import time
import sys
from urllib.parse import urlparse, parse_qs

PORT = 9999


class SSEHandler(http.server.BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        sys.stderr.write(
            "[%s] %s - %s\n"
            % (self.log_date_time_string(), self.address_string(), format % args)
        )

    def send_sse_headers(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Connection", "keep-alive")
        self.end_headers()

    def send_event(self, event_type=None, data=None, event_id=None, retry=None):
        if event_type:
            self.wfile.write(f"event: {event_type}\n".encode("utf-8"))
        if event_id:
            self.wfile.write(f"id: {event_id}\n".encode("utf-8"))
        if retry is not None:
            self.wfile.write(f"retry: {retry}\n".encode("utf-8"))
        if data:
            for line in data.split("\n"):
                self.wfile.write(f"data: {line}\n".encode("utf-8"))
        self.wfile.write(b"\n")
        self.wfile.flush()

    def do_GET(self):
        parsed = urlparse(self.path)
        path = parsed.path

        if path == "/events":
            self.send_sse_headers()
            self.send_event(data="Event 1")
            time.sleep(0.1)
            self.send_event(data="Event 2")
            time.sleep(0.1)
            self.send_event(data="Event 3")
            self.log_message("Sent 3 events to /events")

        elif path == "/rapid":
            self.send_sse_headers()
            for i in range(100):
                self.send_event(data=f"Rapid event {i + 1}")
            self.log_message("Sent 100 rapid events")

        elif path == "/error-404":
            self.send_response(404)
            self.send_header("Content-Type", "text/plain")
            self.end_headers()
            self.wfile.write(b"Not Found")
            self.log_message("Sent 404 error")

        elif path == "/wrong-type":
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps({"error": "wrong type"}).encode("utf-8"))
            self.log_message("Sent wrong Content-Type")

        elif path == "/with-id":
            self.send_sse_headers()
            self.send_event(event_id="msg-001", data="First message")
            time.sleep(0.1)
            self.send_event(event_id="msg-002", data="Second message")
            time.sleep(0.1)
            self.send_event(event_id="msg-003", data="Third message")
            self.log_message("Sent events with IDs")

        elif path == "/multiline":
            self.send_sse_headers()
            self.send_event(data="Line 1\nLine 2\nLine 3")
            self.log_message("Sent multiline event")

        elif path == "/retry":
            self.send_sse_headers()
            self.send_event(retry=5000, data="Retry set to 5 seconds")
            time.sleep(0.1)
            self.send_event(data="Another event")
            self.log_message("Sent retry field")

        elif path == "/disconnect":
            self.send_sse_headers()
            self.send_event(data="Event before disconnect 1")
            time.sleep(0.1)
            self.send_event(data="Event before disconnect 2")
            self.log_message("Sent 2 events, closing connection")

        elif path == "/reconnect-test":
            self.send_sse_headers()
            self.send_event(data="Reconnect test event")
            self.log_message("Sent 1 event, closing for reconnect test")

        elif path == "/retry-override":
            self.send_sse_headers()
            self.send_event(retry=500, data="fast")
            self.log_message("Sent retry:500 event, closing")

        elif path == "/events-with-id":
            last_event_id = self.headers.get("Last-Event-ID", None)
            self.send_sse_headers()

            if last_event_id == "100":
                self.send_event(event_id="101", data="resumed")
                self.log_message("Sent resumed event (id:101)")
            else:
                self.send_event(event_id="100", data="first")
                self.log_message("Sent first event (id:100)")

        else:
            self.send_response(404)
            self.send_header("Content-Type", "text/plain")
            self.end_headers()
            self.wfile.write(f"Unknown endpoint: {path}".encode("utf-8"))

    def do_POST(self):
        parsed = urlparse(self.path)
        path = parsed.path

        if path == "/chat":
            content_length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(content_length).decode("utf-8")

            self.log_message("POST /chat received body: %s", body)

            auth_header = self.headers.get("Authorization", "None")
            self.log_message("Authorization header: %s", auth_header)

            self.send_sse_headers()

            for i in range(5):
                self.send_event(event_type="chunk", data=f"Chunk {i + 1}")
                time.sleep(0.05)

            self.log_message("Sent 5 chunk events")

        else:
            self.send_response(404)
            self.send_header("Content-Type", "text/plain")
            self.end_headers()
            self.wfile.write(f"Unknown POST endpoint: {path}".encode("utf-8"))


def run_server():
    with socketserver.TCPServer(("", PORT), SSEHandler) as httpd:
        print(f"Mock SSE Server running on http://localhost:{PORT}")
        print("Available endpoints:")
        print("  GET  /events            - 3 standard events")
        print("  POST /chat              - 5 chunk events (expects JSON body)")
        print("  GET  /rapid             - 100 rapid events")
        print("  GET  /error-404         - 404 error")
        print("  GET  /wrong-type        - Wrong Content-Type")
        print("  GET  /with-id           - Events with id field")
        print("  GET  /multiline         - Multi-line data event")
        print("  GET  /retry             - Retry field test")
        print("  GET  /disconnect        - 2 events then disconnect")
        print("  GET  /reconnect-test    - 1 event then close (auto-reconnect test)")
        print("  GET  /retry-override    - retry:500 event then close")
        print("  GET  /events-with-id    - ID-based resumption (checks Last-Event-ID)")
        print("\nPress Ctrl+C to stop\n")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nShutting down server...")
            sys.exit(0)


if __name__ == "__main__":
    run_server()
