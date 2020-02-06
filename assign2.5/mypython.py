import os
import string
import random


def generateFileString():
	writeString =  ''.join(random.choice(string.ascii_lowercase) for _ in range(10))
	print(writeString)
	writeString += '\n'
	return writeString


def doMath():
	int1 = random.randint(1,42)
	int2 = random.randint(1,42)
	print("%d \n%d \n%d" %(int1, int2, int1*int2))

def main():
	for i in range(3):
		with open("outfile_" + str(i) + ".txt", "w+") as writeFile:
			writeFile.write(generateFileString())
	doMath()

if __name__ == "__main__":
	main()
