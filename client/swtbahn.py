#!/usr/bin/env python3

"""

Copyright (C) 2025 University of Bamberg, Software Technologies Research Group
<https://www.uni-bamberg.de/>, <http://www.swt-bamberg.de/>

This file is part of the SWTbahn command line interface (swtbahn-cli), which is
a client-server application to interactively control a BiDiB model railway.

swtbahn-cli is licensed under the GNU GENERAL PUBLIC LICENSE (Version 3), see
the LICENSE file at the project's top-level directory for details or consult
<http://www.gnu.org/licenses/>.

swtbahn-cli is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or any later version.

swtbahn-cli is a RESEARCH PROTOTYPE and distributed WITHOUT ANY WARRANTY, without
even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

The following people contributed to the conception and realization of the
present swtbahn-cli (in alphabetic order by surname):

- Nicolas Gross <https://github.com/nicolasgross>
- Bernhard Luedtke

"""

import click, yaml, json, requests



# ------------------------
# --- global variables ---
# ------------------------

config_file = "swtbahn-config.yml"
hostname = ""
default_track_output = ""
port = 0
session_id = 0
grab_id = 0
server = ""

# -----------------------
# --- config handling ---
# -----------------------

def create_config(hostname, port, default_output, do_test_connection=True):
    try:
        data = {'hostname': hostname, 'port': port,
                'default_track_output': default_output, 'session_id': 0,
                'grab_id': -1}
        with open(config_file, 'w') as outfile:
            yaml.dump(data, outfile, default_flow_style=False)
        click.echo("Config saved")
    except Exception as e:
        click.echo(e, err=True)
        return
    if do_test_connection:
        global server
        server = "http://" + hostname + ":" + str(port)
        click.echo("Connection test:")
        try:
            response = requests.get(server)
            if (response.status_code != 200):
                click.echo("No SWTbahn server on " + server, err=True)
            else:
                click.echo("SWTbahn server found on " + server)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


def update_session_and_grab_id(session_id, grab_id):
    global config_file
    try:
        with open(config_file) as infile:
            config = yaml.safe_load(infile)
        config['session_id'] = session_id
        config['grab_id'] = grab_id
        with open(config_file, 'w') as outfile:
            yaml.dump(config, outfile, default_flow_style=False)
    except Exception as e:
        click.echo(e, err=True)


# Returns True if config is valid, otherwise returns False
def parse_config():
    global config_file, hostname, port, default_track_output, session_id, grab_id, server
    try:
        with open(config_file) as infile:
            config = yaml.safe_load(infile)
        hostname = config['hostname']
        port = config['port']
        default_track_output = config['default_track_output']
        session_id = config['session_id']
        grab_id = config['grab_id']
        server = "http://" + hostname + ":" + str(port)
        return True
    except Exception as e:
        return False

# -----------------------
# --- logging helpers ---
# -----------------------

def log_common_feedback_err_msg(response_text:str, internal_err=False):
    response_json: dict = json.loads(response_text)
    if (internal_err):
        click.echo("Internal error, server msg: " + response_json.get("msg", ""), err=True)
    else:
        click.echo("Error, server msg: " + response_json.get("msg", ""), err=True)

def log_not_running_or_unknown_err(status_code:int):
    if (status_code == 503):
        click.echo("System is not running.", err=True)
    else:
        click.echo(f"Unknown error, reply status code: {status_code}", err=True)

# ------------------------------
# --- command line interface ---
# ------------------------------


@click.group(help="Command-line interface for controlling the SWTbahn. "
             "Use the config command first to set the hostname and port."
             "Use the reset_grab_and_session_id command if the server says they are invalid, "
             "this may happen if an admin force-releases a train you've grabbed.")
def cli():
    pass


@click.command(help="Configure hostname, port, and track output")
@click.argument('hostname', required=True)
@click.argument('port', type=click.IntRange(0, 65535), required=True)
@click.argument('default_track_output', default="master")
def config(hostname, port, default_track_output):
    create_config(hostname, port, default_track_output)


@click.command(help="Reset your train ownership (grab and session id). "
               "Do NOT do this if you still have proper control over the train!")
