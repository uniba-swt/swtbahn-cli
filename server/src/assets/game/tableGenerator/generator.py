import json, csv, os

def numberToWord(number: int):
    return ["Zero", "One", "Two", "Three", "Four", "Five", "Six"][number]

def characterToColor(character):
	return {"b": "Blue", "g": "Green", "r": "Red", "s": "Black"}[character.lower()]

groupingFileDirectory = "./flagMappings"
    
mappingFolderContent = os.scandir(groupingFileDirectory)
for entry in mappingFolderContent:
    if entry.is_file() and entry.name[-4:] == ".csv":
        with open("{}/{}".format(groupingFileDirectory, entry.name), "r") as f:
            print("Generating signal flag json for " + entry.name[:-4])
            reader = csv.reader(f)
            jsonString = {}
            for row in reader:
                signal = row[0]
                colorDefinition = row[1]

                # [-1] -> last character of color def. string
                diceValueNum = int(colorDefinition[-1])
                if diceValueNum >= 6 or diceValueNum <= -1:
                    diceValueNum = 0

                signalSymbolCombiStr = "flagTheme" + characterToColor(colorDefinition[0])

                if colorDefinition[1] == "w":
                    signalSymbolCombiStr += " flagOutline "
                else:
                    signalSymbolCombiStr += " flagFilled "

                signalSymbolCombiStr += "flag" + numberToWord(diceValueNum)
                print(signalSymbolCombiStr)
                signalName = "signal{}".format(str(signal))
                jsonString[signalName] = signalSymbolCombiStr

            print("Now writing signal flag json to file")
            with open("../flags-{}.json".format(entry.name[:-4]), "w") as file:
                file.write("const signalFlagMap_{} = ".format(entry.name[:-4].replace("-", "")))
                file.write(json.dumps(jsonString, indent=2))
                file.write(";")
            print("Finished writing signal flag json (" + "flags-{}.json".format(entry.name[:-4]) + ")")
