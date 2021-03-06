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

from calvin.actor.actor import Actor, manage, condition

class ReplicaIdentity(Actor):
    """
    forward a token with replica index prepended
    Inputs:
      token : a token
    Outputs:
      token : the prepended token
    """
    @manage(['dump', 'index'])
    def init(self, dump=False):
        self.dump = dump
        self.index = 0

    def log(self, data):
        print("%s<%s,%s>: %s" % (self.__class__.__name__, self.name, self.id, data))

    @condition(['token'], ['token'])
    def prepend(self, input):
        output = "{}:{}".format(self.index, input)
        if self.dump:
            self.log(output)
        return (output, )

    def did_replicate(self, index):
        self.index = index

    action_priority = (prepend, )

    test_set = [
        {
            'inports': {'token': [1, 2, 3]},
            'outports': {'token': ["0:1", "0:2", "0:3"]}
        }
    ]