def reset_grab_and_session_id():
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server, session_id, grab_id
        try:
            # to be safe, try to release the train with the current grab id before resetting it
            requests.post(server + "/driver/release-train",
                          data = {'session-id': session_id, 'grab-id': grab_id})
            # Reset the session_id and grab_id
            update_session_and_grab_id(0, -1)
        except Exception as e:
            click.echo(f"reset_grab_and_session_id failed. Error: {e}", err=True)


# --- admin ---

@click.group(help="Admin functionality")
def admin():
    pass


@click.command(help="Start up the SWTbahn")
def startup():
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.post(server + "/admin/startup")
            if (response.status_code == 200):
                click.echo("System has started up.")
            elif (response.status_code == 409):
                click.echo("System already running!")
            elif (response.status_code == 500):
                log_common_feedback_err_msg(response.text, internal_err=True)
            else:
                click.echo(f"Unknown error, reply status code: {response.status_code}", err=True)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Shut down the SWTbahn")
def shutdown():
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.post(server + "/admin/shutdown")
            if (response.status_code == 200):
                click.echo("System has been stopped.")
                update_session_and_grab_id(0, -1)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Set the power state of all track outputs")
@click.argument('state', type=click.Choice(['off', 'stop', 'soft_stop', 'go']))
def set_track_output(state):
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            mapping = {
                    'off': 0,
                    'stop': 1,
                    'soft_stop': 2,
                    'go': 3
            }
            response = requests.post(server + "/admin/set-track-output",
                                     data = {'state': mapping.get(state)})
            if (response.status_code == 200):
                click.echo(f"Track output state set to {state}.")
            elif (response.status_code == 400):
                log_common_feedback_err_msg(response.text)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Set whether plugins will be verified upon upload")
@click.argument('verification-option', type=click.Choice(['true', 'false']))
def set_verification_option(verification_option):
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.post(server + "/admin/set-verification-option",
                                     data = {'verification-option': verification_option})
            if (response.status_code == 200):
                click.echo(f"Verification option set to {verification_option}.")
            elif (response.status_code == 400):
                log_common_feedback_err_msg(response.text)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Set the url of the verification server to use for verification on upload")
@click.argument('verification-url', type=click.STRING)
def set_verification_url(verification_url):
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.post(server + "/admin/set-verification-url",
                                     data = {'verification-url': verification_url})
            if (response.status_code == 200):
                click.echo(f"Verification url set to {verification_url}.")
            elif (response.status_code == 400):
                log_common_feedback_err_msg(response.text)
            else:
                click.echo(f"Unknown error, reply status code: {response.status_code}", err=True)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(name="release", help="Release a train")
@click.argument('train')
def admin_release(train):
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.post(server + "/admin/release-train", data = {'train': train})
            if (response.status_code == 200):
                click.echo(f"Released grabbed train {train}.")
            elif (response.status_code == 400):
                log_common_feedback_err_msg(response.text)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(name="set-dcc-speed", help="Set the dcc speed step of a train")
@click.option('--backwards', '-b', help="The rear end of the train moves forward", is_flag=True)
@click.option('--track_output', '-t', help="Use another track output than the default one", default="")
@click.argument('train')
@click.argument('speed', type=click.IntRange(0, 126))
def admin_set_dcc_speed(train, backwards, speed, track_output):
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server, default_track_output
        if (backwards and speed > 0):
            speed *= -1
        if (track_output == ""):
            track_output = default_track_output
        try:
            response = requests.post(server + "/admin/set-dcc-train-speed",
                                     data = {'train': train, 'speed': speed, 
                                             'track-output': default_track_output})
            if (response.status_code == 200):
                click.echo(f"DCC speed of {train} set to {speed}.")
            elif (response.status_code == 400):
                log_common_feedback_err_msg(response.text)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


# --- controller ---

@click.group(help="Controller functionality")
def controller():
    pass


@click.command(help="Release a route")
@click.argument('route-id', type=int)
def release_route(route_id):
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        try:
            response = requests.post(server + "/controller/release-route",
                                     data = {'route-id': route_id})
            if (response.status_code == 200):
                click.echo(f"Route {route_id} released.")
            elif (response.status_code == 400):
                log_common_feedback_err_msg(response.text)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Set a point")
