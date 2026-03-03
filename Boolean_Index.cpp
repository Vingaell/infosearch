#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_WORD 256
#define MAX_DOC 128
#define MAX_LINE 65536
#define INITIAL_CAPACITY 100000

struct DocList {
    char word[MAX_WORD];
    char **docs;
    int doc_count;
    int doc_capacity;
};

DocList *index_data = NULL;
int index_size = 0;
int index_capacity = 0;

int compare_strings(const char *a, const char *b) {
    return strcmp(a, b);
}

int starts_with(const char *str, const char *prefix) {
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

int ends_with(const char *str, const char *suffix) {
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix > lenstr) return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

void ensure_index_capacity() {
    if (index_size >= index_capacity) {
        index_capacity = index_capacity == 0 ? INITIAL_CAPACITY : index_capacity * 2;
        index_data = (DocList*)realloc(index_data, sizeof(DocList) * index_capacity);
    }
}

int find_word(const char *word) {
    for (int i = 0; i < index_size; i++) {
        if (strcmp(index_data[i].word, word) == 0)
            return i;
    }
    return -1;
}

void add_doc_to_word(int idx, const char *doc_id) {
    for (int i = 0; i < index_data[idx].doc_count; i++) {
        if (strcmp(index_data[idx].docs[i], doc_id) == 0)
            return;
    }
    if (index_data[idx].doc_count >= index_data[idx].doc_capacity) {
        index_data[idx].doc_capacity = index_data[idx].doc_capacity == 0 ? 8 : index_data[idx].doc_capacity * 2;
        index_data[idx].docs = (char**)realloc(index_data[idx].docs, sizeof(char*) * index_data[idx].doc_capacity);
    }
    index_data[idx].docs[index_data[idx].doc_count] = strdup(doc_id);
    index_data[idx].doc_count++;
}

void add_entry(const char *word, const char *doc_id) {
    if (strlen(word) == 0) return;
    int idx = find_word(word);
    if (idx == -1) {
        ensure_index_capacity();
        idx = index_size++;
        strcpy(index_data[idx].word, word);
        index_data[idx].docs = NULL;
        index_data[idx].doc_count = 0;
        index_data[idx].doc_capacity = 0;
    }
    add_doc_to_word(idx, doc_id);
}

int compare_files(const void *a, const void *b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

int build_index(const char *corpus_dir, const char *output_file) {
    DIR *dir = opendir(corpus_dir);
    if (!dir) {
        printf("❌ Папка %s не найдена\n", corpus_dir);
        return 0;
    }

    printf("============================================================\n");
    printf("ПОСТРОЕНИЕ БУЛЕВА ИНДЕКСА\n");
    printf("============================================================\n");

    char **files = NULL;
    int file_count = 0;
    int file_capacity = 16;
    files = (char**)malloc(sizeof(char*) * file_capacity);

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (starts_with(entry->d_name, "part_") && ends_with(entry->d_name, ".tsv")) {
            if (file_count >= file_capacity) {
                file_capacity *= 2;
                files = (char**)realloc(files, sizeof(char*) * file_capacity);
            }
            files[file_count++] = strdup(entry->d_name);
        }
    }
    closedir(dir);

    qsort(files, file_count, sizeof(char*), compare_files);

    printf("📁 Найдено файлов: %d\n", file_count);

    if (file_count == 0) {
        printf("❌ Нет TSV-файлов в папке\n");
        return 0;
    }

    int docs = 0;
    int total_entries = 0;
    clock_t start_time = clock();

    for (int i = 0; i < file_count; i++) {
        printf("\r📊 Обработка файла %d/%d...", i + 1, file_count);
        fflush(stdout);

        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", corpus_dir, files[i]);
        FILE *f = fopen(path, "r");
        if (!f) continue;

        char line[MAX_LINE];
        fgets(line, sizeof(line), f);

        while (fgets(line, sizeof(line), f)) {
            char *doc_id = strtok(line, "\t");
            strtok(NULL, "\t");
            char *text = strtok(NULL, "\t\n");

            if (doc_id && text) {
                docs++;
                char *word = strtok(text, " \n\r");
                while (word) {
                    add_entry(word, doc_id);
                    total_entries++;
                    word = strtok(NULL, " \n\r");
                }
            }
        }
        fclose(f);
    }

    double elapsed = (double)(clock() - start_time) / CLOCKS_PER_SEC;

    FILE *out = fopen(output_file, "w");
    fprintf(out, "{\n");
    for (int i = 0; i < index_size; i++) {
        fprintf(out, "  \"%s\": [", index_data[i].word);
        for (int j = 0; j < index_data[i].doc_count; j++) {
            fprintf(out, "\"%s\"", index_data[i].docs[j]);
            if (j < index_data[i].doc_count - 1) fprintf(out, ", ");
        }
        fprintf(out, "]");
        if (i < index_size - 1) fprintf(out, ",");
        fprintf(out, "\n");
    }
    fprintf(out, "}\n");
    fclose(out);

    printf("\n\n============================================================\n");
    printf("📊 СТАТИСТИКА ИНДЕКСА\n");
    printf("============================================================\n");
    printf("  Файлов обработано: %d\n", file_count);
    printf("  Документов: %d\n", docs);
    printf("  Уникальных слов: %d\n", index_size);
    printf("  Всего записей (слово-документ): %d\n", total_entries);
    if (index_size > 0)
        printf("  Среднее число документов на слово: %.1f\n", (double)total_entries / index_size);
    printf("  Время построения: %.2f сек\n", elapsed);
    printf("\n✅ Индекс сохранён в %s\n", output_file);

    printf("\n📝 ПРИМЕРЫ ИЗ ИНДЕКСА:\n");
    for (int i = 0; i < 5 && i < index_size; i++) {
        printf("  '%s': [", index_data[i].word);
        for (int j = 0; j < index_data[i].doc_count && j < 5; j++) {
            printf("'%s'", index_data[i].docs[j]);
            if (j < index_data[i].doc_count - 1 && j < 4) printf(", ");
        }
        if (index_data[i].doc_count > 5) printf("...");
        printf("]\n");
    }

    return 1;
}

int main() {
    printf("\nВыберите вариант:\n");
    printf("1. Стеммированные тексты (../literature_corpus_stemmed)\n");
    printf("2. Обычные тексты (../literature_corpus)\n");

    char choice[8];
    printf("\nВаш выбор (1/2): ");
    fgets(choice, sizeof(choice), stdin);

    const char *corpus_dir;
    const char *output_file;

    if (choice[0] == '1') {
        corpus_dir = "../literature_corpus_stemmed";
        output_file = "boolean_index_stemmed.json";
    } else {
        corpus_dir = "../literature_corpus";
        output_file = "boolean_index.json";
    }

    if (build_index(corpus_dir, output_file)) {
        printf("\n🔍 Пример работы с индексом:\n");
        char word[MAX_WORD];
        printf("Введите слово для проверки: ");
        fgets(word, sizeof(word), stdin);
        word[strcspn(word, "\n")] = 0;

        int idx = find_word(word);
        if (idx != -1) {
            printf("  Слово '%s' встречается в %d документах\n", word, index_data[idx].doc_count);
            printf("  Первые 5: ");
            for (int i = 0; i < index_data[idx].doc_count && i < 5; i++) {
                printf("%s ", index_data[idx].docs[i]);
            }
            printf("\n");
        } else {
            printf("  Слово '%s' не найдено в индексе\n", word);
        }
    }

    return 0;
}