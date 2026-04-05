"Collection protocol — implemented on indexable and byte-indexable objects"

!Collection methodsFor: 'accessing'!
size
    <primitive: 11>
!

at: index
    "1-based. Dispatches on format: indexable → word, bytes → byte as SmallInt"
    <primitive: 7>
!

at: index put: value
    "1-based. Dispatches on format: indexable → word, bytes → SmallInt byte"
    <primitive: 9>
!

basicAt: index
    <primitive: 7>
!

basicAt: index put: value
    <primitive: 9>
!
