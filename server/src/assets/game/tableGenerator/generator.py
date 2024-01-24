import json
import csv

def numberToWord(number):
    return ["Zero", "One", "Two", "Three", "Four", "Five", "Six"][int(number)]

def characterToColor(character):
	return {"b": "Blue", "g": "Green", "r": "Red", "s": "Black"}[character]

alias = input("What train is used (alias): ")
filestring = input("Where is the Signal Mapping csv? ")
# Load Match to Signal into File
with open(filestring, "r") as f:
    reader = csv.reader(f)
    jsonString = {}
    for row in reader:
        signal = row[0]
        colorDefinition = row[1]
        number = colorDefinition[-1]

        endString = "flagTheme" + characterToColor(colorDefinition[0])

        if colorDefinition[1] == "w":
            endString += " flagOutline "
        else:
            endString += " flagFilled "

        if int(number) == 6:
            number = 0

        endString += "flag" + numberToWord(number)
        print(endString)
        signalName = "signal{}".format(str(signal))
        jsonString[signalName] = endString

    with open("../flags-swtbahn-standard.json", "w") as file:
        file.write("const signalFlagMap{} = ".format(alias))
        file.write(json.dumps(jsonString, indent=2))
        file.write(";")

