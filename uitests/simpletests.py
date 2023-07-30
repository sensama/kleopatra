#!/usr/bin/env python3

# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2016 Microsoft Corporation. All rights reserved.
# SPDX-FileCopyrightText: 2021-2022 Harald Sitter <sitter@kde.org>

import subprocess
import time
import unittest
from appium import webdriver
from appium.webdriver.common.appiumby import AppiumBy
import selenium.common.exceptions
from selenium.webdriver.support.ui import WebDriverWait
from selenium.webdriver.common.action_chains import ActionChains

class SimpleKleopatraTests(unittest.TestCase):
    @classmethod
    def setUpClass(self):
        desired_caps = {}
        # The app capability may be a command line or a desktop file id.
        desired_caps["app"] = "org.kde.kleopatra.desktop"
        # Boilerplate, always the same
        self.driver = webdriver.Remote(
            command_executor='http://127.0.0.1:4723',
            desired_capabilities=desired_caps)
        # Set a timeout for waiting to find elements. If elements cannot be found
        # in time we'll get a test failure. This should be somewhat long so as to
        # not fall over when the system is under load, but also not too long that
        # the test takes forever.
        self.driver.implicitly_wait = 10

    @classmethod
    def tearDownClass(self):
        # Make sure to terminate the driver again, lest it dangles.
        self.driver.quit()

    def setUp(self):
        # self.driver.find_element(by=AppiumBy.NAME, value="AC").click()
        # wait = WebDriverWait(self.driver, 20)
        # wait.until(lambda x: self.getresults() == '0')
        self.pinentryDriver = None

    def tearDown(self):
        if self.pinentryDriver:
            self.pinentryDriver.quit()

    def getPinentry(self):
        pid = None

        def getPid():
            nonlocal pid

            p = subprocess.run(["pgrep", "pinentry"], capture_output=True)
            pid = p.stdout.decode().split("\n")[0]
            return pid

        wait = WebDriverWait(self.driver, 20)
        try:
            wait.until(lambda x: getPid())
        except selenium.common.exceptions.TimeoutException:
            pass

        print("pid: ", pid)
        desired_caps = {
            "app": pid,
        }
        # Boilerplate, always the same
        self.pinentryDriver = webdriver.Remote(
            command_executor='http://127.0.0.1:4723',
            desired_capabilities=desired_caps)
        # Set a timeout for waiting to find elements. If elements cannot be found
        # in time we'll get a test failure. This should be somewhat long so as to
        # not fall over when the system is under load, but also not too long that
        # the test takes forever.
        self.pinentryDriver.implicitly_wait = 10

    def getresults(self):
        displaytext = self.driver.find_element(by='description', value='Result Display').text
        return displaytext

    def assertResult(self, actual, expected):
        wait = WebDriverWait(self.driver, 20)
        try:
            wait.until(lambda x: self.getresults() == expected)
        except selenium.common.exceptions.TimeoutException:
            pass
        self.assertEqual(self.getresults(), expected)

    @unittest.skip("skipping")
    def test_generateOpenPGPCertificate(self):
