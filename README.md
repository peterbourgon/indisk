indisk — Wikipedia dump indexer
===============================

Usage
-----

The **indexer** takes an input XML file and an output basename. It will create
one or more index files using the basename as a common prefix.

The **reader** commandline program takes one or more index files, and parses
them into memory. It provides a trivial CLI for performing single-word queries
against that parsed index.

**server.py** provides a webserver on port 8080, which performs the same task
as the reader commandline program, but in a browser.


Assumptions
-----------

1. It's not much fun to just wrap a Lucene (or whatever) instance, so to keep
things interesting I'll be writing the entire indexing mechanism from scratch,
including the encoding format. (I would probably not do this In Real Life.)

2. We only need a simple index on words -- no stems, phrases, or complex
boolean operations.


Limits
------

* The number of returned results is #defined to 10, but it's trivially
changeable. Pagination of the complete result set is relatively
straightforward.


Performance
-----------

Performance is largely dictated by sequential IO throughput. On my 2008 MacBook
Pro with a 5400RPM spinning disk, I can index approximately 1200 articles per
second (APS) peak, 600 APS sustained. On my 2011 MacBook Air with a SSD, I can
index approximately 3000 APS peak, 1500 APS sustained.

The complete enwiki.xml contains approximately 12 million "articles" by our
definition (ie. title tags), so with a SSD, the full index takes me
approximately 2 hours. For a proof of concept, more immediate results may be
obtained by using the [Simple Wiki] [1] text dump, which is two orders of
magnitude smaller (approximately 140000 articles) and can be indexed on the
same hardware in under a minute.

  [1]: http://dumps.wikimedia.org/simplewiki/latest/simplewiki-latest-pages-articles.xml.bz2


Semi-structured thoughts on design
----------------------------------

Good searching is all about leveraging the constraints of the problem space to
cheat in intelligent ways. The Wikipedia problem is interesting because it has
constraints you don't normally see in real-life systems:

* a static data set in a single file
* that's really huge
* and that you need to search on a desktop-class machine

Search backends that are expected to, for example, handle a stream of incoming
or changing data, are necessarily structured differently to deal with that
requirement.  Solving problems of consistency and responsiveness in a
maintainable way -- most likely in a large, distributed system -- is a very
different (and harder) problem than simply efficiently searching 35GB of text
with 4GB of RAM.

Searching means building an inverted index, from content to container. I first
thought that a tailor-made data structure like a [PATRICIA Trie] [1] would be
sufficient, but [my first-pass implementation] [2] wasn't nearly
memory-efficient enough.

  [1]: http://gcc.gnu.org/onlinedocs/libstdc++/ext/pb_ds/trie_based_containers.html
  [2]: http://github.com/peterbourgon/patrie

So I went back to the drawing board and came up with a simple encoding scheme.
A few iterations, accounting for problems with large amounts of data in not
large amounts of RAM, yielded the current implementation. Details on request :)

There are lots of areas for improvement. From the lowest level to the highest,

 * the XML parsing could probably get 50% faster with optimizations
 * the indexer could benefit from smarter synchronization policies
 * I could better schedule (stagger) flushes to disk
 * the indexer could save progress, if interrupted
 * a post-process could unify index files, and save disk space (guessing 30%?)
 * the reader can better parallelize index file parsing
 * the reader can more efficiently represent the index in memory
 * the web UI could use some polish :)

