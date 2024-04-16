# Open an image file and output the image bytes into another file that stuffs them into an array of C++ uint8_t's
# type.

from PIL import Image
from pprint import pprint

def GetFileName(filePath : str):
    lastSlashIndex = filePath.rfind("/")
    lastPointIndex = filePath.rfind(".")

    return filePath[:lastPointIndex] if lastSlashIndex == -1 else filePath[lastSlashIndex + 1:lastPointIndex]

def main(filePath : str, cppVariableNameNog_Prefix : str):
    fileName = GetFileName(filePath)

    stringToPrint = ""
    with Image.open(filePath) as image:
        byteData = image.tobytes()

        if byteData:
            byteString = ", ".join([f"0x{byte:02x}" for byte in byteData])

            stringToPrint = f"const uint8_t g_{cppVariableNameNog_Prefix}[] = \n{{\n{byteString}\n}};"

    with open(f"{fileName}.embed", "w+") as outputFile:
        outputFile.write(stringToPrint)

# Provide the file path with forward slashe NOT BACK SLASHES
if __name__ == "__main__":
    main("Tester/qiyana.jpg", "QiyanaIcon")