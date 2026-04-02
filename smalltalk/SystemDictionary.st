"SystemDictionary bootstrap support"

initializeSmalltalkNamespace
    | globals |
    globals := Dictionary new.
    globals at: #Smalltalk put: globals.
    ^ globals

