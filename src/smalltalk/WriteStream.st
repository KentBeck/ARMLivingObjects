"WriteStream — sequential writes over a String"

!WriteStream class methodsFor: 'instance creation'!
on: aCollection
    ^ self new initializeOn: aCollection
!

!WriteStream methodsFor: 'initialization'!
initializeOn: aCollection
    collection := aCollection.
    ^ self
!

!WriteStream methodsFor: 'accessing'!
position
    ^ collection size + 1
!

nextPut: aByte
    | one |
    one := String new: 1.
    one at: 1 put: aByte.
    collection := collection , one.
    ^ aByte
!

nextPutAll: aCollection
    collection := collection , aCollection.
    ^ aCollection
!

contents
    ^ collection
!
