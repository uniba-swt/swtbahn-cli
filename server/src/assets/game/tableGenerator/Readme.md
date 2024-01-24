# Table Generator for SWTbahn Game

This application generates the configuration json files for the swtbahn game client, which are required for the signal-symbol mapping and the route suggestion mechanics.
Once entered the signals to symbol, the programm will automatically find a suitable route (which the least nodes) and store it for the game client.
There is also the option to block single route ids within the blacklist.txt


## Requirements
* Python3
* Configurationfiles of Bahn
    * Interlocking table
    * extra configuration (extra_config.yaml)


## How to get started
### Map the Signals with the Symbols
* Create a CSV Document with the following Scheme to the flagMappingsFolder (it should have the same name, like the configuration folder): [signalId],[coloCode]
* insert your mapping attributes
* link the csv to generator.py and converter.py
* run generator.py
* run converter.py
* link the generated files to the game

