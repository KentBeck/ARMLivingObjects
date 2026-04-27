"Object — root of the class hierarchy"

nil subclass: #Object instanceVariableNames: ''

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

!Object class methodsFor: 'exceptions'!
signal: aString
    <primitive: 37>
    ^ nil
!

!Object methodsFor: 'convenience'!
yourself
    ^ self
!