@click.argument('point')
@click.argument('state')
def set_point(point, state):
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.post(server + "/controller/set-point", 
                                     data = {'point': point, 'state': state})
            if (response.status_code == 200):
                click.echo(f"Point {point} set to {state}.")
            elif (response.status_code == 400):
                log_common_feedback_err_msg(response.text)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Set a signal")
@click.argument('signal')
@click.argument('state')
def set_signal(signal, state):
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.post(server + "/controller/set-signal", 
                                     data = {'signal': signal, 'state': state})
            if (response.status_code == 200):
                click.echo(f"Signal {signal} set to {state}.")
            elif (response.status_code == 400):
                log_common_feedback_err_msg(response.text)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Set a peripheral")
@click.argument('peripheral')
@click.argument('state')
def set_peripheral(peripheral, state):
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.post(server + "/controller/set-peripheral", 
                                     data = {'peripheral': peripheral, 'state': state})
            if (response.status_code == 200):
                click.echo(f"Peripheral {peripheral} set to {state}.")
            elif (response.status_code == 400):
                log_common_feedback_err_msg(response.text)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Get the interlocker in use")
def get_interlocker():
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.get(server + "/controller/get-interlocker")
            if (response.status_code == 200):
                response_json: dict = json.loads(response.text)
                click.echo("Interlocker in use: " + response_json.get("interlocker", None))
            elif (response.status_code == 404):
                log_common_feedback_err_msg(response.text)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Set an interlocker to use")
@click.argument('interlocker')
def set_interlocker(interlocker):
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.post(server + "/controller/set-interlocker", 
                                     data = {'interlocker': interlocker})
            if (response.status_code == 200):
                click.echo(f"Interlocker set to {interlocker}.")
            elif (response.status_code == 400 or 
                    response.status_code == 409):
                log_common_feedback_err_msg(response.text)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Unset an interlocker from use")
@click.argument('interlocker')
def unset_interlocker(interlocker):
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.post(server + "/controller/unset-interlocker", 
                                     data = {'interlocker': interlocker})
            if (response.status_code == 200):
                click.echo(f"Interlocker {interlocker} has been unset.")
            elif (response.status_code == 400 or 
                    response.status_code == 409):
                log_common_feedback_err_msg(response.text)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Upload an interlocker")
@click.argument('filepath', type=click.Path(exists=True))
def upload_interlocker(filepath):
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.post(server + "/upload/interlocker",
                                     files = {'file': open(filepath, 'r')})
            if (response.status_code == 200):
                click.echo("Interlocker has been uploaded.")
            elif (response.status_code == 400 or 
                    response.status_code == 409):
                log_common_feedback_err_msg(response.text)
            elif (response.status_code == 500):
                log_common_feedback_err_msg(response.text, internal_err=True)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Delete an interlocker")
@click.argument('interlocker-name')
def delete_interlocker(interlocker_name):
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.post(server + "/upload/remove-interlocker",
                                     data = {'interlocker-name': interlocker_name})
            if (response.status_code == 200):
                click.echo(f"Interlocker {interlocker_name} deleted.")
            elif (response.status_code == 400 or 
                    response.status_code == 404 or
                    response.status_code == 409):
                log_common_feedback_err_msg(response.text)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


# --- driver ---

@click.group(help="Driver functionality")
def driver():
    pass


@click.command(help="Grab a train")
@click.argument('train')
@click.option('--engine', '-e', help="The train engine behaviour to use",
              default="libtrain_engine_default (unremovable)")
