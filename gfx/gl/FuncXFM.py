import re

# Groups:             1    2          3
_kRegex = re.compile('(.+) ([^( ]+)[(](.*)[)]')

class CFuncList:
    def __init__(self, path):
        self.parsedList = []

        with open(path, 'rb') as f:
            lineNum = 0;
            for line in f:
                lineNum += 1

                line = line.strip()

                if not line:
                    continue

                match = _kRegex.search(line)
                assert match, '{}:{}: {}'.format(path, lineNum, line)

                retType = match.group(1)
                funcName = match.group(2)
                args = match.group(3)

                argList = []

                if args.strip():
                    for x in args.split(','):
                        split2 = x.rsplit(' ', 1)
                        assert len(split2) == 2, '{}:{}: {}'.format(path, lineNum, line)

                        (type, name) = split2
                        assert name[0] != '*', '{}:{}: {}'.format(path, lineNum, line)

                        argList.append( (type.strip(), name.strip()) )
                        continue

                self.parsedList.append( (funcName, argList, retType) )
                continue

        return


    def Sort(self, fnSortKey):
        self.parsedList = sorted(self.parsedList, key=fnSortKey)
        return


    def Transform(self, fnTransform):
        outputListDict = dict()

        for x in self.parsedList:
            outputDict = fnTransform(*x)

            for (key, output) in outputDict.iteritems():
                if not output:
                    continue

                if key not in outputListDict:
                    outputListDict[key] = []

                outputListDict[key].append(output)
                continue
            continue

        return outputListDict

########################################

def WriteLines(f, lines):
    for line in lines:
        f.write(line + '\n')
        continue
    return


def WriteOutputListDict(outputListDict, path):
    with open(path, 'wb') as f:
        for (key, outputList) in outputListDict.iteritems():
            f.write('/' * 80 + '\n')
            f.write('// ' + key + '\n\n')

            WriteLines(f, outputList)

            f.write('\n')
            continue
    return
