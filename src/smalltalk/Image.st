"Image — persistence/checkpoint entry points"

Object subclass: #Image instanceVariableNames: ''

!Image class methodsFor: 'checkpointing'!
checkpointTo: aPath
    <primitive: 42>
    self error: 'checkpointTo: not implemented'
!

restartFrom: aPath valueOfGlobal: aKey
    <primitive: 43>
    self error: 'restartFrom:valueOfGlobal: not implemented'
!
