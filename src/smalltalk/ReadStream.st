"ReadStream — sequential read over a collection"

Object subclass: #ReadStream instanceVariableNames: 'collection position readLimit'

!ReadStream class methodsFor: 'instance creation'!
on: aCollection
    ^ self new initializeOn: aCollection
!

!ReadStream methodsFor: 'initialization'!
initializeOn: aCollection
    collection := aCollection.
    position := 1.
    ^ self
!

!ReadStream methodsFor: 'accessing'!
position
    ^ position
!

atEnd
    ^ (position < (collection size + 1))
        ifTrue: [false]
        ifFalse: [true]
!

peek
    ^ self atEnd
        ifTrue: [nil]
        ifFalse: [collection at: position]
!

next
    | value |
    ^ self atEnd
        ifTrue: [nil]
        ifFalse: [
            value := collection at: position.
            position := position + 1.
            value
        ]
!

upToEnd
    | result |
    result := String new: ((collection size - position) + 1).
    self copyTo: result sourceIndex: position destIndex: 1.
    position := collection size + 1.
    ^ result
!

!ReadStream methodsFor: 'private'!
copyTo: aString sourceIndex: sourceIndex destIndex: destIndex
    ^ (sourceIndex < (collection size + 1))
        ifTrue: [
            aString at: destIndex put: (collection at: sourceIndex).
            self copyTo: aString sourceIndex: sourceIndex + 1 destIndex: destIndex + 1
        ]
        ifFalse: [aString]
!
