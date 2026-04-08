"BlockClosure — closure object with home receiver and compiled method"

!BlockClosure methodsFor: 'evaluating'!
value
    <primitive: 6>
    self error: 'value primitive failed'
!

value: anArg
    <primitive: 16>
    self error: 'value: primitive failed'
!

cannotReturn: aValue
    self error: 'cannot return from terminated block'
!
