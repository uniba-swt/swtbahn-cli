import json
import csv

def numberToWord(number):
    if number == 1:
        return "one"
    elif number == 2:
        return "second"
    elif number == 3:
        return "third"
    elif number == 4:
        return "fourth"
    elif number == 5:
        return "fifth"
    elif number == 6:
        return "sixth"

# Load Match to Signal into File
with open("SignalToFlag.csv", "r") as f:
    reader = csv.reader(f)
    jsonString = {}
    for row in reader:
        signal = row[0]
        colorDefinition = row[1]
        backgroundColor = None
        fillColor = "white"
        borderColor = None
        print(colorDefinition)
        number = colorDefinition[-1]
        if colorDefinition[0] == "b":
            backgroundColor = "blue"
            borderColor = "blue"
        elif colorDefinition[0] == "g":
            backgroundColor = "green"
            borderColor = "green"
        elif colorDefinition[0] == "r":
            backgroundColor = "red"
            borderColor = "red"
        elif colorDefinition[0] == "s":
            backgroundColor = "black"
            borderColor = "black"
        if colorDefinition[1] == "w":
            fillColor = backgroundColor
            backgroundColor = "white"

        if number == 6:
            number = 0

        signalName = "signal{}".format(str(signal))
        jsonString[signalName] = {}
        jsonString[signalName]["fillColor"] = fillColor
        jsonString[signalName]["backgroundColor"] = backgroundColor
        jsonString[signalName]["cssClassForColor"] = "bg-{}-{}".format(backgroundColor, fillColor)
        jsonString[signalName]["number"] = number
        jsonString[signalName]["numberClass"] = "{}-face".format(numberToWord(int(number)))

    with open("SignalFlag.json", "w") as file:
        jsonString = json.dumps(jsonString, indent=4)
        file.write(jsonString)

