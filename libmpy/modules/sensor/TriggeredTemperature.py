# -*- coding: utf-8 -*-

# Copyright (c) 2015 Ericsson AB
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

from calvin.actor.actor import Actor, manage, stateguard, condition, calvinsys


class TriggeredTemperature(Actor):

    """
    Read temperature when told to
    Inputs:
        trigger: Triggers a temperature reading
    Outputs:
        centigrade: The measured temperature in centigrade
    """

    @manage([])
    def init(self):
        self.temp = None
        self.setup()

    def setup(self):
        self.temp = calvinsys.open(self, "io.temperature")

    def will_migrate(self):
        calvinsys.close(self.temp)

    def will_end(self):
        calvinsys.close(self.temp)

    def did_migrate(self):
        self.setup()

    @stateguard(lambda self: self.temp and calvinsys.can_read(self.temp))
    @condition(['trigger'], ['centigrade'])
    def measure(self, _):
        return (calvinsys.read(self.temp),)

    action_priority = (measure,)
    requires =  ['io.temperature']
