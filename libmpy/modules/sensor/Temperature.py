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

class Temperature(Actor):

    """
    Measure temperature. Takes the frequency of measurements, in Hz, as input.

    Outputs:
        centigrade :  temperature, in centigrade
    """

    @manage(['frequency', 'timer', 'temperature'])
    def init(self, frequency):
        self.frequency = frequency
        self.temperature = calvinsys.open(self, "io.temperature")
        self.timer = calvinsys.open(self, "sys.timer.once")
        calvinsys.write(self.timer, 1/self.frequency)

    @stateguard(lambda self: self.timer and calvinsys.can_read(self.timer))
    @condition([], ['centigrade'])
    def read_measurement(self):
        calvinsys.read(self.timer)
        calvinsys.write(self.timer, True)
        return (calvinsys.read(self.temperature),)

    action_priority = (read_measurement, )
    requires =  ['io.temperature', 'sys.timer.once']