def grab(train, engine):
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global session_id, grab_id, server
        if (session_id == 0 and grab_id == -1):
            try:
                response = requests.post(server + "/driver/grab-train",
                                         data = {'train': train, 'engine': engine})
                if (response.status_code == 200):
                    response_json: dict = json.loads(response.text)
                    update_session_and_grab_id(response_json["session-id"], response_json["grab-id"])
                    click.echo("Grabbed train " + train)
                    click.echo(f"   Debug information: session-id ({response_json["session-id"]})")
                    click.echo(f"   Debug information: grab-id ({response_json["grab-id"]})")
                elif (response.status_code == 400 or 
                        response.status_code == 404 or
                        response.status_code == 409):
                    log_common_feedback_err_msg(response.text)
                elif (response.status_code == 500):
                    log_common_feedback_err_msg(response.text, internal_err=True)
                else:
                    log_not_running_or_unknown_err(response.status_code)
            except requests.exceptions.RequestException as e:
                click.echo(e, err=True)
        else:
            click.echo("You already have a train grabbed! "
                       "Release it before grabbing a new train.\n"
                       "If your grab-id or session-id is not valid, i.e., if you do not actually "
                       "have control over a train, then please re-run the " 
                       "reset_grab_and_session_id command.",
                       err=True)


@click.command(help="Release your train")
def release():
    global session_id, grab_id, server
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    elif (session_id == 0 and grab_id == -1):
        click.echo("No train grabbed!")
    else:
        try:
            response = requests.post(server + "/driver/release-train",
                                     data = {'session-id': session_id, 'grab-id': grab_id})
            if (response.status_code == 200):
                click.echo("Released grabbed train.")
                update_session_and_grab_id(0, -1)
            elif (response.status_code == 400 or 
                    response.status_code == 409):
                log_common_feedback_err_msg(response.text)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Request a route for your train from a source to a destination signal")
@click.argument('source')
@click.argument('destination')
def request_route(source, destination):
    global session_id, grab_id, server
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    elif (session_id == 0 and grab_id == -1):
        click.echo("No train grabbed!")
    else:
        try:
            response = requests.post(server + "/driver/request-route",
                                     data = {'session-id': session_id, 'grab-id': grab_id,
                                             'source': source, 'destination': destination})
            if (response.status_code == 200):
                response_json: dict = json.loads(response.text)
                route_id = response_json["granted-route-id"]
                click.echo(f"Route {route_id} from {source} to {destination} has been granted.")
            elif (response.status_code == 400 or 
                    response.status_code == 409):
                log_common_feedback_err_msg(response.text)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Request a specific route by its ID for your train")
@click.argument('route-id')
def request_route_id(route_id):
    global session_id, grab_id, server
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    elif (session_id == 0 and grab_id == -1):
        click.echo("No train grabbed!")
    else:
        try:
            response = requests.post(server + "/driver/request-route-by-id",
                                     data = {'session-id': session_id, 'grab-id': grab_id,
                                             'route-id': route_id})
            if (response.status_code == 200):
                click.echo(f"Route {route_id} has been granted.")
            elif (response.status_code == 400 or 
                    response.status_code == 409):
                log_common_feedback_err_msg(response.text)
            elif (response.status_code == 500):
                log_common_feedback_err_msg(response.text, internal_err=True)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Get a train's required physical driving direction for a given route")
@click.argument('train')
@click.argument('route-id')
def direction(train, route_id):
    global server
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        try:
            response = requests.post(server + "/driver/direction",
                                     data = {'session-id': session_id, 'train': train,
                                             'route-id': route_id})
            if (response.status_code == 200):
                response_json: dict = json.loads(response.text)
                click.echo(f"Direction: {response_json["direction"]}.")
            elif (response.status_code == 400 or 
                    response.status_code == 404):
                log_common_feedback_err_msg(response.text)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Drive along a granted route in automatic or manual mode")
@click.argument('mode', type=click.Choice(['manual', 'automatic']))
@click.argument('route-id')
def drive_route(mode, route_id):
    global session_id, grab_id, server
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    elif (session_id == 0 and grab_id == -1):
        click.echo("No train grabbed!")
    elif ((not mode == "manual") and (not mode == "automatic")):
        click.echo("Incorrect mode! Mode shall be either manual or automatic.")
    else:
        try:
            response = requests.post(server + "/driver/drive-route",
                                     data = {'session-id': session_id, 'grab-id': grab_id,
                                             'mode': mode, 'route-id': route_id})
            if (response.status_code == 200):
                response_json: dict = json.loads(response.text)
                click.echo(f"Driving route {route_id} completed, server msg: {response_json.get("msg", "")}")
            elif (response.status_code == 400):
                log_common_feedback_err_msg(response.text)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Set the dcc speed step of your train")
