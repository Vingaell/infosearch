#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_SUFFIX 256
#define MAX_SUFFIXES 512
#define MAX_WORD 512
#define MAX_LINE 65536
#define MAX_WORDS 1024
#define MAX_TEXT 65536
#define MAX_PATH 1024

struct Stemmer {
    char suffixes[MAX_SUFFIXES][MAX_SUFFIX];
    int suffix_count;
    char exceptions[MAX_SUFFIXES][MAX_SUFFIX];
    int exc_count;
    int min_stem_length;

    void init() {
        const char *suffs[] = {
            "иями","ями","ями","ами","ями","ами","иями","иями","иями","ствами","енность","остью",
            "овского","евского","ического","ального","ического","ового","евого","ивого","авого","явого",
            "овой","евой","ивой","авой","явой","аться","яться","иться","ыться","оться","уются","юются",
            "аются","яются","иются","овать","евать","ывать","ивать","авать","авший","явший","ивший",
            "ывший","евший","аемая","яемая","имая","ымая","омая","анный","янный","инный","ынный",
            "онный","ами","ями","иям","ям","ах","ях","ого","его","ому","ему","ими","ыми","ее","ое",
            "ие","ые","ая","яя","ий","ой","ый","ость","ост","ение","ание","ение","ание","ия","иям",
            "иях","ие","ия","ью","ть","ти","тся","ться","ешь","ет","ем","ете","ут","ют","ат","ят",
            "ал","ял","ил","ыл","ол","ала","яла","ила","ыла","ола","али","яли","или","ыли","оли",
            "ого","его","ому","ему","ом","ем","ым","им","ую","юю","ую","юю","ах","ях","е","и",
            "а","я","у","ю","ы","и","е","о","ь"
        };
        suffix_count = sizeof(suffs)/sizeof(suffs[0]);
        for (int i = 0; i < suffix_count; i++) strcpy(suffixes[i], suffs[i]);

        const char *excs[] = {
            "и","в","на","с","к","у","по","за","из","от","до","без","над","под","о","об","при","про",
            "для","а","но","да","или","либо","то","не","ни","бы","же","ли","ведь","вот","вон","это",
            "тот","эта","этот","такой","как","так","очень","еще","уже"
        };
        exc_count = sizeof(excs)/sizeof(excs[0]);
        for (int i = 0; i < exc_count; i++) strcpy(exceptions[i], excs[i]);

        min_stem_length = 2;
    }

    void to_lower(char *s) {
        for (int i = 0; s[i]; i++) s[i] = tolower((unsigned char)s[i]);
    }

    int is_exception(const char *word) {
        for (int i = 0; i < exc_count; i++)
            if (strcmp(word, exceptions[i]) == 0) return 1;
        return 0;
    }

    void stem(char *word) {
        to_lower(word);
        if (is_exception(word) || strlen(word) <= min_stem_length) return;
        for (int i = 0; i < suffix_count; i++) {
            int len_s = strlen(suffixes[i]);
            int len_w = strlen(word);
            if (len_w > len_s && strcmp(word + len_w - len_s, suffixes[i]) == 0) {
                if (len_w - len_s >= min_stem_length) word[len_w - len_s] = 0;
                break;
            }
        }
    }

    void stem_text(const char *text, char *out) {
        out[0] = 0;
        int i = 0, j = 0;
        char word[MAX_WORD];
        while (text[i]) {
            if ((text[i] >= 'a' && text[i] <= 'z') || (text[i] >= '0' && text[i] <= '9') ||
                (text[i] >= (char)0xC0 && text[i] <= (char)0xFF) || (text[i] >= (char)0xE0 && text[i] <= (char)0xFF)) {
                int k = 0;
                while (text[i] && ((text[i] >= 'a' && text[i] <= 'z') || (text[i] >= '0' && text[i] <= '9') ||
                    (text[i] >= (char)0xC0 && text[i] <= (char)0xFF))) word[k++] = text[i++];
                word[k] = 0;
                stem(word);
                if (out[0]) strcat(out, " ");
                strcat(out, word);
            } else i++;
        }
    }
};

int main() {
    Stemmer s;
    s.init();

    const char *test_words[] = {
        "книга","книги","книгу","книгой","книгами","книжный","читать","читает","читали",
        "читающий","читавший","стол","стола","столу","столом","столах","интересный",
        "интересная","интересное","интересные","работа","работы","работе","работой",
        "работать","работает","большой","большая","большое","большие","новый","новая",
        "новое","новые","новость","новости"
    };

    printf("============================================================\n");
    printf("ТЕСТИРОВАНИЕ РУЧНОГО СТЕММЕРА\n");
    printf("============================================================\n");

    for (int i = 0; i < sizeof(test_words)/sizeof(test_words[0]); i++) {
        char w[MAX_WORD];
        strcpy(w, test_words[i]);
        s.stem(w);
        printf("  %-15s -> %s\n", test_words[i], w);
    }

    printf("\n📏 ТЕСТ КОРОТКИХ СЛОВ:\n");
    const char *short_words[] = {"и","в","на","у","он","она","оно"};
    for (int i = 0; i < sizeof(short_words)/sizeof(short_words[0]); i++) {
        char w[MAX_WORD];
        strcpy(w, short_words[i]);
        s.stem(w);
        printf("  %-5s -> %s\n", short_words[i], w);
    }

    printf("\nТЕСТ ЦЕЛОГО ТЕКСТА:\n");
    const char *text = "Я читаю интересную книгу о великих писателях и их произведениях";
    char stemmed_text[MAX_TEXT];
    s.stem_text(text, stemmed_text);
    printf("  Было: %s\n", text);
    printf("  Стало: %s\n", stemmed_text);

    const char *input_dir = "../literature_corpus";
    const char *output_dir = "../literature_corpus_stemmed";

    struct stat st;
    if (stat(input_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        printf("Папка %s не найдена, пропускаем обработку файлов\n", input_dir);
        return 0;
    }

    mkdir(output_dir, 0755);

    DIR *dir = opendir(input_dir);
    struct dirent *entry;
    char path[MAX_PATH], outpath[MAX_PATH];
    int total_docs = 0;

    while ((entry = readdir(dir))) {
        if (strncmp(entry->d_name, "part_", 5) == 0) {
            snprintf(path, MAX_PATH, "%s/%s", input_dir, entry->d_name);
            snprintf(outpath, MAX_PATH, "%s/%s", output_dir, entry->d_name);
            FILE *fin = fopen(path, "r");
            FILE *fout = fopen(outpath, "w");
            if (!fin || !fout) continue;
            char line[MAX_LINE];
            fgets(line, MAX_LINE, fin);
            fputs(line, fout);
            while (fgets(line, MAX_LINE, fin)) {
                char *id = strtok(line, "\t");
                char *title = strtok(NULL, "\t");
                char *text = strtok(NULL, "\t");
                if (id && title && text) {
                    char stemmed_title[MAX_TEXT], stemmed_body[MAX_TEXT];
                    s.stem_text(title, stemmed_title);
                    s.stem_text(text, stemmed_body);
                    fprintf(fout, "%s\t%s\t%s\n", id, stemmed_title, stemmed_body);
                    total_docs++;
                }
            }
            fclose(fin);
            fclose(fout);
        }
    }
    closedir(dir);

    printf("\nГотово! Обработано документов: %d\n", total_docs);
    printf("Результат в папке: %s\n", output_dir);

    return 0;
}