"Class — metaclass with method dictionary and instance layout"
"Instance variables: superclass, methodDict, instSize, instFormat"

basicNew
    <primitive: 8>

basicNew: size
    <primitive: 10>

new
    ^ self basicNew

new: size
    ^ self basicNew: size
