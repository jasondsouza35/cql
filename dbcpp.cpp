#include <iostream>
#include <string>
using namespace std;

enum MetaCommandResult {
    META_COMMAND_SUCCESS,
    META_COMMAND_UNRECOGNIZED_COMMAND
};

enum PrepareResult {
    PREPARE_SUCCESS, 
    PREPARE_UNRECOGNIZED_STATEMENT
};

enum StatementType {
    STATEMENT_INSERT, 
    STATEMENT_SELECT
};

struct Statement {
    StatementType type;
};

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
    delete(input_buffer);
}

MetaCommandResult do_meta_command(InputBuffer* input_buffer) {
    if (input_buffer->buffer == ".exit") {
        close_input_buffer(input_buffer);
        exit(EXIT_SUCCESS);
    } else {
        return META_COMMAND_UNRECOGNIZED_COMMAND;
    }
}

PrepareResult prepare_statment(InputBuffer* input_buffer, Statement* statement) {
    if (input_buffer->buffer == "insert") {
        statement->type = STATEMENT_INSERT;
        return PREPARE_SUCCESS;
    } else if (input_buffer->buffer == "select") {
        statement->type = STATEMENT_SELECT;
        return PREPARE_SUCCESS;
    } else {
        return PREPARE_UNRECOGNIZED_STATEMENT;
    }
}

void execute_statement(Statement* statement) {
    switch (statement->type) {
        case (STATEMENT_INSERT):
            cout << "This is where we would do an insert." << endl;
            break;
        case (STATEMENT_SELECT):
            cout << "This is where we would do an select." << endl;
            break;
    }
}

int main() {
    InputBuffer* input_buffer = new_input_buffer();
    while (true) {
        read_input(input_buffer);
        
        if (input_buffer->buffer[0] == '.') {
            switch (do_meta_command(input_buffer)) {
                case (META_COMMAND_SUCCESS):
                    continue;
                case (META_COMMAND_UNRECOGNIZED_COMMAND):
                    cout << "Unrecognized command " << input_buffer->buffer << endl;
                    continue;
            }
        }

        Statement statement; 
        switch (prepare_statment(input_buffer, &statement)) {
            case (PREPARE_SUCCESS):
                break;
            case (PREPARE_UNRECOGNIZED_STATEMENT):
                cout << "Unrecognized keyword at start of " << input_buffer->buffer << "." << endl;
                continue;
        }

        execute_statement(&statement);
        cout << "Executed." << endl;
    }  
}
