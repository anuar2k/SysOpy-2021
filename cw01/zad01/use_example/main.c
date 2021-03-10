#include <rrmerge.h>

int main() {
    v_file_pair file_pairs;
    vec_init(&file_pairs);

    add_file_pair(&file_pairs, "file1.txt:file2.txt");
    add_file_pair(&file_pairs, "file3.txt:file4.txt");
    print_file_pairs(&file_pairs);

    v_FILE tmp_files;
    vec_init(&tmp_files);

    merge_file_pairs(&tmp_files, &file_pairs);
    free_file_pairs(&file_pairs);

    for (int i = 0; i < tmp_files.size; i++) {
        printf("File %d:\n", i);

        FILE *file = tmp_files.storage[i];
        int read;
        while ((read = getc(file)) != EOF) {
            putc(read, stdout);
        }

        rewind(file);
    }

    v_v_char main;
    vec_init(&main);

    for (int i = 0; i < tmp_files.size; i++) {
        printf("Added block: %lu\n", add_row_block(&main, tmp_files.storage[i]));
    }

    remove_row_block(&main, 0);
    remove_row(&main, 0, 1);
    remove_row(&main, 0, 2);

    printf("size: %lu\n", main.size);
    
    for (int i = 0; i < main.size; i++) {
        v_char *row_block = main.storage[i];
        for (int j = 0; j < row_block->size; j++) {
            char *line = row_block->storage[j];
            printf("From block: %s", line);
        }
    }

    free_tmp_files(&tmp_files);
    free_main(&main);
}
