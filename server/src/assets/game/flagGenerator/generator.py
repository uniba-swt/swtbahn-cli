import json, csv

from diceGenerator import generateDice
colors = {
    "red": "FF0000",
    "blue": "0000ff",
    "green": "00ff00",
    "black": "000000",
    "white": "ffffff"
}

color = json.loads(json.dumps(colors))

def getColorCode(color):
    print("Searching color: {}".format(color))
    try:
        return colors[color]
    except KeyError:
        return "-1"

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
        filename = colorDefinition + ".svg"
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

        backgroundColor = getColorCode(backgroundColor)
        borderColor = getColorCode(borderColor)
        fillColor = getColorCode(fillColor)

        print("Executing: generateDice({}, {}, {}, {}, {})".format(backgroundColor, borderColor, fillColor, number, filename))
        generateDice(backgroundColor, borderColor, fillColor, number, filename)

        signalName = "signal{}".format(str(signal))
        jsonString[signalName] = {}
        jsonString[signalName]["file"] = filename
        jsonString[signalName]["altTag"] = "{} - {}".format(signalName, colorDefinition)

    with open("SignalFlag.json", "w") as file:
        jsonString = json.dumps(jsonString, indent=4)
        file.write(jsonString)

