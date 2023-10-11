#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
using namespace std;

enum MetaCommandResult {
    META_COMMAND_SUCCESS,
    META_COMMAND_UNRECOGNIZED_COMMAND
};

enum PrepareResult {
    PREPARE_SUCCESS, 
    PREPARE_NEGATIVE_ID,
    PREPARE_STRING_TOO_LONG,
    PREPARE_SYNTAX_ERROR,
    PREPARE_UNRECOGNIZED_STATEMENT
};

enum StatementType {
    STATEMENT_INSERT, 
    STATEMENT_SELECT
};

enum ExecuteResult {
    EXECUTE_SUCCESS,
    EXECUTE_TABLE_FULL
};

#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 255

struct Row {
    int id;
    string username;
    string email; 
};

struct Statement {
    StatementType type;
    Row row_to_insert; //only used by insert statement
};

#define size_of_attribute(Struct, Attribute) sizeof(((Struct*)0)->Attribute)

const uint32_t ID_SIZE = size_of_attribute(Row, id);
const uint32_t USERNAME_SIZE = size_of_attribute(Row, username);
const uint32_t EMAIL_SIZE = size_of_attribute(Row, email);
const uint32_t ID_OFFSET = 0;
const uint32_t USERNAME_OFFSET = ID_OFFSET + ID_SIZE;
const uint32_t EMAIL_OFFSET = USERNAME_OFFSET + USERNAME_SIZE;
const uint32_t ROW_SIZE = ID_SIZE + USERNAME_SIZE + EMAIL_SIZE;

const uint32_t PAGE_SIZE = 4096;
#define TABLE_MAX_PAGES 100
const uint32_t ROWS_PER_PAGE = PAGE_SIZE / ROW_SIZE;
const uint32_t TABLE_MAX_ROWS = ROWS_PER_PAGE * TABLE_MAX_PAGES;

struct Pager {
    int file_descriptor;
    uint32_t file_length;
    void* pages[TABLE_MAX_PAGES];
};

struct Table {
    uint32_t num_rows;
    Pager* pager;
};

struct Cursor {
    Table* table; 
    uint32_t row_num;
    bool end_of_table; // Indicates a position one past the last element
};

void print_row(Row* row) {
    cout << "(" << row->id << ", " << row->username << ", " << row->email <<")"<< endl;
}

void serialize_row(Row* source, void* destination) {
    char* dest = static_cast<char*>(destination);

    memcpy(dest + ID_OFFSET, &(source->id), ID_SIZE);
    memcpy(dest + USERNAME_OFFSET, &(source->username), USERNAME_SIZE);
    memcpy(dest + EMAIL_OFFSET, &(source->email), EMAIL_SIZE);
}

void deserialize_row(void* source, Row* destination) {
    const char* src = static_cast<const char*>(source);

    memcpy(&(destination->id), src + ID_OFFSET, ID_SIZE);
    memcpy(&(destination->username), src + USERNAME_OFFSET, USERNAME_SIZE);
    memcpy(&(destination->email), src + EMAIL_OFFSET, EMAIL_SIZE);
}

void* get_page(Pager* pager, uint32_t page_num) {
    if (page_num > TABLE_MAX_PAGES) {
        printf("Tried to fetch page number out of bounds. %d > %d\n", page_num,
     	TABLE_MAX_PAGES);
        exit(EXIT_FAILURE);
    }

    if (pager->pages[page_num] == NULL) {
        // Cache miss. Allocate memory and load from file.
        void* page = malloc(PAGE_SIZE);
        uint32_t num_pages = pager->file_length / PAGE_SIZE;

        // We might save a partial page at the end of the file
        if (pager->file_length % PAGE_SIZE) {
            num_pages += 1;
        }

        if (page_num <= num_pages) {
            lseek(pager->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);
            ssize_t bytes_read = read(pager->file_descriptor, page, PAGE_SIZE);
            if (bytes_read == -1) {
        printf("Error reading file: %d\n", errno);
        exit(EXIT_FAILURE);
            }
        }

        pager->pages[page_num] = page;
    }

    return pager->pages[page_num];
}

Cursor* table_start(Table* table) {
    Cursor* cursor = new Cursor;
    cursor->table = table;
    cursor->row_num = 0;
    cursor->end_of_table = (table->num_rows == 0);

    return cursor;
}

