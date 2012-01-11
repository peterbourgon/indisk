indisk
======

**indisk** is a arbitrary name for a Wikipedia dump indexer.


Usage
-----

The software was tested on Mac OS 10.7 (Lion) and Debian Linux 6 (Squeeze). It
requires gcc (g++) 4.2 or higher, make, python2.7, and probably a few other
things I'm forgetting. You should be able to just type "make" and everything
will compile; if not, file an issue.

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

3. Common stop words (the, and, but, etc.) are not indexed.

4. Special pages (Category:, Wikipedia:, Special:, etc.) are not indexed.

5. Contributors are only indexed if they have a username -- no IP addresses.


Limits
------

1. The number of returned results is #defined to 10, but it's trivially
changeable. Pagination of the complete result set is relatively
straightforward.

2. The maximum number of terms in an index set is constrainted to UINT32 MAX,
or about 4.2 billion. Actually, less, as a function of the number of articles
allowed in a single index file.

3. An input file may be split into a maximum of 64 regions. In the current
implementation, this means the maximum number of index threads is also 64.

4. The maximum size for the title, contributor, and article text regions are
1KB, 1MB and 100MB respectively.

Performance
-----------

Performance is largely dictated by sequential IO throughput. On my 2008 MacBook
Pro with a 5400RPM spinning disk, I can index approximately 1200 articles per
second (APS) peak, 600 APS sustained. On my 2011 MacBook Air with a SSD, I can
index approximately 3000 APS peak, 1500 APS sustained. That translates to about
20GB per hour; not as good as [Mike McCandless' highly-optimized Lucene][1] at
96GB per hour, but not completely out of the ballpark. Interestingly, he
reports that his resulting index is 6.9GB in memory; about the same as mine.

 [1]: http://blog.mikemccandless.com/2010/09/lucenes-indexing-is-fast.html

The complete enwiki.xml contains approximately 12 million "articles" by our
definition (ie. title tags), so with a SSD, the full index takes me
approximately 2 hours. For a proof of concept, more immediate results may be
obtained by using the [Simple Wiki][1] text dump, which is two orders of
magnitude smaller (approximately 140000 articles) and can be indexed on the
same hardware in under a minute.

 [1]: http://dumps.wikimedia.org/simplewiki/latest/simplewiki-latest-pages-articles.xml.bz2

Searching performance is Pretty Good™; on my MacBook Air with 4GB of RAM, the
Simple Wiki dump returns results in less than 10ms, and the complete English
Wiki dump returns results in between 500ms and 800ms when disk swapping is
required, or between 50ms and 80ms when it's not.


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
requirement. Solving problems of consistency and responsiveness in a
maintainable way -- most likely in a large, distributed system -- is a very
different (and harder) problem than simply efficiently searching 35GB of text
with 4GB of RAM.

Searching means building an inverted index, from content to container. I first
thought that a tailor-made data structure like a [PATRICIA Trie][1] would be
sufficient, but [my first-pass implementation][2] wasn't nearly
memory-efficient enough.

 [1]: http://gcc.gnu.org/onlinedocs/libstdc++/ext/pb_ds/trie_based_containers.html
 [2]: http://github.com/peterbourgon/patrie

So I went back to the drawing board and came up with a simple encoding scheme.
A few iterations, accounting for problems with large amounts of data in not
large amounts of RAM, yielded the current implementation. Details on request :)

There are lots of areas for improvement. From the lowest level to the highest,

 * The XML parsing could probably get 50% faster with optimizations
 * The indexer could benefit from smarter synchronization policies
 * I could better schedule (stagger) flushes to disk
 * The indexer could save progress, if interrupted
 * A post-process could unify index files, and save disk space (guessing 30%?)
 * The reader can better parallelize index file parsing
 * The reader can more efficiently represent the index in memory. This is
   significant, as the full enwiki index requires currently ~6GB of memory,
   meaning machines with less than that will swap to disk. OK on SSDs (still
   sub-second); not so great on laptop-class spinning metal (up to 4-5s).
 * The web UI could use some polish :)

