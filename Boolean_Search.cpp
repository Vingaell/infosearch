#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <dirent.h>
#include <ctime>

#define MAX_WORD 256
#define MAX_LINE 65536
#define MAX_DOCS 200000
#define MAX_INDEX 200000
#define MAX_TOKENS 512

struct DocList {
    char word[MAX_WORD];
    char **docs;
    int count;
    int capacity;
};

struct Title {
    char id[64];
    char title[512];
};

DocList *index_data = NULL;
int index_size = 0;

Title *titles = NULL;
int title_count = 0;

FILE *log_file;

void to_lower(char *s) {
    for (int i = 0; s[i]; i++)
        s[i] = tolower((unsigned char)s[i]);
}

int find_word(const char *word) {
    for (int i = 0; i < index_size; i++)
        if (strcmp(index_data[i].word, word) == 0)
            return i;
    return -1;
}

int find_title(const char *id) {
    for (int i = 0; i < title_count; i++)
        if (strcmp(titles[i].id, id) == 0)
            return i;
    return -1;
}

void load_index(const char *filename) {
    printf("Загрузка индекса из %s...\n", filename);
    FILE *f = fopen(filename, "r");
    if (!f) exit(1);

    index_data = (DocList*)malloc(sizeof(DocList) * MAX_INDEX);
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (strchr(line, '[')) {
            char *w1 = strchr(line, '"') + 1;
            char *w2 = strchr(w1, '"');
            *w2 = 0;

            strcpy(index_data[index_size].word, w1);
            index_data[index_size].docs = (char**)malloc(sizeof(char*) * MAX_DOCS);
            index_data[index_size].count = 0;

            char *p = strchr(w2 + 1, '[') + 1;
            while (p && *p && *p != ']') {
                char *d1 = strchr(p, '"');
                if (!d1) break;
                d1++;
                char *d2 = strchr(d1, '"');
                if (!d2) break;
                *d2 = 0;
                index_data[index_size].docs[index_data[index_size].count++] = strdup(d1);
                p = d2 + 1;
            }
            index_size++;
        }
    }
    fclose(f);
    printf("  Загружено %d уникальных слов\n", index_size);
}

void load_titles(const char *corpus_dir) {
    printf("Загрузка названий документов из %s...\n", corpus_dir);
    titles = (Title*)malloc(sizeof(Title) * MAX_DOCS);
    DIR *dir = opendir(corpus_dir);
    struct dirent *entry;
    char path[1024];

    while ((entry = readdir(dir))) {
        if (strncmp(entry->d_name, "part_", 5) == 0) {
            sprintf(path, "%s/%s", corpus_dir, entry->d_name);
            FILE *f = fopen(path, "r");
            if (!f) continue;
            char line[MAX_LINE];
            fgets(line, sizeof(line), f);
            while (fgets(line, sizeof(line), f)) {
                char *id = strtok(line, "\t");
                char *title = strtok(NULL, "\t");
                if (id && title) {
                    strcpy(titles[title_count].id, id);
                    strcpy(titles[title_count].title, title);
                    title_count++;
                }
            }
            fclose(f);
        }
    }
    closedir(dir);
    printf("  Загружено %d документов\n", title_count);
}

void tokenize_query(const char *query, char tokens[MAX_TOKENS][MAX_WORD], int *count) {
    char buffer[1024];
    strcpy(buffer, query);
    to_lower(buffer);
    *count = 0;
    int i = 0;
    while (buffer[i]) {
        if (isspace(buffer[i])) { i++; continue; }
        if (buffer[i] == '(' || buffer[i] == ')') {
            tokens[*count][0] = buffer[i];
            tokens[*count][1] = 0;
            (*count)++;
            i++;
            continue;
        }
        int j = 0;
        while (isalnum(buffer[i])) {
            tokens[*count][j++] = buffer[i++];
        }
        tokens[*count][j] = 0;
        (*count)++;
    }
}

void set_copy(char **src, int sc, char **dst, int *dc) {
    *dc = sc;
    for (int i = 0; i < sc; i++)
        dst[i] = src[i];
}

void set_and(char **a, int ac, char **b, int bc, char **res, int *rc) {
    *rc = 0;
    for (int i = 0; i < ac; i++)
        for (int j = 0; j < bc; j++)
            if (strcmp(a[i], b[j]) == 0)
                res[(*rc)++] = a[i];
}

void set_or(char **a, int ac, char **b, int bc, char **res, int *rc) {
    *rc = 0;
    for (int i = 0; i < ac; i++)
        res[(*rc)++] = a[i];
    for (int j = 0; j < bc; j++) {
        int found = 0;
        for (int i = 0; i < ac; i++)
            if (strcmp(b[j], a[i]) == 0) found = 1;
        if (!found) res[(*rc)++] = b[j];
    }
}

