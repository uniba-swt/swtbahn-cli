# SWTbahn client-server command line interface

This is a client-server command line interface for the SWTbahn. The server is
connected to the BiDiB interface and provides a REST API. The client side
provides a command line interface which connects to the web service to execute
the commands. The command line interface was developed by
[Nicolas Gross](https://github.com/nicolasgross) as part of his work for the
libbidib project.


## Dependencies

#### Server
* A C compiler
* Libraries: [onion](https://github.com/davidmoreno/onion), libpam, libgnutls,
libgcrypt, libpthread, libglib-2.0, libyaml,
[libbidib](https://github.com/uniba-swt/libbidib)

#### Client
* python3
* Libraries: click, requests, pyaml (`pip3 install click requests pyaml`)


## Build
1. Clone the repository
2. Adjust `server/CMakeLists.txt` according to your installations of the
dependencies
3. Navigate to the directory where the build files should be generated
4. Execute `cmake <path-to-project-root>/server`
5. Execute `make`


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

#### Client
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

#### Logging into the Raspberry Pi remotely
Use `ssh` to the log in remotely. Suppose the Raspberry Pi is located at `141.13.106.30`.
Then use the command `ssh pi@141.13.106.30`. 

#### Strategy for finding the IP address of the Raspberry Pi
Use `ping raspberrypi.local` to find the IP address of the Raspberry Pi, where `raspberrypi.local` is its hostname.

#### Sending files from the Raspberry Pi to a client computer:
Use `scp` on your client computer. For example, suppose the Raspberry Pi is located at 
`141.13.106.30` and has the user `pi`. To copy the the file `/var/log/syslog` from the 
Raspberry Pi via the user `pi`, use the command `scp pi@141.13.106.30:/var/log/syslog syslog`


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

