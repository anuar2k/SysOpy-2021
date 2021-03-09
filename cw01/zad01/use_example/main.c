#include <rrmerge.h>
#include <ptr_vector.h>

int main() {
    ptr_vector file_pairs_vector;
    vec_init(&file_pairs_vector);

    add_file_pair(&file_pairs_vector, "file1.txt:file2.txt");
    add_file_pair(&file_pairs_vector, "file3.txt:file4.txt");
    print_file_pairs(&file_pairs_vector);

    ptr_vector tmp_files_vector;
    vec_init(&tmp_files_vector);

    merge_file_pairs(&tmp_files_vector, &file_pairs_vector);
    free_file_pairs(&file_pairs_vector);

    for (int i = 0; i < tmp_files_vector.size; i++) {
        printf("File %d:\n", i);

        FILE *file = tmp_files_vector.storage[i];
        int read;
        while ((read = getc(file)) != EOF) {
            putc(read, stdout);
        }

        rewind(file);
    }

    ptr_vector main_vector;
    vec_init(&main_vector);

    for (int i = 0; i < tmp_files_vector.size; i++) {
        printf("Added block: %lu\n", add_row_block(&main_vector, tmp_files_vector.storage[i]));
    }

    remove_row_block(&main_vector, 0);
    remove_row(&main_vector, 0, 1);
    remove_row(&main_vector, 0, 2);

    printf("size: %lu\n", main_vector.size);
    
    for (int i = 0; i < main_vector.size; i++) {
        ptr_vector *row_block = main_vector.storage[i];
        for (int j = 0; j < row_block->size; j++) {
            char *line = row_block->storage[j];
            printf("From block: %s", line);
        }
    }

    free_tmp_files(&tmp_files_vector);
    vec_clear(&tmp_files_vector);
    free_main_vector(&main_vector);
}
