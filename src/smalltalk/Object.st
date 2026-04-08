"Object — root of the class hierarchy"

!Object methodsFor: 'comparing'!
== anObject
    <primitive: 12>
    ^ false
!

!Object methodsFor: 'introspection'!
basicClass
    <primitive: 13>
    self error: 'basicClass primitive failed'
!

class
    ^ self basicClass
!

isNil
    ^ false
!

hash
    <primitive: 14>
    ^ 0
!

!Object methodsFor: 'message sending'!
perform: aSelector
    <primitive: 17>
    self error: 'perform: primitive failed'
!

!Object methodsFor: 'debugging'!
halt
    <primitive: 18>
    ^ self
!

error: aString
    <primitive: 29>
    ^ self halt
!

!Object methodsFor: 'convenience'!
yourself
    ^ self
!
