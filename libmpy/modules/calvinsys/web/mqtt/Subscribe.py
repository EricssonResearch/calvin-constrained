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

from calvin.runtime.south.calvinsys import base_calvinsys_object
from calvin.utilities.calvinlogger import get_logger
from mqttclient import MQTTClient

_log = get_logger(__name__)

class Subscribe(base_calvinsys_object.BaseCalvinsysObject):
    """
    Subscribe to data on given MQTT broker
    """

    def init(self, topics, hostname, port=1883, qos=0, client_id='cc_client', will=None, auth=None, tls=None, transport='tcp', payload_only=False, **kwargs):
        def sub_cb(topic, msg):
            self.data.append({"topic": topic.decode('utf-8'), "payload": msg})

        self.data = []
        self.payload_only = payload_only
        self.topics = topics
        self.user = None
        self.password = None
        if auth:
            user = auth.get("username")
            password = auth.get("password")

        self.c = MQTTClient(client_id, hostname, port=port, user=self.user, password=self.password)
        self.c.set_callback(sub_cb)
        self.c.connect()
        for topic in self.topics:
            self.c.subscribe(topic.encode("ascii"))

    def can_write(self):
        return True

    def write(self, data=None):
        pass

    def can_read(self):
        try:
            data = self.c.check_msg()
        except:
            return False
        return bool(self.data)

    def read(self):
        data = self.data.pop(0)
        if self.payload_only:
            return data.get("payload")
        else :
            return data

    def close(self):
        self.c.disconnect()
