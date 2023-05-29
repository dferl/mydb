/* card.h */

struct card_table {
    int card_abi;
    int card_id;
    char card_owner_name[20];
};

extern void card_open_db(const char* filename, bool readOnly); 
extern void card_close_db();
extern int card_insert(struct card_table* data);
extern int card_erase(int abi, int card_id);
extern struct card_table* card_search_key (int abi, int card_id);
extern int card_update (int abi, int card_id, struct card_table d);
extern int card_open_cursor_search_abi (int abi);
extern void* card_fetch_search_abi (int cursorIndex);
extern void card_close_cursor_search_abi (int cursorIndex);
extern int card_traverse (const char* filename);
