# -*- coding: utf-8 -*-

# Copyright (c) 2015-2016 Ericsson AB
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

import functools
import cc_mp_port
import cc_mp_calvinsys

def manage(include=None, exclude=None):
    if include and type(include) is not list or exclude and type(exclude) is not list:
        raise Exception("@manage decorator: Must use list as argument")

    include = set(include) if include else set()
    exclude = set(exclude) if exclude else set()

    def wrap(f):
        @functools.wraps(f)
        def manage_wrapper(instance, *args, **kwargs):
            exclude.update(instance.__dict__)
            r = f(instance, *args, **kwargs)
            if not include:
                # include set not given, so construct the implicit include set
                include.update(instance.__dict__)
                include.difference_update(exclude)
            instance._managed = list(include)
            return r
        return manage_wrapper
    return wrap

def condition(action_input=[], action_output=[]):
    """
    Decorator condition specifies the required input data and output space.
    Both parameters are lists of port names
    Return value is a tuple (did_fire, output_available, exhaust_list)
    """
    tokens_produced = len(action_output)
    tokens_consumed = len(action_input)

    def wrap(action_method):

        @functools.wraps(action_method)
        def condition_wrapper(self):
            # Check if input ports have enough tokens
            input_ok = all(cc_mp_port.ccmp_tokens_available(self.actor_ref, portname, 1) for portname in action_input)

            # Check if output port have enough free token slots
            output_ok = all(cc_mp_port.ccmp_slots_available(self.actor_ref, portname, 1) for portname in action_output)

            if not input_ok or not output_ok:
                return (False, output_ok, ())

            # Build the arguments for the action from the input port(s)
            # TODO: Handle exception tokens
            args = []
            for portname in action_input:
                value = cc_mp_port.ccmp_peek_token(self.actor_ref, portname)
                args.append(value)

            # Perform the action (N.B. the method may be wrapped in a guard)
            production = action_method(self, *args)
            if production is None:
                production = []

            valid_production = (tokens_produced == len(production))

            exhausted_ports = set()
            if valid_production:
                for portname in action_input:
                    exhausted = cc_mp_port.ccmp_peek_commit(self.actor_ref, portname)
                    if exhausted:
                        exhausted_ports.add(portname)
            else:
                for portname in action_input:
                    cc_mp_port.ccmp_peek_cancel(self.actor_ref, portname)
                raise Exception("Failed to execute %s, invalid production", action_input)

            for portname, retval in zip(action_output, production):
                cc_mp_port.ccmp_write_token(self.actor_ref, portname, retval)

            return (True, True, exhausted_ports)
        return condition_wrapper
    return wrap

def stateguard(action_guard):
    """
    Decorator guard refines the criteria for picking an action to run by stating a function
    with THE SAME signature as the guarded action returning a boolean (True if action allowed).
    If the speciified function is unbound or a lambda expression, you must account for 'self',
    e.g. 'lambda self, a, b: a>0'
    """

    def wrap(action_method):

        @functools.wraps(action_method)
        def guard_wrapper(self, *args):
            if not action_guard(self):
                return (False, True, ())
            return action_method(self, *args)

        return guard_wrapper
    return wrap


calvinsys_ref = None

def set_calvinsys(ref):
    global calvinsys_ref
    if calvinsys_ref is None:
        calvinsys_ref = ref

def get_calvinsys():
    global calvinsys_ref
    return calvinsys_ref

class calvinsys(object):

    """
    Calvinsys interface exposed to actors
    """

    @staticmethod
    def open(actor, name, **kwargs):
        return cc_mp_calvinsys.open(actor.actor_ref, name, **kwargs)

    @staticmethod
    def can_write(obj):
        try:
            data = cc_mp_calvinsys.can_write(get_calvinsys(), obj)
        except Exception as e:
            _log.exception("'can_write' failed, exception={}".format(e))
        return data

    @staticmethod
    def write(obj, data):
        try:
            cc_mp_calvinsys.write(get_calvinsys(), obj, data)
        except Exception as e:
            _log.exception("'write()' failed, exception={}".format(e))

    @staticmethod
    def can_read(obj):
        try:
            data = cc_mp_calvinsys.can_read(get_calvinsys(), obj)
        except Exception as e:
            _log.exception("'can_read()' failed, exception={}".format(e))
        return data

    @staticmethod
    def read(obj):
        try:
            data = cc_mp_calvinsys.read(get_calvinsys(), obj)
        except Exception as e:
            _log.exception("'read()' failed, exception={}".format(e))
        return data

    @staticmethod
    def close(obj):
        cc_mp_calvinsys.close(get_calvinsys(), obj)

class Actor(object):
    """
    Base class for all actors
    """

    # Class variable controls action priority order
    action_priority = tuple()

    def __init__(self, actor_ref, calvinsys_ref):
        """Should _not_ be overridden in subclasses."""
        super(Actor, self).__init__()
        self.actor_ref = actor_ref
        set_calvinsys(calvinsys_ref)
        self._managed = []

    def init(self):
        raise Exception("Implementing 'init()' is mandatory.")

    def fire(self):
        """
        Fire an actor.
        Returns True if any action fired
        """
        actor_did_fire = False
        done = False
        while not done:
            for action_method in self.__class__.action_priority:
                did_fire, output_ok, exhausted = action_method(self)
                actor_did_fire |= did_fire
                if did_fire:
                    break
            if not did_fire:
                done = True

        return actor_did_fire

    def _managed_state(self):
        """
        Serialize managed state.
        Managed state can only contain objects that can be JSON-serialized.
        """
        state = {key: self.__dict__[key] for key in self._managed}
        return state

    def exception_handler(self, action, args, context):
        """Defult handler when encountering ExceptionTokens"""
        raise Exception("ExceptionToken NOT HANDLED")
