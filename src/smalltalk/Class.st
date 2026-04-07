"Class — metaclass with method dictionary and instance layout"
"Instance variables: superclass, methodDict, instSize, instFormat"

!Class methodsFor: 'instance creation'!
basicNew
    <primitive: 8>
    self error: 'basicNew primitive failed'
!

basicNew: size
    <primitive: 10>
    self error: 'basicNew: primitive failed'
!

new
    ^ self basicNew
!

new: size
    ^ self basicNew: size
!
