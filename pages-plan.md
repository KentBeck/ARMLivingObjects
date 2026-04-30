# Object Pages Plan

Goal:
- move persistence from whole-image checkpointing to page-based checkpointing
- make checkpoint cost proportional to dirty pages
- preserve the current object model as much as possible while changing storage underneath

## 1. First Page Model

Start simple:

- fixed-size pages
- one page table
- one metadata/root page
- objects remain normal in-memory objects
- persistent storage is page-oriented, not object-oriented

First version constraints:

- no compaction across pages
- no variable page sizes
- no fancy generations
- no concurrent flushing

Questions to settle first:

- page size: probably `4 KB` or `8 KB`
- an object cannot straddle pages in v1
- page identity is stable across checkpoints
- each page has:
  - page id
  - dirty bit
  - used bytes
  - checksum/version later

## 2. Separate Logical Objects From Physical Persistence

Do not redesign the object model yet.

Keep:

- normal OOP pointers in memory
- current execution semantics
- current transaction log shape for now

Add:

- mapping from in-memory object address to owning page
- mapping from page id to persisted page bytes

That gives a transition path:

- runtime still works mostly as it does now
- persistence stops depending on dumping the entire heap

## 3. Page Metadata

Add a page table structure in memory.

Per page track:

- page id
- start address
- capacity
- used bytes
- dirty flag
- maybe page type later

Global metadata:

- list/table of all persistent pages
- root object references needed for restart
- schema/version marker for file format

The first checkpoint file format should be simple:

- file header
- root metadata
- page table entries
- raw page bodies

## 4. Allocate Objects Into Pages

Change allocation policy only as much as needed.

First version:

- allocate persistent objects into a current page until full
- then move to a new page
- large objects may need:
  - dedicated whole pages, or
  - temporary “not yet supported” restriction

Important first rule:

- object placement should be deterministic enough to test

Do not optimize placement yet. Just get:

- page ownership
- page fullness
- page creation

## 5. Dirty Tracking

This is the core feature.

Any committed write to a persistent object should:

- mark that object’s page dirty

This applies to:

- field writes
- byte writes
- indexed writes
- writes applied during transaction commit
- replayed journal writes

Important boundary:

- transaction logging remains logical
- dirty tracking remains physical

So commit flow becomes:

1. apply logical writes
2. mark affected pages dirty

## 6. First Checkpoint Semantics

Replace whole-image checkpoint with page checkpoint for persistent pages.

Checkpoint should:

- write metadata/root info
- write only dirty pages
- leave clean pages untouched in the persistent store
- clear dirty flags only after successful write

For the very first slice, it is acceptable if checkpoint still rewrites:

- metadata
- page table
- dirty pages

That is already a huge step up from rewriting the whole heap.

## 7. Restart / Load

Restart should:

- read page metadata
- map/load pages into memory
- restore roots
- reconstruct in-memory page table
- then replay the durable journal if needed

So recovery path becomes:

1. load checkpointed pages
2. restore roots/page table
3. replay durable journal
4. resume normal execution

That aligns with the transaction work already done.

## 8. Tests To Write First

Before much implementation, add tests for:

Allocation/page ownership:

- objects get assigned to pages
- new page allocated when one fills
- object does not straddle pages in v1

Dirty tracking:

- field write marks page dirty
- commit marks touched pages dirty
- untouched pages remain clean

Checkpoint behavior:

- clean checkpoint writes zero dirty pages
- one changed object writes one dirty page
- two changed objects on same page write one page
- two changed objects on different pages write two pages

Restart behavior:

- checkpoint/load preserves object fields
- checkpoint + durable replay preserves post-checkpoint durable commit
- clean pages survive unchanged across incremental checkpoints

These should mostly be C tests first.

## 9. Suggested Slice Order

1. Metadata-only slice
- define page structs and file format
- no behavior change yet
- tests for page bookkeeping only

2. Allocation slice
- allocate objects into tracked pages
- page ownership queries
- tests for placement/fullness

3. Dirty-page slice
- mark pages dirty on committed writes
- tests for dirty tracking

4. Incremental checkpoint slice
- write metadata + dirty pages only
- tests for page-count behavior

5. Restart slice
- load page-based image
- replay durable journal
- tests for restart correctness

6. Cleanup/integration slice
- reconcile old whole-image code paths
- decide what remains as bootstrap/debug tooling

## 10. Design Decisions To Make Now

I would explicitly choose these now:

- fixed-size pages
- no object spanning pages in v1
- no savepoints/concurrency work during page implementation
- checkpoint granularity is pages
- transactions stay as they are until pages work
- restart still replays the current durable journal

That keeps the project moving.

## Definition Of Done For First Page Milestone

You are done with the first page milestone when:

- objects live on tracked pages
- committed writes mark pages dirty
- checkpoint writes only dirty pages plus metadata
- restart reconstructs the image from pages
- journal replay still works on top
- tests prove checkpoint cost is page-proportional, not heap-proportional