Cursor* table_end(Table* table) {
    Cursor* cursor = new Cursor;
    cursor->table = table;
    cursor->row_num = table->num_rows;
    cursor->end_of_table = true;

    return cursor;
}

void* cursor_value(Cursor* cursor) {
    uint32_t row_num = cursor->row_num;
    uint32_t page_num = row_num / ROWS_PER_PAGE;
    void *page = get_page(cursor->table->pager, page_num);
    uint32_t row_offset = row_num % ROWS_PER_PAGE;
    uint32_t byte_offset = row_offset * ROW_SIZE;
    return static_cast<char*>(page) + byte_offset;
}

void cursor_advance(Cursor* cursor) {
    cursor->row_num += 1;
    if (cursor->row_num >= cursor->table->num_rows) {
        cursor->end_of_table = true;
    }
}

Pager* pager_open(const char* filename) {
    int fd = open(filename,
                    O_RDWR | 	// Read/Write mode
                    O_CREAT,	// Create file if it does not exist
                    S_IWUSR |	// User write permission
                    S_IRUSR	// User read permission
                );

    if (fd == -1) {
        printf("Unable to open file\n");
        exit(EXIT_FAILURE);
    }

    off_t file_length = lseek(fd, 0, SEEK_END);

    Pager* pager = new Pager;
    pager->file_descriptor = fd;
    pager->file_length = file_length;

    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
            pager->pages[i] = NULL;
    }


  return pager;
}

Table* db_open(const char* filename) {
  Pager* pager = pager_open(filename);
  uint32_t num_rows = pager->file_length / ROW_SIZE;

  Table* table = new Table;
  table->pager = pager;
  table->num_rows = num_rows;

  return table;
}

struct InputBuffer {
    string buffer;
    // size_t buffer_length;
    // ssize_t input_length;
};

InputBuffer* new_input_buffer() {
    return new InputBuffer;
}

void read_input(InputBuffer* input_buffer) {
    cout << "db > ";
    getline(cin, input_buffer->buffer);
}

void close_input_buffer(InputBuffer* input_buffer) {
    // delete(input_buffer->buffer);
    delete input_buffer;
}

void pager_flush(Pager* pager, uint32_t page_num, uint32_t size) {
  if (pager->pages[page_num] == NULL) {
     printf("Tried to flush null page\n");
     exit(EXIT_FAILURE);
  }

  off_t offset = lseek(pager->file_descriptor, page_num * PAGE_SIZE,
     		 SEEK_SET);

  if (offset == -1) {
     printf("Error seeking: %d\n", errno);
     exit(EXIT_FAILURE);
  }

  ssize_t bytes_written = write(
     pager->file_descriptor, pager->pages[page_num], size
     );

  if (bytes_written == -1) {
     printf("Error writing: %d\n", errno);
     exit(EXIT_FAILURE);
  }
}

void db_close(Table* table) {
  Pager* pager = table->pager;
  uint32_t num_full_pages = table->num_rows / ROWS_PER_PAGE;

  for (uint32_t i = 0; i < num_full_pages; i++) {
     if (pager->pages[i] == NULL) {
         continue;
     }
     pager_flush(pager, i, PAGE_SIZE);
     free(pager->pages[i]);
     pager->pages[i] = NULL;
  }

  // There may be a partial page to write to the end of the file
  // This should not be needed after we switch to a B-tree
  uint32_t num_additional_rows = table->num_rows % ROWS_PER_PAGE;
  if (num_additional_rows > 0) {
     uint32_t page_num = num_full_pages;
     if (pager->pages[page_num] != NULL) {
         pager_flush(pager, page_num, num_additional_rows * ROW_SIZE);
         free(pager->pages[page_num]);
         pager->pages[page_num] = NULL;
     }
  }

  int result = close(pager->file_descriptor);
  if (result == -1) {
     printf("Error closing db file.\n");
     exit(EXIT_FAILURE);
  }
  for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
     void* page = pager->pages[i];
     if (page) {
         free(page);
         pager->pages[i] = NULL;
     }
  }

  free(pager);
  free(table);
}


