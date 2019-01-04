# -*- coding: utf-8 -*-

# Copyright (c) 2017 Ericsson AB
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

class Stdout(base_calvinsys_object.BaseCalvinsysObject):
    """
    Print
    """

    def init(self, prefix=None, **kwargs):
        self._prefix = prefix

    def can_write(self):
        return True

    def write(self, data):
        msg = ""
        if data is not None and self._prefix is not None:
            msg = u"{}: {}".format(self._prefix, data)
        elif data is not None:
            msg = u"{}".format(data)
        elif self._prefix is not None:
            msg = u"{}".format(self._prefix)
        print(msg)

    def close(self):
        pass
