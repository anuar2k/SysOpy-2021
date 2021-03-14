#include <rrmerge.h>

int main() {
    v_file_pair file_pairs;
    vec_init(&file_pairs);

    add_file_pair(&file_pairs, "./example/file1.txt:./example/file2.txt");
    add_file_pair(&file_pairs, "./example/file3.txt:./example/file4.txt");

    v_FILE tmp_files;
    vec_init(&tmp_files);

    merge_file_pairs(&tmp_files, &file_pairs);
    free_file_pairs(&file_pairs);

    for (size_t i = 0; i < tmp_files.size; i++) {
        printf("File %zu:\n", i);

        FILE *file = tmp_files.storage[i];
        int read;
        while ((read = getc(file)) != EOF) {
            putc(read, stdout);
        }

        rewind(file);
    }

    v_v_char row_blocks;
    vec_init(&row_blocks);

    for (size_t i = 0; i < tmp_files.size; i++) {
        printf("Added block: %zu\n", add_row_block(&row_blocks, tmp_files.storage[i]));
    }

    remove_row_block(&row_blocks, 0);
    remove_row(&row_blocks, 0, 1);
    remove_row(&row_blocks, 0, 2);

    printf("size: %zu\n", row_blocks.size);
    
    print_row_blocks(&row_blocks);

    free_tmp_files(&tmp_files);
    free_row_blocks(&row_blocks);
}
