"""Run with: python -m unittest discover -s test -p config_webpage_test.py

Requires Playwright for Python and its Chromium browser.
"""

from pathlib import Path
import re
import unittest

from playwright.sync_api import expect, sync_playwright


SOURCE = Path(__file__).resolve().parents[1] / "src" / "ConfigurationWebServer.cpp"


def config_html(state="On", brightness=255):
    source = SOURCE.read_text(encoding="utf-8")
    html = source.split('CONFIG_HTML[] PROGMEM = R"(', 1)[1].split(')";', 1)[0]
    values = {
        "LATITUDE": "51.5",
        "LONGITUDE": "-0.1",
        "RADIUS": "1",
        "ROTATION": "0",
        "MAX_INFO_FIELDS": "3",
        "DISPLAY_STATE": state,
        "DISPLAY_BRIGHTNESS": str(brightness),
    }
    return re.sub(r"%([A-Z_]+)%", lambda match: values.get(match[1], ""), html)


class DisplayControlsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.playwright = sync_playwright().start()
        cls.browser = cls.playwright.chromium.launch()

    @classmethod
    def tearDownClass(cls):
        cls.browser.close()
        cls.playwright.stop()

    def setUp(self):
        self.context = self.browser.new_context()
        self.page = self.context.new_page()
        self.requests = []
        self.pending = []
        self.response_status = 202
        self.response_body = "Command accepted."
        self.network_failure = False
        self.hold_response = False
        self.html = config_html()
        self.page.route("https://cdn.jsdelivr.net/**", lambda route: route.fulfill(body=""))
        self.page.route("http://microradar.test/**", self.handle_request)
        self.page.goto("http://microradar.test/")

    def tearDown(self):
        self.context.close()

    def handle_request(self, route):
        request = route.request
        if request.url == "http://microradar.test/":
            route.fulfill(content_type="text/html", body=self.html)
            return
        self.requests.append((request.method, request.url, request.post_data))
        if self.network_failure:
            route.abort()
        elif self.hold_response:
            self.pending.append(route)
        else:
            route.fulfill(
                status=self.response_status,
                content_type="text/plain",
                body=self.response_body,
            )

    def assert_accepted(self):
        expect(self.page.locator("#display-result")).to_contain_text("Command accepted.")
        expect(self.page.locator("#display-on")).to_be_enabled()

    def test_power_commands_do_not_submit_configuration(self):
        self.page.get_by_role("button", name="Turn off", exact=True).click()
        self.assert_accepted()
        expect(self.page.locator("#display-state")).to_have_text("Off")
        self.page.get_by_role("button", name="Turn on", exact=True).click()
        self.assert_accepted()
        expect(self.page.locator("#display-state")).to_have_text("On")
        self.assertEqual(self.requests, [
            ("PUT", "http://microradar.test/api/display/off", None),
            ("PUT", "http://microradar.test/api/display/on", None),
        ])
        self.assertNotEqual(
            self.page.locator("#display-brightness").evaluate("(input) => input.form.id"),
            "cfg",
        )

    def test_brightness_boundaries_do_not_wake_display(self):
        self.html = config_html("Off", 96)
        self.page.reload()
        expect(self.page.locator("#display-state")).to_have_text("Off")
        expect(self.page.locator("#display-brightness")).to_have_value("96")
        for value in ("0", "128", "255"):
            self.page.locator("#display-brightness").fill(value)
            self.page.get_by_role("button", name="Apply brightness").click()
            self.assert_accepted()
            expect(self.page.locator("#display-state")).to_have_text("Off")
        self.assertEqual([url for _, url, _ in self.requests], [
            "http://microradar.test/api/display/brightness?value=0",
            "http://microradar.test/api/display/brightness?value=128",
            "http://microradar.test/api/display/brightness?value=255",
        ])

    def test_invalid_brightness_is_not_sent(self):
        for value in ("", "-1", "256", "1.5"):
            self.page.locator("#display-brightness").fill(value)
            self.page.get_by_role("button", name="Apply brightness").click()
            self.assertFalse(self.page.locator("#display-brightness").evaluate(
                "(input) => input.validity.valid"
            ))
        self.assertEqual(self.requests, [])

    def test_http_error_preserves_requested_state(self):
        self.response_status = 400
        self.response_body = "Rejected <brightness>."
        self.page.get_by_role("button", name="Turn off", exact=True).click()
        expect(self.page.locator("#display-result")).to_contain_text("Rejected <brightness>.")
        expect(self.page.locator("#display-state")).to_have_text("On")
        expect(self.page.locator("#display-off")).to_be_enabled()
        self.assertEqual(self.page.locator("#display-result brightness").count(), 0)

    def test_network_error_allows_retry(self):
        self.network_failure = True
        self.page.get_by_role("button", name="Turn off", exact=True).click()
        expect(self.page.locator("#display-result")).to_contain_text("Could not update display.")
        expect(self.page.locator("#display-state")).to_have_text("On")
        expect(self.page.locator("#display-off")).to_be_enabled()
        self.network_failure = False
        self.page.get_by_role("button", name="Turn off", exact=True).click()
        self.assert_accepted()
        expect(self.page.locator("#display-state")).to_have_text("Off")

    def test_pending_command_disables_live_controls_only(self):
        self.hold_response = True
        self.page.get_by_role("button", name="Turn off", exact=True).click()
        expect(self.page.locator("#display-on")).to_be_disabled()
        expect(self.page.locator("#display-off")).to_be_disabled()
        expect(self.page.locator("#display-brightness")).to_be_disabled()
        expect(self.page.get_by_role("button", name="Save", exact=True)).to_be_enabled()
        expect(self.page.locator("#display-controls")).to_have_attribute("aria-busy", "true")
        self.pending[0].fulfill(status=202, body="Command accepted.")
        self.assert_accepted()
        expect(self.page.locator("#display-controls")).not_to_have_attribute("aria-busy")

    def test_configuration_save_still_uses_its_own_form(self):
        self.response_status = 200
        self.response_body = "Saved - restarting device..."
        self.page.get_by_role("button", name="Save", exact=True).click()
        expect(self.page.locator("#result")).to_have_text(self.response_body)
        self.assertEqual(len(self.requests), 1)
        method, url, body = self.requests[0]
        self.assertEqual((method, url), ("POST", "http://microradar.test/save"))
        self.assertIn('name="rotation"', body)
        self.assertNotIn("display-brightness", body)


if __name__ == "__main__":
    unittest.main()
