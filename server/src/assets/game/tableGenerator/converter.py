from csv import reader
import json, yaml, os

##### GLOBALS #####
pathToConfig = "../../../../../configurations"

interlockingTableFileName = "interlocking_table.yml"
extrasConfigFileName = "extras_config.yml"

blacklistFileDirectory = "./blacklists"
signalFlagCSVFileDirectory = "./flagMappings"

##### FUNCTIONS #####
def getConfigurationsWithMatchingSignalFlagCSV() -> list[str]:
    """
    For every configuration folder in swtbahn-cli/configurations, check if a corresponding
    signal-to-symbol/flag mapping CSV file exists. Returns a list with names of all folders
    where this CSV file exists.
    """
    global pathToConfig, signalFlagCSVFileDirectory
    # Load the list of available CSV files with signal-symbol/flag map definitions
    signalFlagCSVFolderItemList = os.scandir(signalFlagCSVFileDirectory)
    #flagMappingFileNames = []
    #for entry in signalFlagCSVFolderItemList:
    #    if entry.is_file() and entry.name[-4:] == ".csv":
    #        flagMappingFileNames.append(entry.name[:-4])
    flagMappingFileNames = [entry.name[:-4] for entry in signalFlagCSVFolderItemList if entry.is_file() and entry.name[-4:] == ".csv"]
    # Load list of configuration folders and search for matching (name-based) CSV files
    configFolderItemList = os.scandir(pathToConfig)
    folderList = [entry.name for entry in configFolderItemList if entry.is_dir()]
    #configNamesWithCSVList = []
    #for configuration in folderList: 
    #    if configuration in flagMappingFileNames:
    #        configNamesWithCSVList.append(configuration)
    configNamesWithCSVList = [configuration for configuration in folderList if configuration in flagMappingFileNames]
    return configNamesWithCSVList


def getSignalIDStrsWithDefinedMapping(signalFlagCSVFilepath: str, extrasConfig) -> list[str]:
    """
    Reads the signal-to-symbol/flag CSV file and extracts all signal IDs (represented as strings,
    in the long format, e.g., `signal8`). If the signal is actually a composite signal, then its
    ID is appendend an "a" as all composite signals AT THE MOMENT have the "proper" (non-distant)
    signal ID with an "a". (e.g., signal4 will be signal4a in the returned list)
    """
    signalIDList = []
    with open(signalFlagCSVFilepath, "r") as csvFile:
        csv_reader = reader(csvFile)
        for row in csv_reader:
            signal = "signal" + row[0]
            if extrasConfig["compositions"] != None:
                for comp in extrasConfig["compositions"]:
                    if signal == comp["id"]:
                        signal += "a"
                        break
            signalIDList.append(signal)
    return signalIDList


def getBlacklistedRouteIDsForConfig(config: str) -> list[int]:
    """
    Get a list of all route IDs blacklisted for this configuration/model railway platform.
    If no blacklist file for the config is found, an empty list is returned.
    """
    global blacklistFileDirectory
    blacklistedRouteIDs = []
    if os.path.exists("{}/{}.txt".format(blacklistFileDirectory, config)):
        for line in open("{}/{}.txt".format(blacklistFileDirectory, config)):
            blacklistedRouteIDs.append(int(line.strip()))
    return blacklistedRouteIDs


def getInterlockingAndExtrasConfigFileContent(config: str) -> any:
    """
    Attempts to load the interlocking table and extras-config configuration files into dicts.
    Throws an exception if the config files are not found.
    """
    global pathToConfig, interlockingTableFileName, extrasConfigFileName
    # Determine filepaths for config files, then load the config file content
    interlockingTableFile = "{}/{}/{}".format(pathToConfig, config, interlockingTableFileName)
    extrasConfigFile = "{}/{}/{}".format(pathToConfig, config, extrasConfigFileName)
    if not os.path.exists(interlockingTableFile) or not os.path.exists(extrasConfigFile):
        raise Exception("Error: interlocking table file or extras-config file for " 
                        + config + " were not found")

    interlockingTable = yaml.safe_load(open(interlockingTableFile))
    extrasConfig = yaml.safe_load(open(extrasConfigFile))
    return interlockingTable, extrasConfig


