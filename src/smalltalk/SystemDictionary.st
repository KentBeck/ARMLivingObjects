"SystemDictionary bootstrap support"

!SystemDictionary methodsFor: 'initialization'!
initializeSmalltalkNamespace
    | globals |
    globals := Dictionary new.
    globals at: #Smalltalk put: globals.
    ^ globals
!