MetaCommandResult do_meta_command(InputBuffer* input_buffer, Table *table) {
    if (input_buffer->buffer == ".exit") {
        close_input_buffer(input_buffer);
        db_close(table);
        exit(EXIT_SUCCESS);
    } else {
        return META_COMMAND_UNRECOGNIZED_COMMAND;
    }
}

PrepareResult prepare_insert(InputBuffer* input_buffer, Statement* statement) {
    statement->type = STATEMENT_INSERT;
        
    istringstream iss(input_buffer->buffer);
    string instruction;
    iss >> instruction >> statement->row_to_insert.id >> statement->row_to_insert.username >> statement->row_to_insert.email;

    if (statement->row_to_insert.username.empty() || statement->row_to_insert.email.empty()) {  
        return PREPARE_SYNTAX_ERROR;
    } else if (statement->row_to_insert.id < 0) {
        return PREPARE_NEGATIVE_ID;
    } else if (statement->row_to_insert.username.length() > COLUMN_USERNAME_SIZE || statement->row_to_insert.username.length() > COLUMN_EMAIL_SIZE) { 
        return PREPARE_STRING_TOO_LONG;
    }

    return PREPARE_SUCCESS;
}

PrepareResult prepare_statement(InputBuffer* input_buffer, Statement* statement) {
    if (input_buffer->buffer.compare(0, 6, "insert") == 0) {
        return prepare_insert(input_buffer, statement);
    } 
    
    if (input_buffer->buffer == "select") {
        statement->type = STATEMENT_SELECT;
        return PREPARE_SUCCESS;
    } 
    
    return PREPARE_UNRECOGNIZED_STATEMENT;
}

ExecuteResult execute_insert (Statement* statement, Table* table) {
    if (table->num_rows >= TABLE_MAX_ROWS) {
        return EXECUTE_TABLE_FULL;
    }

    Row* row_to_insert = &(statement->row_to_insert);
    Cursor* cursor = table_end(table);

    serialize_row(row_to_insert, cursor_value(cursor));
    table->num_rows += 1;

    free(cursor);

    return EXECUTE_SUCCESS;
}

ExecuteResult execute_select(Statement* statement, Table* table) {
    Cursor* cursor = table_start(table);
    
    Row row;
    while (!(cursor->end_of_table)) {
        deserialize_row(cursor_value(cursor), &row);
        print_row(&row);
        cursor_advance(cursor);
    }

    free(cursor);

    return EXECUTE_SUCCESS;
}

ExecuteResult execute_statement(Statement* statement, Table *table) {
    switch (statement->type) {
        case (STATEMENT_INSERT):
       	    return execute_insert(statement, table);
        case (STATEMENT_SELECT):
	        return execute_select(statement, table);
  }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
      printf("Must supply a database filename.\n");
      exit(EXIT_FAILURE);
    }

    char* filename = argv[1];
    Table* table = db_open(filename);


    InputBuffer* input_buffer = new_input_buffer();
    while (true) {
        read_input(input_buffer);
        
        if (input_buffer->buffer[0] == '.') {
            switch (do_meta_command(input_buffer, table)) {
                case (META_COMMAND_SUCCESS):
                    continue;
                case (META_COMMAND_UNRECOGNIZED_COMMAND):
                    cout << "Unrecognized command " << input_buffer->buffer << endl;
                    continue;
            }
        }

        Statement statement; 
        switch (prepare_statement(input_buffer, &statement)) {
            case (PREPARE_SUCCESS):
                break;
            case (PREPARE_NEGATIVE_ID):
                cout << "ID must be positive." << endl;
                continue;
            case (PREPARE_STRING_TOO_LONG):
                cout << "String is too long." << endl;
                continue;
            case (PREPARE_SYNTAX_ERROR):
                cout << "Syntax error. Could not parse statement." << endl;
                continue;
            case (PREPARE_UNRECOGNIZED_STATEMENT):
                cout << "Unrecognized keyword at start of " << input_buffer->buffer << "." << endl;
                continue;
        }

        switch (execute_statement(&statement, table)) {
            case (EXECUTE_SUCCESS):
                cout << "Executed." << endl;
	            break;
            case (EXECUTE_TABLE_FULL):
                cout << "Error: Table full." << endl;
                break;
        }  
    }
}