def calcWantedRoutesForBlock(block: dict, interlockingTable: dict, signalsWithDefMapping: list[str], 
                                blacklistedRouteIDs: list[int]) -> dict:
    """
    Searches the interlocking table for routes that start from a signal within the specified block.
    Adds all these routes to a dict in a certain format, but only if the route's destination signal 
    is in signalsWithDefMapping and where the route-id is not in blacklistedRouteIDs; 
    if multiple routes from a block to a destination signal exist, only the shortest one is added.
    """
    blockDictRet = {}
    signalsOfCurrBlock = []
    try:
        signalsOfCurrBlock = block["signals"]
    except KeyError:
        return blockDictRet
    # Search for valid routes, but we only want the shortest for each destination. 
    # Thus have an extra dict to keep track of lengths -> easier than to modify json structure.
    destsToShortestRouteWithLength = {}
    for route in interlockingTable["interlocking-table"]:
        # If route starts at current block and destination has a valid mapping...
        if (route["source"] in signalsOfCurrBlock 
            and route["destination"] in signalsWithDefMapping 
            and int(route["id"]) not in blacklistedRouteIDs):

            routeID = int(route["id"])
            rLen = int(route["length"][:-5]) # length calc here ignores millimeters
            # If this route is the first to this destination or shorter than the current one...
            if (route["destination"] not in destsToShortestRouteWithLength.keys() 
                or destsToShortestRouteWithLength[route["destination"]][2] > rLen):

                destsToShortestRouteWithLength[route["destination"]] = (routeID, route["source"], rLen)
                blockDictRet[route["destination"]] = {
                    "route-id"    : routeID,
                    "orientation" : route["orientation"],
                    "block"       : route["sections"][-1]["id"],
                    "segment"     : route["path"][-2]["id"]
                }
                # Remove "a" and "b" suffixes from "segment" entry if they exist
                if route["path"][-2]["id"].endswith("a") or route["path"][-2]["id"].endswith("b"):
                    blockDictRet[route["destination"]]["segment"] = route["path"][-2]["id"][:-1]
    return blockDictRet


def processConfiguration(config: str):
    """
    For a given configuration, loads config files, generates the destinations json and persists it as a file.
    """
    # Get the blacklisted route IDs(if any)
    blacklistedRouteIDs = getBlacklistedRouteIDsForConfig(config)
    # Get the interlockingTable and extrasConfig
    try:
        interlockingTable, extrasConfig = getInterlockingAndExtrasConfigFileContent(config)
    except Exception as e:
        print(e)
        return False
    # Get the list of signals with valid mappings (signalID appended with 'a' if its a composite signal)
    signalFlagMapCSVFilepath = "{}/{}.csv".format(signalFlagCSVFileDirectory, config)
    signalsWithDefMapping = getSignalIDStrsWithDefinedMapping(signalFlagMapCSVFilepath, extrasConfig)

    print("Loaded config files for config " + config + ", now calculating routes")

    collectedRoutesPerBlock = {}
    # Relevant keys for searching in extrasConfig
    blocktypes = ["blocks", "platforms"]
    # For blocks of all kinds, get the routes to possible destinations.
    for blockType in blocktypes:
        for block in extrasConfig[blockType]:
            # Initialize block dict, then calculate routes
            collectedRoutesPerBlock[block["id"]] = {}
            blockDict = calcWantedRoutesForBlock(block, interlockingTable, 
                                                 signalsWithDefMapping, blacklistedRouteIDs)
            # Now insert the routes into the "main" dict in the order that they appear in the 
            # symbol/flag mappings csv file. The order in the CSV file is the same as the order 
            # in signalsWithDefMapping, so we use that as the reference.
            for sig in signalsWithDefMapping:
                if sig in blockDict:
                    collectedRoutesPerBlock[block["id"]][sig] = blockDict[sig]

    # Persist the identified routes grouped by block and destination signal as a JSON file.
    with open("../destinations-{}.json".format(config), "w") as destFile: 
        destFile.write("const allPossibleDestinations_{} = ".format(config.replace("-", "_")))
        destFile.write(json.dumps(collectedRoutesPerBlock, indent=2))
        destFile.write(";")
    return True


def startGen():
    configs = getConfigurationsWithMatchingSignalFlagCSV()
    for c in configs:
        print("Run processConfiguration for config " + c)
        if processConfiguration(c):
            print("Finished processConfiguration for config " + c + " successfully!")
        else:
            print("processConfiguration for config " + c + " did not succeed!")


##### START #####
startGen()
