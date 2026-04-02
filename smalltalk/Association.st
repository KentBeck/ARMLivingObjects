"Association — key/value pair"

key
    ^ key

key: anObject
    key := anObject.
    ^ anObject

value
    ^ value

value: anObject
    value := anObject.
    ^ anObject

key: aKey value: aValue
    key := aKey.
    value := aValue.
    ^ self

