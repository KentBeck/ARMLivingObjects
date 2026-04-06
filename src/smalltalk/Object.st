"Object — root of the class hierarchy"

!Object methodsFor: 'comparing'!
== anObject
    <primitive: 12>
!

!Object methodsFor: 'introspection'!
basicClass
    <primitive: 13>
!

class
    ^ self basicClass
!

hash
    <primitive: 14>
!

!Object methodsFor: 'message sending'!
perform: aSelector
    <primitive: 17>
!

!Object methodsFor: 'debugging'!
halt
    <primitive: 18>
!

!Object methodsFor: 'convenience'!
yourself
    ^ self
!
