# Table Generator for SWTbahn Game

These python scripts generate the game configuration json files for the swtbahn game client, which are required for the client's signal-to-symbol mappings (which symbol is related to which signal id) and the route suggestion mechanics (which routes are available depending on where the train is located). These game configuration files are generated for every model railway platform for which a signal-to-symbol mapping csv file is provided in the flagMappings directory and for which the required configuration files are available (for details, see below).

The scripts will automatically find the shortest possible route from a valid starting (source) signal to any destination signal which is listed in the signal-to-flag mapping csv (for the model railway platform). The routes found are grouped based on the block on which the respective source signal is located, and they are stored in JSON format, in one file per model railway platform. There is an option to block specific routes from being considered for the generated game configuration files. See below for more details.


## Requirements
* Python3
* Python3 libraries: see `requirements.txt` in the same directory as this readme.
* Configuration files for the model railway: (have to be located at `swtbahn-cli/configurations/<platformname>/<configfileshere>`, as the script tries to access them via relative paths)
    * Interlocking table (`interlocking_table.yml`)
    * extra configuration (`extras_config.yaml`)

## How to get started
As listed in the requirements above, it is expected that the python scripts are executed from the commandline in this directory ([`swtbahn-cli/server/src/assets/game/tableGenerator`](.)). The configuration files are expected to be located at [`swtbahn-cli/configurations`](../../../../../configurations), where the `configurations` directory contains one directory per model railway platform, e.g. `swtbahn-full` and `swtbahn-standard`.   
To generate game configuration files, first you need to define - for each model railway platform you wish to generate game configuration files for - which signals are to be considered as destinations, and with what symbol they should be represented. 

### Map the signals to symbols
* In the [`flagMappings`](flagMappings) directory, create a CSV file for each model railway platform you wish to define the signal-to-symbol mappings for. The file name shall match the name of the directory which contains the configuration files of the respective model railway platform. For example, the signal-to-symbol mapping file for the SWTbahn Standard shall be named [`swtbahn-standard.csv`](flagMappings/swtbahn-standard.csv), because the directory with the configuration files of that model railway platform is named `swtbahn-standard` (see [`swtbahn-cli/configurations`](../../../../../configurations)).
* The format of the CSV file shall be as follows: One line per definition of a signal-to-symbol mapping. A line has the format `<signal-id-as-integer>,<symbol code>`. The signal ID is to be provided as just the number at the end of the usual signal ID. As an example, the signal we would usually call `signal5` is just `5` in the CSV file. The format for symbol codes is explained further down in this document.
* All signals which should be considered as possible destinations shall have exactly one signal-to-symbol mapping. 
* Every signal ID shall occur at most once in a CSV file. If two mappings for the same signal ID are defined in one file, the behavior is undefined.
* The game client user interface tries to display the possible next destinations (as symbols) ordered in the same way that they appear in the CSV.

#### Example:  
As described above, for the SWTbahn Standard the filename would be [`swtbahn-standard.csv`](flagMappings/swtbahn-standard.csv), due to how its configuration files folder is named.   
The content of the file could look like:
```
6,g3
5,g2
17,g1
19,g0
3,r2
12,r1
```
Thus, the signals with IDs 6, 5, 17, 19, 3 and 12 would be considered as possible destination signals when generating the game configuration for the SWTbahn Standard.

### Define blacklisted routes (optional)
You can exclude specific routes from being considered by providing the route IDs in a blacklist file (one per model railway platform) in the [`blacklists`](blacklists) directory. 
* Like the CSV file for the signal-to-symbol mappings, the filename shall match the name of the configuration folder for the model railway platform, though the file ending shall be `.txt` in this case. As an example, the blacklist file for SWTbahn Standard would be `swtbahn-standard.txt` and it would exist inside the `blacklists` directory.
* For each route you want to blacklist, add a new line to the blacklist file and enter the route's ID in that line (and nothing else). For example, if you want to exclude the routes with ID 1 and 2, then the file would have to have 2 lines, with one line containing `1` and the other line containing `2`.

### Run the game configuration file generation
* run [`generator.py`](generator.py) via a commandline/terminal whose current working directory is the same as where the python scripts for generation are located, i.e., [`tableGenerator`](.).
   * In the directory above, new files should appear - one per model railway platform which was recognized (i.e., for which the signal-to-symbol CSV and the config directory were found). The names of the new files should start with `flags-`, followed by the name of the model railway platform, and ending on `.json`. These files contain the definitions of the symbol for each signal that has exactly one mapping defined in the CSV file discussed earlier.
* run [`converter.py`](converter.py) (in the same way as you ran `generator.py`).
   * Again, new files should appear (or existing files updated) in the directory above. The names of the new files should start with `destinations-`, followed by the name of the model railway platform, and ending on `.json`. These files contain the definitions of which destinations are available from which block of the model railway platform.
* (TODO ADJUST WHEN WHO-AM-I ENDPOINT IS INTEGRATED) link the generated files to the game
    * replace the return string of `getPlatformName()` of [`script-game.js`](../script-game.js) to your platform name.

---

## Symbol codes
The symbol code is a two-part coding system that describes the dice symbol of the swtbahn in 2 to 3 characters.
The first part describes the coloring schema of the symbol. There are four colors:
* **b**: Blue
* **g**: Green
* **r**: Red
* **s**: Black

A symbol code shall start with one of the four characters above, which determines the main color of the symbol. The color will be the background of the dice face, with the dots being white.
In case you want to "invert" the colors (so instead of a symbol with colored background and white dots, you get a colored outline with colored dots on a white background), append the character **w** after the color character. So, the first part could be, e.g., `b`, `bw`, `g`, or `gw`, and so on.

The second part of the code defines the number of dots of the dice face. Currently any number within the range 0 to 5 is supported. 6 is not supported (would be displayed as a 0).    
Some examples for valid symbol codes: `sw1`, `b5`, `rw0`, `b2`, `g3`, `s4`.

### Examples
#### `g3`:  
![](docs/g3.png)  
#### `bw5`:  
![](docs/bw5.png)  
