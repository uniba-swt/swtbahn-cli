# Table Generator for SWTbahn Game

This python script generates the configuration json files for the swtbahn game client, which are required for the signal-symbol mapping and the route suggestion mechanics.
The script will automatically find a suitable route (which the least nodes) and store it for the game client, it will do this action only for signals which are listed in the mapping-csv to reduce storage usage.
There is also the option to block single route ids within the blacklist.txt


## Requirements
* Python3
* Configuration files for the model railway
    * Interlocking table
    * extra configuration (extra_config.yaml)


## How to get started
### Map the Signals with the Symbols
* Create a CSV Document with the following Scheme to the flagMappings directory (the filename should match with the name of the configuration folder of the model railway): [signalId],[colorCode]  
#### Example:  
for the swtbahn-standard the filename would be `swtbahn-standard.csv`  
The content of the file would look like:
```
6,g3
5,g2
17,g1
19,g0
3,r2
12,r1
```
* insert your mapping attributes
* link the csv to generator.py and converter.py
* run generator.py
* run converter.py
* link the generated files to the game
    * replace the return string of `getPlatformName()` of `script.js` to your platform name

