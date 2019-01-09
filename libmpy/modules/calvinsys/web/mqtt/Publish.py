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


class Publish(base_calvinsys_object.BaseCalvinsysObject):
    """
    Publish data to given MQTT broker
    """

    def init(self, hostname, port=1883, qos=0, client_id='', will=None, auth=None, tls=None, transport='tcp', topic=None, **kwargs):
        self.user = None
        self.password = None
        self.ssl = False
        self.ssl_params = {}
        self.topic = topic

        if auth:
            user = auth.get("username")
            password = auth.get("password")

        if tls:
            self.ssl = True
            key_file = open(tls.get("keyfile"), "r")
            key = key_file.read()
            cert_file = open(tls.get("certfile"), "r")
            cert = cert_file.read()
            self.ssl_params = {"key": key, "cert": cert}

        self.c = MQTTClient(client_id, hostname, port=port, user=self.user, password=self.password, ssl=self.ssl, ssl_params=self.ssl_params)
        self.c.connect()

    def can_write(self):
        return True

    def write(self, data):
        topic = None
        payload = None

        if isinstance(data, dict):
            payload = data.get("payload")
            topic = data.get("topic")

            if self.topic and topic:
                topic = "{}{}".format(self.topic, topic)
            elif self.topic :
                topic = self.topic
        else:
            payload = data
            topic = self.topic

        assert topic is not None

        self.c.publish(topic, str(payload))

    def close(self):
        self.c.disconnect()
        self.c = None
