# Solution: Load the rules file and then go through the dates according to the dates in dates-example and 
#   tokenize the rules on the spaces and then check for +/- flags and push/pop in a set to evade duplicates
#   At the end just print the items inside the set

# DATE is addressed as dd-mm-yyyy

# from datetime import datetime
from pprint import pprint

def MakeDateList(date : str) -> list:
    return [int(x) for x in reversed(date.split('-'))]

def IsDateLessThan(date1 : list, date2 : list) -> bool: # for now will take them as list but it is possible to just take them as integers
    if date1[0] < date2[0]:
        return True
    elif date1[0] == date2[0]:
        if date1[1] < date2[1]:
            return True
        elif date1[1] == date2[1]:
            if date1[2] <= date2[2]:
                return True

    return False

def ReadRules(rulesFilePath : str, datesFilePath : str):
    dates = list()
    try:
        with open(datesFilePath, "r") as fileHandle:
            dates = fileHandle.readlines()
    except IOError as error:
        print(f"Error in opening the file! Description: {error}")

    for i in range(len(dates)):
        dates[i] = MakeDateList(dates[i])

    pprint(dates)

    lines = list()
    try:
        with open(rulesFilePath, "r") as fileHandle:
            lines = fileHandle.readlines()

    except IOError as error:
        print(f"Error in opening the file! Description: {error}")

    for dateOuter in dates:
        rulesSet = set()
        for line in lines:
            date, rules = line.rstrip().split(':')
            date = MakeDateList(date)
            if not IsDateLessThan(date, dateOuter):
                break
            # print(date, rules)
            for rule in rules.lstrip().split(' '):
                # print(rule)
                if rule[0] == '+':
                    rulesSet.add(rule[1:])
                elif rule[0] == '-':
                    rulesSet.discard(rule[1:])

        print(rulesSet)

def main():
    ReadRules("TutoringSes_9_1_2023/rules-example1.dat", "TutoringSes_9_1_2023/dates-example1.dat")

if __name__ == '__main__':
    main()