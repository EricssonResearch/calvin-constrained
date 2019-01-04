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

from calvin.actor.actor import Actor, manage, condition, stateguard, calvinsys

class HTTPGet(Actor):
    """
    Get contents of URL

    Input:
      URL : URL to get
      params : dictionary with query parameters (optional)
      headers: dictionary with headers to include in request (optional)
      auth : dictionary with authtype (basic/digest), username and password (optional)
    Output:
      status: HTTP status of request
      headers: dictionary of response headers
      data : body of response (only if body is non-empty)
    """

    @manage()
    def init(self):
        self.cmd = calvinsys.open(self, 'http.get', cmd='GET')
        self.response = None

    @stateguard(lambda self: calvinsys.can_write(self.cmd))
    @condition(action_input=['URL', 'params', 'headers', 'auth'])
    def new_request(self, url, params, headers, auth):
        calvinsys.write(self.cmd, {"url": url, "params": params, "headers": headers, "auth": auth})

    @stateguard(lambda self: calvinsys.can_read(self.cmd))
    @condition()
    def handle_reply(self):
        self.response = calvinsys.read(self.cmd)

    @stateguard(lambda self: self.response and self.response.get("body"))
    @condition(action_output=['status', 'headers', 'data'])
    def reply_with_body(self):
        response = self.response
        self.response = None
        return (response.get("status"), response.get("headers"), response.get("body"))

    @stateguard(lambda self: self.response and not self.response.get("body"))
    @condition(action_output=['status', 'headers'])
    def reply_without_body(self):
        response = self.response
        self.response = None
        return (response.get("status"), response.get("headers"))

    action_priority = (new_request, handle_reply, reply_with_body, reply_without_body)
    requires = ['http.get']