@click.option('--backwards', '-b', help="The rear end of the train moves forward", is_flag=True)
@click.option('--track_output', '-t', help="Use another track output than the default one",
              default="")
@click.argument('speed', type=click.IntRange(0, 126))
def set_dcc_speed(backwards, speed, track_output):
    global session_id, grab_id, server, default_track_output
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    elif (session_id == 0 and grab_id == -1):
        click.echo("No train grabbed!")
    else:
        if (backwards and speed > 0):
            speed *= -1
        if (track_output == ""):
            track_output = default_track_output
        try:
            response = requests.post(server + "/driver/set-dcc-train-speed",
                                     data = {'session-id': session_id, 'grab-id': grab_id,
                                             'speed': speed, 'track-output': track_output})
            if (response.status_code == 200):
                click.echo(f"DCC train speed set to {speed} on track-output {track_output}.")
            elif (response.status_code == 400):
                log_common_feedback_err_msg(response.text)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Set the calibrated speed step of your train")
@click.option('--backwards', '-b', help="The rear end of the train moves forward", is_flag=True)
@click.option('--track_output', '-t', help="Use another track output than the default one", 
              default="")
@click.argument('speed', type=click.IntRange(0, 9))
def set_calibrated_speed(backwards, speed, track_output):
    global session_id, grab_id, server, default_track_output
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    elif (session_id == 0 and grab_id == -1):
        click.echo("No train grabbed!")
    else:
        if (backwards and speed > 0):
            speed *= -1
        if (track_output == ""):
            track_output = default_track_output
        try:
            response = requests.post(server + "/driver/set-calibrated-train-speed",
                                     data = {'session-id': session_id, 'grab-id': grab_id,
                                             'speed': speed, 'track-output': track_output})
            if (response.status_code == 200):
                click.echo(f"Calibrated train speed set to {speed} on track-output {track_output}.")
            elif (response.status_code == 400):
                log_common_feedback_err_msg(response.text)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Emergency stop your train")
@click.option('--track_output', '-t', help="Use another track output than the default one", 
              default="")
def emergency_stop(track_output):
    global session_id, grab_id, server, default_track_output
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    elif (session_id == 0 and grab_id == -1):
        click.echo("No train grabbed!")
    else:
        if (track_output == ""):
            track_output = default_track_output
        try:
            response = requests.post(server + "/driver/set-train-emergency-stop",
                                     data = {'session-id': session_id, 'grab-id': grab_id,
                                             'track-output': track_output})
            if (response.status_code == 200):
                click.echo("Train emergency stopped.")
            elif (response.status_code == 400):
                log_common_feedback_err_msg(response.text)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Set a peripheral of your train")
@click.option('--track_output', '-t', help="Use another track output than the default one",
              default="")
@click.argument('peripheral')
@click.argument('state', type=click.Choice(['off', 'on']))
def set_train_peripheral(peripheral, state, track_output):
    global session_id, grab_id, server, default_track_output
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    elif (session_id == 0 and grab_id == -1):
        click.echo("No train grabbed!")
    else:
        if (track_output == ""):
            track_output = default_track_output
        try:
            mapping = {
                    'off': 0,
                    'on': 1,
            }
            response = requests.post(server + "/driver/set-train-peripheral",
                                     data = {'session-id': session_id, 'grab-id': grab_id,
                                             'peripheral': peripheral, 'state': mapping.get(state),
                                             'track-output': track_output})
            if (response.status_code == 200):
                click.echo(f"Train peripheral {peripheral} set to {state}")
            elif (response.status_code == 400):
                log_common_feedback_err_msg(response.text)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Upload a train engine")
