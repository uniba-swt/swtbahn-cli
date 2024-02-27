# Table Generator for SWTbahn Game

This python script generates the configuration json files for the swtbahn game client, which are required for the signal-symbol mapping and the route suggestion mechanics.
The script will automatically find the shortest possible route from any block of the track to any destination which is equipped with a symbol and store it for the game client in a fast readable format, it will do this action only for signals which are listed in the mapping-csv to reduce storage usage.
There is also the option to block single route ids within the blacklist.txt


## Requirements
* Python3
* Configuration w for the model railway
    * Interlocking table
    * extra configuration (extra_config.yaml)

## Color codes
The color code is a coding system which descripes the dice symbol of the swtbahn within 2 to 3 characters and can be divided into two parts.
The first part describes the coloring schema of the symbol. There are four colors:
* **B**: Blue
* **G**: Green
* **R**: Red
* **S**: Black
In case we want to invert the colors (so instead of a symbol with red background and a red outline, we want a red outline and a white background), we append a *w* behind the color.

The second part of the code descripes the dots of the dice. (Currently they can be numbers from 0 to 5, a six will currently count as 0).

### Examples
* B5:  
![](docs/b5.png)  
* RW5:  
![](docs/rw5.png)  

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
* run generator.py
* run converter.py
* link the generated files to the game
    * replace the return string of `getPlatformName()` of `script.js` to your platform name

