# pyqdbm
python binding for qdbm library. (implemented only depot)

This module requires QDBM C Library (http://fallabs.com/qdbm/index.html)

Example code:

```py
from qdbm import depot

db = depot.open("test.db", "n") # depot.open(FILENAME, FLAG)

db["apple"] = "red"       # add data
db["lemon"] = "black"
db["orange"] = "orange"

db["lemon"] = "yellow"    # update data

print db["lemon"]   # get value that key is "lemon"

print db.get("orange", "unknown")  # get data with default value (returns orange)
print db.get("melon", "unknown")   # get data with default value (returns unknown)

print db.keys()               # get list of keys (Python2), get iterator of keys (Python3)

print db.listkeys()           # get list of keys (Python3)

for k in db.iterkeys():       # get iterator of keys (Python2)
    print k

for k in db.keys():           # get iterator of keys (Python3)
    print k

for k, v in db.iteritems():   # get iterator of (key, value) (Python2)
    print k, v

for k, v in db.items():       # get iterator of (key, value) (Python3)
    print k, v

for v in db.itervalues():     # get iterator of values (Python2)
    print v

for v in db.values():         # get iterator of values (Python3)
    print v

db.close()                    # close database object
```

Flags:
- r: Read Only
- w: Read / Write
- c: Read / Write (create if not exists)
- n: Read / Write (always create new file)


See also https://www.hirano.cc/pyqdbm