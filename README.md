# SWTbahn Client-Server Command Line Interface
[![CI/CD](https://github.com/uniba-swt/swtbahn-cli/actions/workflows/ci.yml/badge.svg)](https://github.com/uniba-swt/swtbahn-cli/actions)
[![Latest release](https://img.shields.io/github/v/release/uniba-swt/swtbahn-cli?style=version)](https://github.com/uniba-swt/swtbahn-cli/releases)

This is a client-server command line interface for the SWTbahn. The server is
connected to the BiDiB interface and provides a REST API. The client side
provides a command line interface which connects to the web service to execute
the commands. The command line interface was developed by
[Nicolas Gross](https://github.com/nicolasgross) as part of his work for the
libbidib project.


## Dependencies

#### Server
* C compiler
* Libraries: [onion](https://github.com/uniba-swt/onion), libpam, libgnutls,
libgcrypt, libpthread, libglib-2.0, libyaml,
[libbidib](https://github.com/uniba-swt/libbidib)
* ForeC command line compiler: [forecc](https://github.com/PRETgroup/ForeC/tree/master/ForeC%20Compiler)
* KIELER command line compiler: [kico.jar](https://rtsys.informatik.uni-kiel.de/~kieler/files/nightly/sccharts/cli/)
  * Path to the folder that contains the kico.jar has to be defined in the environment variable `KIELER_PATH`
* BahnDSL command line compiler: [bahnc](https://github.com/trinnguyen/bahndsl)
* SCCharts verifier: [SWTbahn Verifier (SWT internal repository)](https://gitlab.rz.uni-bamberg.de/swt/swtbahn-verifier)

#### Command Line Client
* python3
* Python libraries: click, requests, pyaml (`pip3 install click requests pyaml`)

#### Web Client
* Web browser

## Build
1. Clone the repository
2. Install the necessary dependencies (remember to run `ldconfig` after `make install` for libbidib if you are on Linux)
3. Navigate to the directory where the build files should be generated
4. Execute `cmake <path-to-project-root>/server`
5. Execute `make`


## Test
To run the unit tests, execute `make test` from within the build directory. Each unit test can be executed to display more detailed test results, e.g., `./server_bahn_util_tests`.


## Usage

#### Server
0. Build the executable (see section Build)
1. Create the configuration files for your track (see libbidib documentation)
2. Connect the BiDiB interface to the server
3. Check on which serial device the board is connected: `dmesg | grep tty`
4. Start the server: `./swtbahn-server <serial-device> <config-directory>
<IP> <port>` (IP is the IP-address under which the server can be reached and
port specifies on which port the server listens)  
  For example: `./swtbahn-server /dev/ttyUSB0 ../../configurations/swtbahn-lite/ 141.13.106.27 2048`  
5. Quit the server with Ctrl-C if you're done

#### Client (Command Line)
0. The client is located under `<project-root>/client/swtbahn`
1. Use the `--help` flag to get information about parameters, options, ...
2. Use the config command to setup the client: `swtbahn config <hostname> <port>
<default-track-output>`, where hostname is the IP and port the port which you
used when you started the server. The <default-track-output> is the name of the
track output which should be used if no track output is explicitly specified
when issuing a command.  
  For example: `./swtbahn config 141.13.106.27 2048 master`  
This will create a config file in the current working directory. Dont use the
client from a different working directory for the current session, otherwise it
won't find the configuration file. If you really want to use two logically
different clients on the same device, you could just run the clients from
different working directories which will lead to distinct configs.
3. Now you can use the commands from the categories `swtbahn admin`,
`swtbahn controller`, `swtbahn driver` and `swtbahn monitor`. If the system was
not started by `swtbahn admin startup`, all the other commands won't work.  

    For example:  
    `./swtbahn admin startup`  
    `./swtbahn monitor get_segments`  
    `./swtbahn monitor get_trains`  
    `./swtbahn driver grab cargo`  
    `./swtbahn driver set_dcc_speed -b 5`  
4. If you're done, you should shut the system down gracefully by invoking
`swtbahn admin shutdown`

#### Client (Web Interface)
0. Use a web browser to navigate to `<IP>:<port>/assets/client.html`
1. Set the main track output (default is `master`).
2. Click the `Startup` button.
3. To be a train driver, select a train and click the `Grab` button. 
   Enter a speed between `0` (stop) and `127` (max speed), and click the `Drive` button.
4. To control a point, type the name of a point (see the YAML configuration file), 
   select a position, and click the `Set` button.
5. To control a signal, type the name of a signal (see the YAML configuration file), 
   select an aspect, and click the `Set` button.

#### Logging into the Raspberry Pi remotely
Use `ssh` to the log in remotely. Suppose the Raspberry Pi is located at `141.13.106.30`.
Then use the command `ssh pi@141.13.106.30`. 

#### Strategy for finding the IP address of the Raspberry Pi
There are two Raspberry Pis with the hostnames `raspberrypi1` and `raspberrypi2`. 
The [Raspberry Pi documentation](https://www.raspberrypi.org/documentation/remote-access/ip-address.md) 
lists several strategies for finding the IP address of a Raspberry Pi.
If you change the Raspberry Pi's hostname, e.g., by editing `/etc/hostname` or by using
the Raspberry Pi Configuration application, Avahi will automatically change the .local mDNS address.
The MAC address of all Raspberry Pis begin with `b8:27:eb`.

1. **macOS:** 
Try and resolve the Raspberry Pi's hostname, e.g., raspberrypi1.local, with multicast DNS:

    `ping raspberrypi1.local`

    If this fails, try the strategy for Linux.

2. **Linux:** 
Use `ifconfig` to find your subnet, e.g., `192.168.1._`. 
Use the `nmap` command to scan your subnet for connected devices,
and look for the Raspberry Pi's hostname in the results:

    `nmap -sn 192.168.1.0/24`


3. **Windows:** 
Use `ipconfig` to find your subnet, e.g., `192.168.1._`.
Install [nmap](https://nmap.org/download.html) and open the command line 
as an administrator for nmap to display the hostnames in its results. 
Use the `nmap` command to scan your subnet for connected devices, and 
look for the Raspberry Pi's hostname in the results:

    `nmap -sn 192.168.1.0/24`

    Use the command `nmap -sn 192.168.1.0/24 | findstr /i "b8:27:eb"` to quickly 
    see a list of Raspberry Pis (MAC addresses only).

#### Sending files from the Raspberry Pi to a client computer:
Use `scp` on your client computer. For example, suppose the Raspberry Pi is located at 
`141.13.106.30` and has the user `pi`. To copy the the file `/var/log/syslog` from the 
Raspberry Pi via the user `pi`, use the command `scp pi@141.13.106.30:/var/log/syslog syslog`

## Verification
Train engines (and ther plugins) defined as SCCharts (*.sctx) files can be verified against certain
properties before they are uploaded to the sever. Verification is on by default and 
can be configured to `true` (On) or `false` (Off) via the `/admin/set-verification-option` endpoint. 
For the command line client, use the command `swtbahn admin set-verification-option <true/false>`.

When verification has been turned on and an SCCharts file is uploaded, the server sends a 
request to [SWTbahn Verifier (SWT internal repository)](https://gitlab.rz.uni-bamberg.de/swt/swtbahn-verifier), 
which...
1. checks that the SCCharts file defines a set of safety properties (invariants, LTLs),
2. compiles the SCCharts file to a format that can be verified (e.g., using nuSmv),
3. verifies the safety properties, and
4. sends verification results back to the server.

If all safety properties hold, the server processes the SCCharts file, e.g., by compiling and loading the file as a plugin. 
Otherwise, if any safety property does not hold, the server does not process the SCCharts file any further and reports
the failure to the client.  

The sever communicates with SWTbahn Verifier via websockets with JSON messages. A typical verification session is as follows:
1. swtbahn-cli server receives an upload request for an SCCharts file;
2. swtbahn-cli server establishes a websocket connection with SWTbahn Verifier;
3. swtbahn-cli server sends the SCCharts file as a JSON message to SWTbahn Verifier;
4. SWTbahn Verifier responds by either 
   1. acknowledging the request to verify the SCCharts file and proceeding with the verification, or
   2. rejecting the request;
5. swtbahn-cli responds by either
   1. waiting for the verification results on the websocket connection, or
   2. stopping the processing of the SCCharts file;
6. for the case that SWTbahn Verifier proceeded with the verification, after the verification has finished, SWTbahn Verifier sends the 
   results as a JSON message to the swtbahn-cli server; and
7. swtbahn-cli receives the results, closes the websocket connection, and processes the results.

## Grab-id and session-id behaviour
Grab-ids are used as tokens for trains. A client needs to grab a train before he
can invoke commands for controlling the train. A train could only be grabbed by
one client at a time.  
Session-ids are used to check wether a grab-id was given out to the client on
the current session or on a previous session.

#### Server side
When is a new session-id created?
* If a user issues `swtbahn admin startup` and the system is not running already

When is the session-id invalidated?
* If a user issues `swtbahn admin shutdown`

When is a grab-id created?
* If a client issues `swtbahn driver grab <train>` and the train is available

When is a grab-id invalidated?
* If a client issues `swtbahn driver release` and the train was grabbed by this
client

#### Client side
When is the session-id fetched?
* If the user issues `swtbahn driver grab <train>` and the train is available and
the client has not already grabbed a train

When is the session-id reset?
* If the user issues `swtbahn driver release`
* If the user issues any `swtbahn driver` command and the session-id or grab-id
is not the same as the one at the server
* If the user issues `swtbahn admin shutdown` and the system was running
* If the user issues `swtbahn config`

When is a grab-id fetched?
* If the user issues `swtbahn driver grab <train>` and the train is available and
the client has not already grabbed a train

When is a grab-id reset?
* If the user issues `swtbahn driver release`
* If the user issues any `swtbahn driver` command and the session-id or grab-id
is not the same as the one at the server
* If the user issues `swtbahn admin shutdown` and the system was running
* If the user issues `swtbahn config`
