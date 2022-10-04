import json
import csv

def numberToWord(number):
    return ["Zero", "One", "Two", "Three", "Four", "Five", "Six"][int(number)]

# Load Match to Signal into File
with open("SignalToFlag.csv", "r") as f:
    reader = csv.reader(f)
    jsonString = {}
    for row in reader:
        signal = row[0]
        colorDefinition = row[1]
        number = colorDefinition[-1]

        endString = "flagTheme"

        if colorDefinition[0] == "b":
            color = "Blue"
        elif colorDefinition[0] == "g":
            color = "Green"
        elif colorDefinition[0] == "r":
            color = "Red"
        elif colorDefinition[0] == "s":
            color = "Black"

        endString = endString + color + " "


        if colorDefinition[1] == "w":
            endString = endString + "flagOutline "
        else:
            endString = endString + "flagFilled "


        if number == 6:
            number = 0

        endString = endString + "flag" + numberToWord(number)
        print(endString)
        signalName = "signal{}".format(str(signal))
        jsonString[signalName] = endString

    with open("SignalFlag.json", "w") as file:
        jsonString = json.dumps(jsonString, indent=4)
        file.write(jsonString)

