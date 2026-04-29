"Transaction — unified API for in-memory and durable transactions"

Object subclass: #Transaction instanceVariableNames: ''

!Transaction class methodsFor: 'running'!
atomic: aBlock
    <primitive: 40>
    ^ aBlock value
!

readOnly: aBlock
    ^ aBlock value
!

durable: aBlock
    ^ self atomic: aBlock
!
