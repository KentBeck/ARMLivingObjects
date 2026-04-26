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

!Class methodsFor: 'introspection'!
superclass
    <primitive: 30>
    ^ nil
!

name
    <primitive: 31>
    ^ nil
!

includesSelector: aSelector
    <primitive: 32>
    ^ false
!

sourceAtSelector: aSelector
    ^ Dictionary smalltalk methodSourceForClass: self name selector: aSelector
!
