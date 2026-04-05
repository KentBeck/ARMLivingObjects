"Association — key/value pair"

!Association methodsFor: 'accessing'!
key
    ^ key
!

key: anObject
    key := anObject.
    ^ anObject
!

value
    ^ value
!

value: anObject
    value := anObject.
    ^ anObject
!

!Association methodsFor: 'initialization'!
key: aKey value: aValue
    key := aKey.
    value := aValue.
    ^ self
!