@click.argument('filepath', type=click.Path(exists=True))
def upload_engine(filepath):
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.post(server + "/upload/engine",
                                     files = {'file': open(filepath, 'r')})
            if (response.status_code == 200):
                click.echo("Train engine uploaded.")
            elif (response.status_code == 400):
                response_json: dict = json.loads(response.text)
                click.echo("Error, server msg: " + response_json.get("msg", ""), err=True)
                # If the verification failed, the feedback can be quite long.
                if (response_json.get("status", True) == False):
                    # easiest for now to directly print the whole verifiedproperties object
                    click.echo("Verification of at least one property failed. " 
                               "Now printing info on all properties:")
                    click.echo(json.dumps(response_json["verifiedproperties"], indent=4, default=str))
            elif (response.status_code == 409):
                log_common_feedback_err_msg(response.text)
            elif (response.status_code == 500):
                log_common_feedback_err_msg(response.text, internal_err=True)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Delete a train engine")
@click.argument('engine-name')
def delete_engine(engine_name):
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.post(server + "/upload/remove-engine",
                                     data = {'engine-name': engine_name})
            if (response.status_code == 200):
                click.echo(f"Train engine {engine_name} deleted.")
            elif (response.status_code == 400 or 
                    response.status_code == 404 or
                    response.status_code == 409):
                log_common_feedback_err_msg(response.text)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


# --- monitor ---

@click.group(help="Monitor functionality")
def monitor():
    pass

@click.command(help="Get the name of the platform/railway that is being run")
def get_platform_name():
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.get(server + "/monitor/platform-name")
            if (response.status_code == 200):
                response_json: dict = json.loads(response.text)
                click.echo(f"Platform name: {response_json["platform-name"]}")
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Get a list of trains")
def get_trains():
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.get(server + "/monitor/trains")
            if (response.status_code == 200):
                if (response.text == ""):
                    click.echo("No trains available.")
                else:
                    response_json: dict = json.loads(response.text)
                    click.echo(json.dumps(response_json, indent=4, default=str))
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Get the state of a train")
@click.argument('train')
def get_train_state(train):
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.post(server + "/monitor/train-state",
                                     data = {'train': train})
            if (response.status_code == 200):
                response_json: dict = json.loads(response.text)
                click.echo(json.dumps(response_json, indent=4, default=str))
            elif (response.status_code == 400):
                log_common_feedback_err_msg(response.text)
            elif (response.status_code == 404):
                click.echo(f"Error, train {train} was not found.", err=True)
            elif (response.status_code == 500):
                click.echo("Internal error, server was unable to build a reply message.", err=True)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Get the state of all trains")
def get_trains_states():
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.get(server + "/monitor/train-states")
            if (response.status_code == 200):
                response_json: dict = json.loads(response.text)
                click.echo(json.dumps(response_json, indent=4, default=str))
            elif (response.status_code == 500):
                click.echo("Internal error, server was unable to build a reply message.", err=True)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Get a list of peripherals of a train")
@click.argument('train')
def get_train_peripherals(train):
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.post(server + "/monitor/train-peripherals",
                                     data = {'train': train})
            if (response.status_code == 200):
                response_json: dict = json.loads(response.text)
                click.echo(json.dumps(response_json, indent=4, default=str))
            elif (response.status_code == 400):
                log_common_feedback_err_msg(response.text)
            elif (response.status_code == 500):
                click.echo("Internal error, server was unable to build a reply message.", err=True)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Get a list of train engines")
def get_engine_list():
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.get(server + "/monitor/engines")
            if (response.status_code == 200):
                response_json: dict = json.loads(response.text)
                click.echo(json.dumps(response_json, indent=4, default=str))
            elif (response.status_code == 500):
                click.echo("Internal error, server was unable to build a reply message.", err=True)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Get a list of interlockers")
def get_interlocker_list():
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.get(server + "/monitor/interlockers")
            if (response.status_code == 200):
                response_json: dict = json.loads(response.text)
                click.echo(json.dumps(response_json, indent=4, default=str))
            elif (response.status_code == 500):
                click.echo("Internal error, server was unable to build a reply message.", err=True)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Get a list of track outputs with their state")
def get_track_outputs():
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.get(server + "/monitor/track-outputs")
            if (response.status_code == 200):
                response_json: dict = json.loads(response.text)
                click.echo(json.dumps(response_json, indent=4, default=str))
            elif (response.status_code == 500):
                click.echo("Internal error, server was unable to build a reply message.", err=True)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Get a list of points")
