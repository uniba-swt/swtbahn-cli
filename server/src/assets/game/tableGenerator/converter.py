from csv import reader
import json, yaml, os

##### META #####

pathToConfig = "../../../../../configurations"



interlockingTableFileName = "interlocking_table.yml"
configurationBahnFileName = "extras_config.yml"

blacklistFileDirectory = "./blacklists"
groupingFileDirectory = "./flagMappings"

configFolderItemList = os.scandir(pathToConfig)
folderList = []

##### FUNCTIONS #####
def generateJsonStructure(resultData):
"""
Params: 
    ResultData 
    Json with the format
    Includes the default structure for the endformat and handle all data

Returns:
    Updated version of the ResultData Json

Doing:
    Roll over all Blocks and search for a RouteID for the destination and insert the details based on start block, destination signal. Data were read from the interlocking table
"""
    originalResultData = resultData
    resultData = {}
    for block in originalResultData:
        destinations = []
        for destination in originalResultData[block]:
            destinations.append(destination)
        destinationsSorted = []
        with open(groupingFile, "r") as csvFile:
            csv_reader = reader(csvFile)
            for row in csv_reader:
                signal = "signal" + row[0]
                print(signal)
                if signal in destinations:
                    destinationsSorted.append(signal)
                signal = signal + "a"
                if signal in destinations:
                    destinationsSorted.append(signal)
                    

        resultData[block] = {}
        for destination in destinationsSorted:
            resultData[block][destination] = {}
            resultData[block][destination]["route-id"] = originalResultData[block][destination]["route-id"]
            resultData[block][destination]["orientation"] = originalResultData[block][destination]["orientation"]
            resultData[block][destination]["block"] = originalResultData[block][destination]["block"]
            resultData[block][destination]["segment"] = originalResultData[block][destination]["segment"]

        return resultData

def interlockerDataExtraction(routes):
"""
Filters the resultData Json for blacklisted routes and check if there are shorter routes
"""
    global resultData
    for route in routes:
        destination = interlockingTable["interlocking-table"][route]["destination"]
        routeID = route
        orientation = interlockingTable["interlocking-table"][route]["orientation"]
        lastBlock = interlockingTable["interlocking-table"][route]["sections"][-1]["id"]
        stopSegment = None
        segments = []
        isError = False

        for qblock in configuratonBahn["platforms"]:
            if qblock["id"] == lastBlock:
                for segment in qblock["main"]:
                    segments.append(segment)
                break
        for qblock in configuratonBahn["blocks"]:
            if qblock["id"] == lastBlock:
                for segment in qblock["main"]:
                    segments.append(segment)
                break
        if len(segments) == 1:
            stopSegment = segments[0]
        elif len(segments) == 2:
            if segments[0][0:-1] == segments[1][0:-1]:
                stopSegment = segments[0][0:-1]
            else:
                print("Error Segment {} and Segment {} are not the same. Block {}".format(segments[0], segments[1],
                                                                                          block["id"]))
                isError = True
        else:
            print("Error with BlockID {}".format(block["id"]))
            isError = True
        if isError:
            print("error with {}".format(route))
            break
        resultData[block["id"]][destination] = {}
        resultData[block["id"]][destination]["route-id"] = routeID
        resultData[block["id"]][destination]["orientation"] = orientation
        resultData[block["id"]][destination]["block"] = lastBlock
        resultData[block["id"]][destination]["segment"] = stopSegment


    
for entry in configFolderItemList: # Get a list of configuration possibilities which are given by the model railway
    if entry.is_dir():
        folderList.append(entry.name)

for configuration in folderList: # Roll over the list of configuration possibilities and check if a mapping CSV is existing
    
    mappingFolderContent = os.scandir(groupingFileDirectory)
    flagMappings = []
    for entry in mappingFolderContent:
        if entry.is_file() and entry.name[-4:] == ".csv":
            flagMappings.append(entry.name[:-4])

    if configuration not in flagMappings: # Ignore all entries which doesnt have an equivalent Mapping-CSV
        continue
    
    
    blacklist = []

    if os.path.exists("{}/{}.txt".format(blacklistFileDirectory, configuration)): # import the blacklists if exists
        for line in open("{}/{}.txt".format(blacklistFileDirectory, configuration)):
            blacklist.append(int(line.strip()))        


    interlockingTableFile = "{}/{}/{}".format(pathToConfig, configuration, interlockingTableFileName)
    configurationBahnFile = "{}/{}/{}".format(pathToConfig, configuration, configurationBahnFileName)


    interlockingTable = json.loads(json.dumps(yaml.safe_load(open(interlockingTableFile))))
    configuratonBahn = json.loads(json.dumps(yaml.safe_load(open(configurationBahnFile))))
    
    groupingFile = "{}/{}.csv".format(groupingFileDirectory, configuration)


    resultData = {}

    blocktypes = ["blocks", "platforms"]

    for blockType in blocktypes: # Roll over all Blocktypes
        for block in configuratonBahn[blockType]: # Roll over all Blocks
            try:
                signals = block["signals"]
            except KeyError:
    #            print("Block {} has no signals".format(block["id"]))
                signals = []
            routes = []
            for signal in signals: # Roll over all Signals of the Block
                destinations = []
                for route in interlockingTable["interlocking-table"]:
                    if route["source"] == signal:
                        if route["destination"] not in destinations:
                            destinations.append(route["destination"])
                for destination in destinations: # Roll over all Destination of the specific signal and check if a route id is there
                    routeID = -1
                    for route in interlockingTable["interlocking-table"]:
                        if route["source"] == signal and route["destination"] == destination:
                            if routeID == -1:
                                routeID = route["id"]
                                print("{} --> {} | {} {}".format(signal, destination, routeID, block["id"]))                
                            else:
                                if interlockingTable["interlocking-table"][routeID]["id"] in blacklist:
                                    print("Route was overriden because old was blacklisted")
                                elif interlockingTable["interlocking-table"][route["id"]]["id"] in blacklist:
                                    print("Route Beibehalten")
                                elif len(interlockingTable["interlocking-table"][route["id"]]["path"]) < len(interlockingTable["interlocking-table"][routeID]["path"]): # if new Route ID is shorter than old route id (less node points) than overwrite the routeid
                                    routeID = route["id"]
                                    print("{} -> {} | {} {}".format(signal, destination, routeID, block["id"]))
                    

                    if routeID not in blacklist:
                        routes.append(routeID)
                    else:
                        print("Route {} wurde ignoriert, weil diese in der Blacklist steht".format(routeID))

            resultData[block["id"]] = {}
            interlockerDataExtraction(routes)


    # Group and sort based on interlocking table blocks (block -> destination)
    jsonStructure = generateJsonStructure(resultData)
    # Write to json file
    with open("../destinations-{}.json".format(configuration), "w") as file: 
        file.write("const allPossibleDestinations-{} = ".format(entry))
        file.write(json.dumps(jsonStructure, indent=2))
        file.write(";")
