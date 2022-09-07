import json, yaml

inputFileName = "./interlocking_table.yml"
outputFileName = "./destination_table.json"

combinations = []


def sortTable(table):
    table = sorted(table, key=lambda x: x[0])
    return table


def generateOutputJson(table):
    output = {}
    index = ""
    for row in table:
        if index != row[0]:
            index = row[0]
            output[index] = {}
            output[index][row[1]] = {}
            output[index][row[1]]["routeID"] = row[2]
        else:
            try:
                if output[index][row[1]] is not None:
                    if input("Ersetzen Route {} durch Route {} ({} => {})".format(output[index][row[1]]["routeID"], row[2], row[0], row[1])) == "y":
                        output[index][row[1]]["routeID"] = row[2]
            except KeyError:
                output[index][row[1]] = {}
                output[index][row[1]]["routeID"] = row[2]


    with open(outputFileName, 'w') as jsonFile:
        jsonFile.write(json.dumps(output))


def extraction(content):
    """
    Extracts the destination source form the interlocking table
    """
    jsonString = json.dumps(content)
    jsonObject = json.loads(jsonString)
    inner_layer = jsonObject['interlocking-table']

    for layer in inner_layer:
        source = layer['source']
        destination = layer['destination']
        routeID = layer['id']
        combinations.append((source, destination, routeID))
    return combinations


with (open(inputFileName, 'r') as stream):
    try:
        combinationMethods = extraction(yaml.safe_load(stream))
        sortedTable = sortTable(combinationMethods)
        generateOutputJson(sortedTable)
    except yaml.YAMLError as exception:
        print(exception)
