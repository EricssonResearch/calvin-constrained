# -*- coding: utf-8 -*-

# Copyright (c) 2018 Ericsson AB
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

try:
    import urequests as requests
except ImportError:
    import requests

from calvin.runtime.south.calvinsys import base_calvinsys_object

class Command(base_calvinsys_object.BaseCalvinsysObject):
    """
    Command - Execute HTTP Command, returning result
    """

    def init(self, cmd, url=None, data=None, timeout=5.0, auth=None, headers=None, params=None, onlydata=None, nodata=False):

        self.settings = {
            "headers": headers or {},
            "params": params or {},
            "auth": auth,
            "timeout": timeout,
            "nodata": nodata,
            "onlydata": onlydata
        }
        self.command = {"POST": requests.post, "PUT": requests.put, "DELETE": requests.delete, "GET": requests.get}.get(cmd, None)
        self.url = url
        self.data = data

        self.result = None

    def can_write(self):
        return not self.result

    def write(self, data=None):
        url = data.get("url")
        response = self.command(url)

        self.result = {"body": response.text, "status": response.status_code }

    def can_read(self):
        return self.result is not None

    def read(self):
        result = self.result
        self.result = None
        return result

    def close(self):
        pass

    def serialize(self):
        return {"command": self.command, "url": self.url, "data": self.data, "result": self.result}
