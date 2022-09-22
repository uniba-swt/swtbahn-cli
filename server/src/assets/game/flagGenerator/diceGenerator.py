import os

if not os.path.isdir("dice"):
    os.mkdir("dice")

# Circles
# 1 X 2
# 3 4 5
# 6 x 7

# Cube 1 (4)
# - - -
# - x -
# - - -

# Cube 2  (1, 7)
# x - -
# - - -
# - - x

# Cube 3 (1, 4, 7)
# x - -
# - x -
# - - x

# Cube 4 (1, 2, 6, 7)
# x - x
# - - -
# x - x

# Cube 5 (1, 2, 4, 6, 7)
# x - x
# - x -
# x - x

# Cube 6 (1, 2, 3, 5, 6, 7)
# x - x
# x - x
# x - x

numberToDot = [
    [],  # 0DotDice
    [4],  # 1DocDice
    [1, 7],  # 2DocDice
    [1, 4, 7],  # 3DocDice
    [1, 2, 6, 7],  # 4DocDice
    [1, 2, 4, 6, 7],  # 5DocDice
    [1, 2, 3, 5, 6, 7]  # 6DocDice
]

DotToPosition = [
    [],
    [200, 200],
    [800, 200],
    [200, 500],
    [500, 500],
    [800, 500],
    [200, 800],
    [800, 800]
]


def generateDice(backgroundColor, borderColor, fillcolor, number, fileName=None):
    if fileName is None:
        filename = "dice/{}_{}.svg".format(backgroundColor, str(number))
    else:
        filename = "dice/{}".format(fileName)

    fileString = """<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg"
    xmlns:xlink="http://www.w3.org/1999/xlink"
    version="1.1" baseProfile="full"
    width="1000" height="1000">"""

    background = """  <rect fill="#{}" x="0" y="0" width="1000" height="1000" />
  <rect fill="#{}" x="5" y="5" width="990" height="990" />""".format(borderColor, backgroundColor)

    fileString += background
    number = int(number)
    if number < 0:
        print("Number can not be negative")
        return
    if number > 6:
        print("Number to high")
        return

    dots = numberToDot[number]

    for dot in dots:
        fileString += """<circle cx="{}" cy="{}" r="100" fill="#{}" />""".format(str(DotToPosition[dot][0]),
                                                                                 str(DotToPosition[dot][1]),
                                                                                 borderColor)
        fileString += """<circle cx="{}" cy="{}" r="90" fill="#{}" />""".format(str(DotToPosition[dot][0]),
                                                                                str(DotToPosition[dot][1]), fillcolor)

    fileString += "</svg>"

    with open(filename, "w") as file:
        file.write(fileString)

generateDice("00ff00", "000000", "ffffff", 3)