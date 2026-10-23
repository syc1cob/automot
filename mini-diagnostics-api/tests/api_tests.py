#!/usr/bin/env python3
"""Black-box HTTP tests for the Mini Diagnostics API."""

import json
import socket
import subprocess
import sys
import time
import unittest
import urllib.error
import urllib.request


class ApiIntegrationTests(unittest.TestCase):
    def setUp(self):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.bind(("127.0.0.1", 0))
            self.port = sock.getsockname()[1]

        self.base_url = f"http://127.0.0.1:{self.port}/api/v1/ecus"
        self.server = subprocess.Popen(
            [sys.argv[1], str(self.port)],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            if self.server.poll() is not None:
                self.fail("API server exited before it became ready")
            try:
                self.request("GET", "")
                break
            except urllib.error.URLError:
                time.sleep(0.05)
        else:
            self.stop_server()
            self.fail("API server did not become ready")

    def stop_server(self):
        if self.server.poll() is None:
            self.server.terminate()
            try:
                self.server.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.server.kill()
                self.server.wait()

    def tearDown(self):
        self.stop_server()

    def request(self, method, path, body=None):
        data = None if body is None else json.dumps(body).encode("utf-8")
        headers = {} if data is None else {"Content-Type": "application/json"}
        request = urllib.request.Request(
            self.base_url + path, data=data, headers=headers, method=method
        )
        try:
            response = urllib.request.urlopen(request, timeout=2)
        except urllib.error.HTTPError as error:
            response = error
        with response:
            response_body = response.read()
            payload = json.loads(response_body) if response_body else None
            return response.code, response.headers, payload

    @staticmethod
    def ecu(ecu_id="brake", name="Brake Controller", status="online"):
        return {
            "id": ecu_id,
            "name": name,
            "status": status,
            "firmwareVersion": "1.0.0",
        }

    def test_list_returns_initial_ecus(self):
        status, _, payload = self.request("GET", "")
        self.assertEqual(status, 200)
        self.assertEqual({ecu["id"] for ecu in payload}, {"engine", "battery"})

    def test_get_existing_and_missing_ecu(self):
        status, _, payload = self.request("GET", "/engine")
        self.assertEqual(status, 200)
        self.assertEqual(payload["id"], "engine")

        status, _, payload = self.request("GET", "/not-real")
        self.assertEqual(status, 404)
        self.assertEqual(payload, {"error": "ECU not found"})

    def test_post_creates_ecu_and_duplicate_returns_conflict(self):
        ecu = self.ecu()
        status, headers, payload = self.request("POST", "", ecu)
        self.assertEqual(status, 201)
        self.assertEqual(payload, ecu)
        self.assertEqual(headers.get("Location"), "/api/v1/ecus/brake")

        status, _, payload = self.request("GET", "/brake")
        self.assertEqual(status, 200)
        self.assertEqual(payload, ecu)

        status, _, payload = self.request("POST", "", ecu)
        self.assertEqual(status, 409)
        self.assertEqual(payload, {"error": "ECU ID already exists"})

    def test_post_rejects_invalid_resources(self):
        invalid_resources = [
            {"name": "Brake Controller", "status": "online"},
            {"id": "brake", "name": "", "status": "online"},
            self.ecu(ecu_id="bad/id"),
            self.ecu(status="unknown-status"),
        ]
        for resource in invalid_resources:
            with self.subTest(resource=resource):
                status, _, payload = self.request("POST", "", resource)
                self.assertEqual(status, 400)
                self.assertIn("error", payload)

    def test_put_replaces_existing_ecu(self):
        replacement = self.ecu("engine", "Updated Engine Controller", "offline")
        status, _, payload = self.request("PUT", "/engine", replacement)
        self.assertEqual(status, 200)
        self.assertEqual(payload, replacement)

        status, _, payload = self.request("GET", "/engine")
        self.assertEqual(status, 200)
        self.assertEqual(payload, replacement)

    def test_put_unknown_ecu_returns_not_found(self):
        status, _, payload = self.request("PUT", "/not-real", self.ecu("not-real"))
        self.assertEqual(status, 404)
        self.assertEqual(payload, {"error": "ECU not found"})

    def test_put_rejects_invalid_resource_or_mismatched_id(self):
        status, _, payload = self.request("PUT", "/engine", self.ecu("battery"))
        self.assertEqual(status, 400)
        self.assertEqual(payload, {"error": "body id must match path id"})

        invalid = self.ecu("engine", status="unknown-status")
        status, _, payload = self.request("PUT", "/engine", invalid)
        self.assertEqual(status, 400)
        self.assertIn("error", payload)

    def test_delete_removes_existing_ecu(self):
        status, _, payload = self.request("DELETE", "/engine")
        self.assertEqual(status, 204)
        self.assertIsNone(payload)

        status, _, payload = self.request("GET", "/engine")
        self.assertEqual(status, 404)
        self.assertEqual(payload, {"error": "ECU not found"})

        status, _, payload = self.request("GET", "")
        self.assertEqual(status, 200)
        self.assertEqual({ecu["id"] for ecu in payload}, {"battery"})

    def test_delete_unknown_ecu_returns_not_found(self):
        status, _, payload = self.request("DELETE", "/not-real")
        self.assertEqual(status, 404)
        self.assertEqual(payload, {"error": "ECU not found"})


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]], verbosity=2)