def get_points():
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.get(server + "/monitor/points")
            if (response.status_code == 200):
                response_json: dict = json.loads(response.text)
                click.echo(json.dumps(response_json, indent=4, default=str))
            elif (response.status_code == 500):
                click.echo("Internal error, server was unable to build a reply message.", err=True)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Get a list of signals")
def get_signals():
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.get(server + "/monitor/signals")
            if (response.status_code == 200):
                response_json: dict = json.loads(response.text)
                click.echo(json.dumps(response_json, indent=4, default=str))
            elif (response.status_code == 500):
                click.echo("Internal error, server was unable to build a reply message.", err=True)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Get detailed information on a point")
@click.argument('point')
def get_point_details(point):
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.post(server + "/monitor/point-details", data = {'point': point})
            if (response.status_code == 200):
                response_json: dict = json.loads(response.text)
                click.echo(json.dumps(response_json, indent=4, default=str))
            elif (response.status_code == 400):
                log_common_feedback_err_msg(response.text)
            elif (response.status_code == 500):
                click.echo("Internal error, server was unable to build a reply message.", err=True)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Get detailed information on a signal")
@click.argument('signal')
def get_signal_details(signal):
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.post(server + "/monitor/signal-details", data = {'signal': signal})
            if (response.status_code == 200):
                response_json: dict = json.loads(response.text)
                click.echo(json.dumps(response_json, indent=4, default=str))
            elif (response.status_code == 400):
                log_common_feedback_err_msg(response.text)
            elif (response.status_code == 500):
                click.echo("Internal error, server was unable to build a reply message.", err=True)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Get the aspects of a point")
@click.argument('point')
def get_point_aspects(point):
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.post(server + "/monitor/point-aspects", data = {'point': point})
            if (response.status_code == 200):
                response_json: dict = json.loads(response.text)
                click.echo(json.dumps(response_json, indent=4, default=str))
            elif (response.status_code == 400):
                log_common_feedback_err_msg(response.text)
            elif (response.status_code == 404):
                click.echo(f"Error, point {point} was not found.")
            elif (response.status_code == 500):
                click.echo("Internal error, server was unable to build a reply message.", err=True)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Get the aspects of a signal")
@click.argument('signal')
def get_signal_aspects(signal):
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.post(server + "/monitor/signal-aspects", data = {'signal': signal})
            if (response.status_code == 200):
                response_json: dict = json.loads(response.text)
                click.echo(json.dumps(response_json, indent=4, default=str))
            elif (response.status_code == 400):
                log_common_feedback_err_msg(response.text)
            elif (response.status_code == 404):
                click.echo(f"Error, signal {signal} was not found.")
            elif (response.status_code == 500):
                click.echo("Internal error, server was unable to build a reply message.", err=True)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Get a list of segments")
def get_segments():
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.get(server + "/monitor/segments")
            if (response.status_code == 200):
                response_json: dict = json.loads(response.text)
                click.echo(json.dumps(response_json, indent=4, default=str))
            elif (response.status_code == 500):
                click.echo("Internal error, server was unable to build a reply message.", err=True)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Get a list of reversers")
def get_reversers():
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.get(server + "/monitor/reversers")
            if (response.status_code == 200):
                response_json: dict = json.loads(response.text)
                click.echo(json.dumps(response_json, indent=4, default=str))
            elif (response.status_code == 500):
                click.echo("Internal error, server was unable to build a reply message " 
                           "or unable to get reverser state update.", err=True)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Get a list of peripherals")
def get_peripherals():
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.get(server + "/monitor/peripherals")
            if (response.status_code == 200):
                response_json: dict = json.loads(response.text)
                click.echo(json.dumps(response_json, indent=4, default=str))
            elif (response.status_code == 500):
                click.echo("Internal error, server was unable to build a reply message.", err=True)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Get whether train engines will be verified upon upload")
def get_verification_option():
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.get(server + "/monitor/verification-option")
            if (response.status_code == 200):
                response_json: dict = json.loads(response.text)
                click.echo(json.dumps(response_json, indent=4, default=str))
            else:
                click.echo(f"Unknown error, reply status code: {response.status_code}", err=True)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Get the url to the verification server")
