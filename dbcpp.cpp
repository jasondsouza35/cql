#include <iostream>
#include <string>
using namespace std;

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

int main() {
    InputBuffer* input_buffer = new_input_buffer();
    while (true) {
        read_input(input_buffer);
        if (input_buffer->buffer == ".exit") {
            close_input_buffer(input_buffer);
            exit(EXIT_SUCCESS);
        } else {
            cout << "Unrecognized command '" << input_buffer->buffer << "'.\n";
        }
    }
}
