I'm a beginner and by developing the application that I'm going to describe, my hope is to develop my C programming skills and maybe one day reach a professional level.

Introduction to the application.
This is a C application that manages data in Balanced + TREE structures where data is saved in leaf nodes and primary key values are saved in internal nodes.
It manages insert, erase and search operations; only whole primary key or partial primary key searches do not result in a full table scan.
Operations can be either processed in a sessionless mode, without the support of a cache but committed immediately, or they can be executed in a session mode, 
possibly sped up by a cache, and not committed immediately but according to appropriate strategies; in this second mode, a maximum of ten different B+Tree Structures at a time can be managed by the cache system.

Balanced + Tree structure.
On this, I tried to follow the beaten path, or at least I hope I did; I looked up for information on Internet, and I believed I came up with a comoon Balanced + Tree.
But, as mentioned in the introduction, I saved the data in the leaf nodes and not in a separate heap table. 
In addition, contrary to the examples I found on the web, I managed to save multiple internal nodes per single page.

Source organisation.
I tried to follow a modular programming style. The foundation layer of this application is the pager.c module; on top of this is the node.c module and finally there are the bptree*.c modules.
The pager.c module deals with all that has to do with I/O operations and cache system. Data is either committed immediately to the database or, in session mode, it is kept in the RAM and only persisted when necessary or at the end of the session. To be precise, given that even single insert, erase and search operations need several nodes/pages, and given that cache unit is a page, even in sessionless mode, at any given time, the cache can hold several nodes/pages before the end of operation flushes data changes to the physical file, therefore making the cache functioning also as a sort of workspace.
The function of the node.c module is to define node structure, to provide methods to upper layers to access node fields, and finally here I implemented the multiplicity of internal nodes in a single page. Node structure is defined in terms of field offset and size; the node layer provides methods to upper layers to access node fields interposing itself in between upper layers and pager layer, and does nothing else but provide the start point and the length of each field, pointing at the 
pages/nodes provided by the lower layer (the pager); finally the node layer envelops the page provided by the pager with additional information to localise internal nodes inside the page (specifically through what I called section).
Finally the bptree*.c modules where I implemented the erase, insert and search operations through Balanced + Tree structures. Here it can be found the logic of these three operations, abstracted from all the logical/physical management of nodes and page. The bptree.c implements the open/close sessions while delegating the other operations to the other two bptree*.c modules; the bptreeS.c modules implements searches while the bptreeIE.c implements erase and insert operations.
Each component has a corrispondent *.h file where the public methods are defined.

Client.
The user of this application needs to implement, besides the client program (I provided client.c as an example), a module for each table that wants to manage with this application. A bankT.c and cardT.c examples are provided. In fact the application does not nothing about the data structure and size and the key structure and size that must be defined in these intermediate modules.




