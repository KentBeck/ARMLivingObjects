"Collection protocol — implemented on indexable and byte-indexable objects"

!Collection methodsFor: 'accessing'!
size
    <primitive: 11>
    self error: 'size primitive failed'
!

at: index
    "1-based. Dispatches on format: indexable → word, bytes → byte as SmallInt"
    <primitive: 7>
    self error: 'at: primitive failed'
!

at: index put: value
    "1-based. Dispatches on format: indexable → word, bytes → SmallInt byte"
    <primitive: 9>
    self error: 'at:put: primitive failed'
!

basicAt: index
    <primitive: 7>
    self error: 'basicAt: primitive failed'
!

basicAt: index put: value
    <primitive: 9>
    self error: 'basicAt:put: primitive failed'
!
