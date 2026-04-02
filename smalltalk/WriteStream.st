"WriteStream — sequential writes over a String"

on: aCollection
    ^ self new initializeOn: aCollection

initializeOn: aCollection
    collection := aCollection.
    ^ self

position
    ^ collection size + 1

nextPut: aByte
    | one |
    one := String new: 1.
    one at: 1 put: aByte.
    collection := collection , one.
    ^ aByte

nextPutAll: aCollection
    collection := collection , aCollection.
    ^ aCollection

contents
    ^ collection