def get_verification_url():
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.get(server + "/monitor/verification-url")
            if (response.status_code == 200):
                response_json: dict = json.loads(response.text)
                click.echo(json.dumps(response_json, indent=4, default=str))
            else:
                click.echo(f"Unknown error, reply status code: {response.status_code}", err=True)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Get a list of granted routes")
def get_granted_routes():
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.get(server + "/monitor/granted-routes")
            if (response.status_code == 200):
                response_json: dict = json.loads(response.text)
                click.echo(json.dumps(response_json, indent=4, default=str))
            elif (response.status_code == 500):
                click.echo("Internal error, server was unable to build a reply message.", err=True)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Get the details of a route")
@click.argument('route-id')
def get_route(route_id):
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.post(server + "/monitor/route", data = {'route-id': route_id})
            if (response.status_code == 200):
                response_json: dict = json.loads(response.text)
                click.echo(json.dumps(response_json, indent=4, default=str))
            elif (response.status_code == 400):
                log_common_feedback_err_msg(response.text)
            elif (response.status_code == 404):
                click.echo(f"Route with ID {route_id} was not found.", err=True)
            elif (response.status_code == 500):
                click.echo("Internal error, server was unable to build a reply message.", err=True)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Get debug information")
def get_debug_info():
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.get(server + "/monitor/debug")
            if (response.status_code == 200):
                click.echo(response.text)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


@click.command(help="Get extra debug information")
def get_debug_info_extra():
    if (not parse_config()):
        click.echo("Corrupt config, please run the config command.", err=True)
    else:
        global server
        try:
            response = requests.get(server + "/monitor/debug-extra")
            if (response.status_code == 200):
                click.echo(response.text)
            else:
                log_not_running_or_unknown_err(response.status_code)
        except requests.exceptions.RequestException as e:
            click.echo(e, err=True)


# --- assign commands to groups ---

cli.add_command(config)
cli.add_command(reset_grab_and_session_id)
cli.add_command(admin)
cli.add_command(controller)
cli.add_command(driver)
cli.add_command(monitor)

admin.add_command(startup)
admin.add_command(shutdown)
admin.add_command(set_track_output)
admin.add_command(set_verification_option)
admin.add_command(set_verification_url)
admin.add_command(admin_release)
admin.add_command(admin_set_dcc_speed)

controller.add_command(release_route)
controller.add_command(set_point)
controller.add_command(set_signal)
controller.add_command(set_peripheral)
controller.add_command(get_interlocker)
controller.add_command(set_interlocker)
controller.add_command(unset_interlocker)
controller.add_command(upload_interlocker)
controller.add_command(delete_interlocker)


driver.add_command(grab)
driver.add_command(release)
driver.add_command(request_route)
driver.add_command(request_route_id)
driver.add_command(direction)
driver.add_command(drive_route)
driver.add_command(set_dcc_speed)
driver.add_command(set_calibrated_speed)
driver.add_command(emergency_stop)
driver.add_command(set_train_peripheral)
driver.add_command(upload_engine)
driver.add_command(delete_engine)

monitor.add_command(get_platform_name)
monitor.add_command(get_trains)
monitor.add_command(get_train_state)
monitor.add_command(get_trains_states)
monitor.add_command(get_train_peripherals)
monitor.add_command(get_engine_list)
monitor.add_command(get_interlocker_list)
monitor.add_command(get_track_outputs)
monitor.add_command(get_points)
monitor.add_command(get_signals)
monitor.add_command(get_point_details)
monitor.add_command(get_signal_details)
monitor.add_command(get_point_aspects)
monitor.add_command(get_signal_aspects)
monitor.add_command(get_segments)
monitor.add_command(get_reversers)
monitor.add_command(get_peripherals)
monitor.add_command(get_verification_option)
monitor.add_command(get_verification_url)
monitor.add_command(get_granted_routes)
monitor.add_command(get_route)
monitor.add_command(get_debug_info)
monitor.add_command(get_debug_info_extra)

if __name__ == '__main__':
    cli()

