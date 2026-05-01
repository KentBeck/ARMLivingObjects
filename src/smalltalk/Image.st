"Image — persistence/checkpoint entry points"

Object subclass: #Image instanceVariableNames: ''

!Image class methodsFor: 'checkpointing'!
checkpointTo: aPath
    <primitive: 42>
    ^ Error signal: 'checkpoint failed'
!

restartFrom: aPath valueOfGlobal: aKey
    <primitive: 43>
    ^ Error signal: 'restart failed'
!
