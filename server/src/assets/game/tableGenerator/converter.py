from csv import reader
import json
import yaml

interlockingTableFile = "../../../../../configurations/swtbahn-full/interlocking_table.yml"
configuratonBahnFile = "../../../../../configurations/swtbahn-full/extras_config.yml"
blacklistFile = "./blacklist.txt"
groupingFile = "./flags-swtbahn-full.csv"
interlockingTable = json.loads(json.dumps(yaml.safe_load(open(interlockingTableFile))))
configuratonBahn = json.loads(json.dumps(yaml.safe_load(open(configuratonBahnFile))))

blacklist = []

for line in open(blacklistFile):
    blacklist.append(int(line.strip()))

resultData = {}

blocktypes = ["blocks", "platforms"]

for blockType in blocktypes:
    for block in configuratonBahn[blockType]:
        try:
            signals = block["signals"]
        except KeyError:
#            print("Block {} has no signals".format(block["id"]))
            signals = []
        routes = []
        for signal in signals:
            destinations = []
            for route in interlockingTable["interlocking-table"]:
                if route["source"] == signal:
                    if route["destination"] not in destinations:
                        destinations.append(route["destination"])
            for destination in destinations:
                routeID = -1
                for route in interlockingTable["interlocking-table"]:
                    if route["source"] == signal and route["destination"] == destination:
                        if routeID == -1:
                            routeID = route["id"]
                        else:
                            if interlockingTable["interlocking-table"][routeID]["id"] in blacklist:
                                print("Route was overriden because old was blacklisted")
                            elif interlockingTable["interlocking-table"][route["id"]]["id"] in blacklist:
                                 print("Route Beibehalten")
                            elif len(interlockingTable["interlocking-table"][route["id"]]["path"]) < len(interlockingTable["interlocking-table"][routeID]["path"]):
                                routeID = route["id"]

                print("{} -> {} | {}".format(signal, destination, routeID))
                if routeID not in blacklist:
                    routes.append(routeID)
                else:
                    print("Route {} wurde ignoriert, weil diese in der Blacklist steht".format(routeID))

        resultData[block["id"]] = {}
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
#                print("Segment: {} at Block {}".format(segments[0], lastBlock))
                stopSegment = segments[0]
            elif len(segments) == 2:
                if segments[0][0:-1] == segments[1][0:-1]:
 #                   print(segments)
                    stopSegment = segments[0][0:-1]
#                    print(stopSegment)
                else:
                    print("Error Segment {} and Segment {} are not the same. Block {}".format(segments[0], segments[1], block["id"]))
                    isError = True
            else:
                print("Error with BlockID {}".format(block["id"]))
                isError = True
            if isError:
                break
            resultData[block["id"]][destination] = {}
            resultData[block["id"]][destination]["route-id"] = routeID
            resultData[block["id"]][destination]["orientation"] = orientation
            resultData[block["id"]][destination]["block"] = lastBlock
            resultData[block["id"]][destination]["segment"] = stopSegment

# Sort
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

with open("../destinations-swtbahn-full.json", "w") as file:
    file.write("const allPossibleDestinationsSwtbahnFull = ")
    file.write(json.dumps(resultData, indent=2))
    file.write(";")
