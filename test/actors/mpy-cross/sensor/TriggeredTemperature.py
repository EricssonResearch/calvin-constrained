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

from calvin.actor.actor import Actor, manage, condition, stateguard, calvinsys

class TriggeredTemperature(Actor):

    """
    Measure temperature.

    Inputs:
        trigger : any token triggers meausurement

    Outputs:
        centigrade :  temperature, in centigrade
    """

    @manage(['temperature'])
    def init(self):
        self.temperature = calvinsys.open(self, "io.temperature")

    @stateguard(lambda self: calvinsys.can_read(self.temperature))
    @condition(['trigger'], ['centigrade'])
    def read_measurement(self, _):
        data = calvinsys.read(self.temperature)
        return (data,)

    action_priority = (read_measurement,)
    requires = ['io.temperature']
