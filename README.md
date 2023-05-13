# mydb
This is a sort of database engine written in C I worked on, hoping to develop my C programming skills and to better understand how database engines work.
Data is saved on disk in a B plus Tree structure in a single file; I mean that both primary key and the rest of data are not in separate files.   
The core of the software is represented by three source files, BPTREE.C, NODE.C and PAGER.C, that I tried to link together in a modular programming tyoe of structure.
BPTREE.C implements node insert, erase, search and traversing functions. Nodes are organised in a B plus Tree structure. Functions in this source file depends on functions in NODE.C file.
NODE.C implements node tyoes structures (internal, leaf and root node types) and provides BPTREE.C with the functions to manipulate node data. It depens on functions in PAGER.C
Finally, PAGER.C manage the reading/writing of the data on the disk, managing a buffer in the process memory to limit the number of accesses to the disk. 
