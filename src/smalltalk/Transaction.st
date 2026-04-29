"Transaction — unified API for in-memory and durable transactions"

Object subclass: #Transaction instanceVariableNames: ''

!Transaction class methodsFor: 'running'!
atomic: aBlock
    <primitive: 40>
    ^ aBlock value
!

readOnly: aBlock
    <primitive: 44>
    ^ aBlock value
!

durable: aBlock
    <primitive: 41>
    ^ self atomic: aBlock
!
