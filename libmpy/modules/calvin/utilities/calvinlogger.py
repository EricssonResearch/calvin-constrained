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

critical = 50
error    = 40
warning  = 30
info     = 20
debug    = 10
notset   = 0

_level_dict = {
    critical: "CRIT",
    error: "ERROR",
    warning: "WARN",
    info: "INFO",
    debug: "DEBUG",
}

_log = None

class Logger:

    def _level_str(self, level):
        l = _level_dict.get(level)
        if l is not None:
            return l
        return "%s" % level

    def log(self, level, msg, *args):
        print("{}: {}".format(self._level_str(level), msg % args))

    def debug(self, msg, *args):
        self.log(debug, msg, *args)

    def info(self, msg, *args):
        self.log(info, msg, *args)

    def warning(self, msg, *args):
        self.log(warning, msg, *args)

    def error(self, msg, *args):
        self.log(error, msg, *args)

    def critical(self, msg, *args):
        self.log(critical, msg, *args)


def _create_logger(filename=None):
    global _log
    if _log is None:
        _log = Logger("calvin")
    return _log

def get_logger(name=None):
    log = _create_logger()
    return log

def get_actor_logger(name):
    log = _create_logger()
    return log
