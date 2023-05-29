/* card.h */

struct bank_table {
    int bank_abi;
    char bank_desc[20];
};

extern void bank_open_db(const char* filename, bool readOnly); 
extern void bank_close_db();
extern int bank_insert(struct bank_table* data);
extern int bank_erase(int abi);
extern struct bank_table* bank_search_key (int abi);
extern int bank_update (int abi, struct bank_table d);
extern int bank_traverse (const char* filename);