#         fileMenu = self.driver.find_element(by=AppiumBy.ACCESSIBILITY_ID, value="KleopatraApplication.MainWindow#1.QMenuBar.file.file")
#         fileMenu.click()
#         
#         self.driver.find_element(by=AppiumBy.ACCESSIBILITY_ID, value="KleopatraApplication.MainWindow#1.QMenuBar.file.file.file_new_certificate").click()
        createButton = self.driver.find_element(by=AppiumBy.NAME, value="New Key Pair")
        createButton.click()

        # generateDialog = self.driver.find_element(by=AppiumBy.ACCESSIBILITY_ID, value="KleopatraApplication.Kleo::NewOpenPGPCertificateDetailsDialog")
        # nameInput = generateDialog.find_element(by=AppiumBy.NAME, value="Name")
        # ^- crashes with
        # b'{"using": "accessibility id", "value": "KleopatraApplication.Kleo::NewOpenPGPCertificateDetailsDialog"}'
        # [dialog | Create OpenPGP Certificate]
        # 127.0.0.1 - - [18/Jul/2023 16:15:27] "POST /session/289a8864-256d-11ee-b6ad-02426f6067c3/element HTTP/1.1" 200 -
        # {'using': 'name', 'value': 'Name', 'id': '-org-a11y-atspi-accessible-2147483873'}
        # [text | Name]
        # [2023-07-18 16:15:27,678] ERROR in app: Exception on /session/289a8864-256d-11ee-b6ad-02426f6067c3/element/-org-a11y-atspi-accessible-2147483873/element [POST]
        # Traceback (most recent call last):
        #   File "/home/ingo/.local/lib/python3.11/site-packages/flask/app.py", line 2525, in wsgi_app
        #     response = self.full_dispatch_request()
        #                ^^^^^^^^^^^^^^^^^^^^^^^^^^^^
        #   File "/home/ingo/.local/lib/python3.11/site-packages/flask/app.py", line 1823, in full_dispatch_request
        #     return self.finalize_request(rv)
        #            ^^^^^^^^^^^^^^^^^^^^^^^^^
        #   File "/home/ingo/.local/lib/python3.11/site-packages/flask/app.py", line 1842, in finalize_request
        #     response = self.make_response(rv)
        #                ^^^^^^^^^^^^^^^^^^^^^^
        #   File "/home/ingo/.local/lib/python3.11/site-packages/flask/app.py", line 2153, in make_response
        #     rv = self.json.response(rv)
        #          ^^^^^^^^^^^^^^^^^^^^^^
        #   File "/home/ingo/.local/lib/python3.11/site-packages/flask/json/provider.py", line 309, in response
        #     f"{self.dumps(obj, **dump_args)}\n", mimetype=mimetype
        #        ^^^^^^^^^^^^^^^^^^^^^^^^^^^^
        #   File "/home/ingo/.local/lib/python3.11/site-packages/flask/json/provider.py", line 230, in dumps
        #     return json.dumps(obj, **kwargs)
        #            ^^^^^^^^^^^^^^^^^^^^^^^^^
        #   File "/usr/lib64/python3.11/json/__init__.py", line 238, in dumps
        #     **kw).encode(obj)
        #           ^^^^^^^^^^^
        #   File "/usr/lib64/python3.11/json/encoder.py", line 200, in encode
        #     chunks = self.iterencode(o, _one_shot=True)
        #              ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
        #   File "/usr/lib64/python3.11/json/encoder.py", line 258, in iterencode
        #     return _iterencode(o, 0)
        #            ^^^^^^^^^^^^^^^^^
        #   File "/home/ingo/.local/lib/python3.11/site-packages/flask/json/provider.py", line 122, in _default
        #     raise TypeError(f"Object of type {type(o).__name__} is not JSON serializable")
        # TypeError: Object of type Accessible is not JSON serializable
        # 127.0.0.1 - - [18/Jul/2023 16:15:27] "POST /session/289a8864-256d-11ee-b6ad-02426f6067c3/element/-org-a11y-atspi-accessible-2147483873/element HTTP/1.1" 500 -
        # E127.0.0.1 - - [18/Jul/2023 16:15:27] "DELETE /session/289a8864-256d-11ee-b6ad-02426f6067c3 HTTP/1.1" 200 -
        # 
        # ======================================================================
        # ERROR: test_generateOpenPGPCertificate (__main__.SimpleKleopatraTests.test_generateOpenPGPCertificate)
        # ----------------------------------------------------------------------
        # Traceback (most recent call last):
        #   File "/home/ingo/dev/kde/kleopatra/uitests/simpletests.py", line 62, in test_generateOpenPGPCertificate
        #     nameInput = generateDialog.find_element(by=AppiumBy.NAME, value="Name")
        #                 ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
        #   File "/home/ingo/.local/lib/python3.11/site-packages/appium/webdriver/webelement.py", line 114, in find_element
        #     return self._execute(RemoteCommand.FIND_CHILD_ELEMENT, {"using": by, "value": value})['value']
        #            ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
        #   File "/home/ingo/.local/lib/python3.11/site-packages/selenium/webdriver/remote/webelement.py", line 396, in _execute
        #     return self._parent.execute(command, params)
        #            ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
        #   File "/home/ingo/.local/lib/python3.11/site-packages/selenium/webdriver/remote/webdriver.py", line 429, in execute
        #     self.error_handler.check_response(response)
        #   File "/home/ingo/.local/lib/python3.11/site-packages/appium/webdriver/errorhandler.py", line 30, in check_response
        #     raise wde
        #   File "/home/ingo/.local/lib/python3.11/site-packages/appium/webdriver/errorhandler.py", line 26, in check_response
        #     super().check_response(response)
        #   File "/home/ingo/.local/lib/python3.11/site-packages/selenium/webdriver/remote/errorhandler.py", line 207, in check_response
        #     raise exception_class(value)

        # generateDialog = self.driver.find_element(by=AppiumBy.XPATH, value='//dialog[@name="Create OpenPGP Certificate"]')
        nameInput = self.driver.find_element(by=AppiumBy.XPATH, value='//dialog[@name="Create OpenPGP Certificate"]//text[@name="Name"]')
        nameInput.click()
        nameInput.clear()
        nameInput.send_keys("Ada Lovelace")
        emailInput = self.driver.find_element(by=AppiumBy.XPATH, value='//dialog[@name="Create OpenPGP Certificate"]//text[@name="Email address"]')
        emailInput.click()
        emailInput.clear()
        emailInput.send_keys("ada@example.net")
        okButton = self.driver.find_element(by=AppiumBy.XPATH, value='//dialog[@name="Create OpenPGP Certificate"]//push_button[@name="OK"]')
        # time.sleep(5)
        okButton.click()
        
        successDialog = self.driver.find_element(by=AppiumBy.CLASS_NAME, value="[dialog | Success]")
        self.driver.find_element(by=AppiumBy.XPATH, value='//dialog[@name="Success"]//push_button[@name="OK"]').click()

        # time.sleep(5)
        # self.assertResult(self.getresults(), "7")

    def test_generateOpenPGPCertificateWithPassword(self):
        kleopatra = self.driver.command_executor

        createButton = self.driver.find_element(by=AppiumBy.NAME, value="New Key Pair")
        createButton.click()

        nameInput = self.driver.find_element(by=AppiumBy.XPATH, value='//dialog[@name="Create OpenPGP Certificate"]//text[@name="Name"]')
        nameInput.click()
        nameInput.clear()
        nameInput.send_keys("Ada Lovelace")
        emailInput = self.driver.find_element(by=AppiumBy.XPATH, value='//dialog[@name="Create OpenPGP Certificate"]//text[@name="Email address"]')
        emailInput.click()
        emailInput.clear()
        emailInput.send_keys("ada@example.net")
        self.driver.find_element(by=AppiumBy.XPATH, value='//dialog[@name="Create OpenPGP Certificate"]//check_box').click()
        okButton = self.driver.find_element(by=AppiumBy.XPATH, value='//dialog[@name="Create OpenPGP Certificate"]//push_button[@name="OK"]')
        okButton.click()

        self.getPinentry()
        passwordInput = self.pinentryDriver.find_element(by=AppiumBy.XPATH, value='//password_text[@name="Passphrase:"]')
        passwordInput.click()
        passwordInput.send_keys("testtest1")
        passwordInput = self.pinentryDriver.find_element(by=AppiumBy.XPATH, value='//password_text[@name="Repeat:"]')
        passwordInput.click()
        passwordInput.send_keys("testtest1")
        self.pinentryDriver.find_element(by=AppiumBy.NAME, value="OK").click()

        # self.pinentryDriver.find_element(by=AppiumBy.NAME, value="Cancel").click()

        successDialog = self.driver.find_element(by=AppiumBy.CLASS_NAME, value="[dialog | Success]")
        self.driver.find_element(by=AppiumBy.XPATH, value='//dialog[@name="Success"]//push_button[@name="OK"]').click()

if __name__ == '__main__':
    unittest.main()