void set_not(char **a, int ac, char **b, int bc, char **res, int *rc) {
    *rc = 0;
    for (int i = 0; i < ac; i++) {
        int found = 0;
        for (int j = 0; j < bc; j++)
            if (strcmp(a[i], b[j]) == 0) found = 1;
        if (!found) res[(*rc)++] = a[i];
    }
}

void evaluate(char tokens[MAX_TOKENS][MAX_WORD], int *pos, int count, char **res, int *rc);

void parse_expression(char tokens[MAX_TOKENS][MAX_WORD], int *pos, int count, char **res, int *rc) {
    char *temp1[MAX_DOCS], *temp2[MAX_DOCS];
    int c1 = 0, c2 = 0;
    char op[8] = "";

    while (*pos < count) {
        char *token = tokens[*pos];

        if (strcmp(token, "(") == 0) {
            (*pos)++;
            evaluate(tokens, pos, count, temp2, &c2);
        } else if (strcmp(token, ")") == 0) {
            (*pos)++;
            break;
        } else if (!strcmp(token, "and") || !strcmp(token, "or") || !strcmp(token, "not")) {
            strcpy(op, token);
            (*pos)++;
            continue;
        } else {
            int idx = find_word(token);
            c2 = 0;
            if (idx != -1)
                set_copy(index_data[idx].docs, index_data[idx].count, temp2, &c2);
            (*pos)++;
        }

        if (c1 == 0) {
            set_copy(temp2, c2, temp1, &c1);
        } else {
            if (!strcmp(op, "and"))
                set_and(temp1, c1, temp2, c2, temp1, &c1);
            else if (!strcmp(op, "or"))
                set_or(temp1, c1, temp2, c2, temp1, &c1);
            else if (!strcmp(op, "not"))
                set_not(temp1, c1, temp2, c2, temp1, &c1);
        }
    }

    set_copy(temp1, c1, res, rc);
}

void evaluate(char tokens[MAX_TOKENS][MAX_WORD], int *pos, int count, char **res, int *rc) {
    parse_expression(tokens, pos, count, res, rc);
}

void save_results(const char *query, char **res, int rc) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char timebuf[64];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm);

    fprintf(log_file, "\n================================================================================\n");
    fprintf(log_file, "Время: %s\n", timebuf);
    fprintf(log_file, "Запрос: %s\n", query);
    fprintf(log_file, "Найдено документов: %d\n", rc);
    fprintf(log_file, "--------------------------------------------------------------------------------\n");

    for (int i = 0; i < rc; i++) {
        int t_idx = find_title(res[i]);
        if (t_idx != -1)
            fprintf(log_file, "%d. %s | %s\n", i + 1, titles[t_idx].id, titles[t_idx].title);
    }

    fprintf(log_file, "================================================================================\n");
    fflush(log_file);
}

int main() {
    printf("============================================================\n");
    printf("БУЛЕВ ПОИСК\n");
    printf("============================================================\n");

    load_index("boolean_index_stemmed.json");
    load_titles("../literature_corpus_stemmed");

    log_file = fopen("search_results.txt", "a");

    printf("\n============================================================\n");
    printf("Доступные операторы:\n");
    printf("  AND - пересечение (по умолчанию)\n");
    printf("  OR  - объединение\n");
    printf("  NOT - исключение\n");
    printf("  ()  - группировка\n");
    printf("============================================================\n");

    char query[1024];

    while (1) {
        printf("\nВведите запрос (или 'exit' для выхода): ");
        fgets(query, sizeof(query), stdin);
        query[strcspn(query, "\n")] = 0;

        char tmp[1024];
        strcpy(tmp, query);
        to_lower(tmp);
        if (!strcmp(tmp, "exit")) break;
        if (strlen(query) == 0) continue;

        printf("\n🔍 Запрос: %s\n", query);

        char tokens[MAX_TOKENS][MAX_WORD];
        int token_count;
        tokenize_query(query, tokens, &token_count);

        printf("  Токены: [");
        for (int i = 0; i < token_count; i++) {
            printf("%s%s", tokens[i], i < token_count - 1 ? ", " : "");
        }
        printf("]\n");

        int pos = 0;
        char *results[MAX_DOCS];
        int rc = 0;

        evaluate(tokens, &pos, token_count, results, &rc);

        printf("\n📊 Найдено документов: %d\n", rc);

        int show = rc < 20 ? rc : 20;
        if (show > 0) {
            printf("\n📋 Первые %d результатов:\n", show);
            for (int i = 0; i < show; i++) {
                int t_idx = find_title(results[i]);
                if (t_idx != -1)
                    printf("  %d. %s\n", i + 1, titles[t_idx].title);
            }
            if (rc > show)
                printf("  ... и ещё %d\n", rc - show);
        }

        save_results(query, results, rc);
        printf("\n💾 Результаты сохранены в search_results.txt\n");
    }

    fclose(log_file);

    printf("\n✅ Все результаты сохранены в search_results.txt\n");
    return 0;
}